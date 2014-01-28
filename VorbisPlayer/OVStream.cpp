#include <iostream>
#include <fstream>
#include <string>
using namespace std;

#include <al.h>
#include <alc.h>
#include <AL\alut.h>
#include <ogg\ogg.h>
#include <vorbis\codec.h>
#include <vorbis\vorbisenc.h>
#include <vorbis\vorbisfile.h>
#include <boost\thread.hpp>
#include <boost\date_time.hpp>

#include "OVStream.h"

OVStream::OVStream(int argc, char** argv) {
	this->argc = argc;
	this->argv = argv;
	first = true;
	position = 1;
	openAL();
}

OVStream::~OVStream() {
	stop();
	closeAL();
}

void OVStream::openAL() {
	// Set up OpenAL.
	alGetError(); // Reset the error state.
	device = alcOpenDevice(NULL); // Set up a device
	printALError("alcOpenDevice", true);
	if (device) {
		context = alcCreateContext(device, NULL); // Create a context.
		printALError("alcCreateContext", true);
		alcMakeContextCurrent(context); // Make the context we just created the current context.
		printALError("alcMakeContextCurrent", true);
	}
	alGenSources(1, &source); // Set up a source.
	printALError("alGenSources", true);
	alGenBuffers(2, buffers); // Set up two buffers.
	printALError("alGenBuffers", true);
	cerr << "state: " << state() << endl;
}

void OVStream::openOV(char* path) {
	// Open an Ogg Vorbis file, and get some information from it.
	printOVError("ov_fopen", true, ov_fopen(path, &ovFile)); // Open the Ogg Vorbis file (and print any errors).
	ovInfo = ov_info(&ovFile, -1); // Load the information struct.
	ovComment = ov_comment(&ovFile, -1); // Load the comments struct.
	// Determine the number of channels
	if (ovInfo->channels == 1) {
		format = AL_FORMAT_MONO16;
	} else {
		format = AL_FORMAT_STEREO16;
	}
}

void OVStream::play() { // perhaps merge this and pause() into playPause() to further encapsulate.
	switch (state()) {
		case AL_PAUSED:
			alSourcePlay(source);
			break;
		case AL_STOPPED:
		case AL_INITIAL:
			playThread = boost::thread(&OVStream::playInThread, this);
			break;
	}
}

void OVStream::playInThread() {
	ovRemains = true;
	while (ovRemains) {
		ALuint buffer;
		if (first) {
			openOV(argv[position]);
			//ov_time_seek(&ovFile, 57.0); // test shit: seek to 0:57
			ovRemains = stream(buffers, 2, true);
			alSourcePlay(source);
			printALError("alSourcePlay", true);
			first = false;
			cerr << "position: " << position << endl;
		}
		switch (state()) {
			case AL_PLAYING:
				int processed;
				alGetSourcei(source, AL_BUFFERS_PROCESSED, &processed);
				int queued;
				alGetSourcei(source, AL_BUFFERS_QUEUED, &queued);

				while (processed--) {
					ovRemains = stream(&buffer, 1, true);
					if (!ovRemains) {
						closeOV();
						if (position == (argc - 1)) { // If there are no more files
							// play out until the end.
							while (queued) {
								alGetSourcei(source, AL_BUFFERS_PROCESSED, &processed);
								while (processed--) {
									ovRemains = stream(&buffer, 1, false);
								}
								alGetSourcei(source, AL_BUFFERS_QUEUED, &queued);
							}
							stop();
							position = 1;
							return;
						} else { // There are more files
							openOV(argv[++position]);
							ovRemains = true;
						}
					}
				}

				boost::this_thread::sleep(boost::posix_time::milliseconds(1)); // reduce CPU usage
				continue;
			case AL_STOPPED:
				if (ovRemains) {
					alSourcePlay(source);
					continue;
				} else {
					return;
				}
				break;
			case AL_PAUSED:
				boost::this_thread::sleep(boost::posix_time::milliseconds(1));
				continue;
				break;
			default: 
				return;
		}
	}
}

void OVStream::stop() {
	if (!first) {
		int queued;
		ALuint buffer;

		alGetSourcei(source, AL_BUFFERS_QUEUED, &queued);
		alSourceStop(source);
		ovRemains = false;
		closeOV();

		// Empty out the buffers
		while (queued) {
			stream(&buffer, 1, false);
			alGetSourcei(source, AL_BUFFERS_QUEUED, &queued);
		}
		first = true;
		playThread.join();
	}
}

void OVStream::pause() {
	alSourcePause(source);
}

void OVStream::next() {
	if (position == (argc - 1)) {
		stop();
		position = 1;
	} else {
		switch (state()) {
			case AL_PLAYING:
				stop();
				position++;
				ovRemains = true;
				play();
				break;
			case AL_PAUSED:
				stop();
				position++;
				ovRemains = true;
				play();
				while (state() != AL_PLAYING) { }
				pause();
				break;
			case AL_INITIAL:
			case AL_STOPPED:
				position++;
				break;
		}
	}
}

void OVStream::back() {
	int thisTime = time;
	if (position == 1) {
		switch (state()) {
			case AL_PLAYING:
				stop();
				if (thisTime >= 3) {
					play();
				}
				break;
			case AL_PAUSED:
				stop();
				if (thisTime >= 3) {
					play();
					while (state() != AL_PLAYING) { }
					pause();
				}
				break;
				break;
			case AL_INITIAL:
			case AL_STOPPED:
				break;
		}
		/*stop();
		if (time >= 3) {
			play();
		}*/
	} else {
		switch (state()) {
			case AL_PLAYING:
				stop();
				if (thisTime < 3) {
					position--;
				}
				ovRemains = true;
				play();
				break;
			case AL_PAUSED:
				stop();
				if (thisTime < 3) {
					position--;
				}
				ovRemains = true;
				play();
				while (state() != AL_PLAYING) { }
				pause();
				break;
			case AL_INITIAL:
			case AL_STOPPED:
				position--;
				break;
		}
	}
}

void OVStream::seek(int seconds) {
	ov_time_seek(&ovFile, (double)seconds);
}

string OVStream::niceTime() {
	int minutes = time / 60;
	int seconds = time % 60;
	ostringstream niceTimeOS;
	niceTimeOS << minutes << ":" << setfill('0') << setw(2) << seconds;
	string niceTimeS = niceTimeOS.str();
	return niceTimeS;
}

string OVStream::niceTimeRemaining() {
	int timeRemaining = ov_time_total(&ovFile, -1) - time;
	int minutes = timeRemaining / 60;
	int seconds = timeRemaining % 60;
	ostringstream niceTimeRemainingOS;
	niceTimeRemainingOS << "-" << minutes << ":" << setfill('0') << setw(2) << seconds;
	string niceTimeRemainingS = niceTimeRemainingOS.str();
	return niceTimeRemainingS;
}

bool OVStream::stream(ALuint* buffers, int bufferCount, bool fill) {
	alGetError();
	if (!first) {
		alSourceUnqueueBuffers(source, bufferCount, buffers);
		printALError("alSourceUnqueueBuffers", true);
	}
	time = (int)ov_time_tell(&ovFile);
	cerr << "\rtime: " << niceTime() << " / " << niceTimeRemaining(); // eventually move this out into the GUI thread
	bool eof = false;
	if (fill) {
		for (int i = 0; i < bufferCount; i++) {
			long justRead; // How many bytes we just read.
			char pcmBuffer[bufferSize]; // Temporary storage for decoded PCM.
			int size = 0; // How many bytes we've read so far for this buffer.
			int section; // Some Ogg Vorbis bullshit that I don't understand, but seems necessary.
			
			while (size < bufferSize) // While our buffer still has room
			{
				justRead = ov_read(&ovFile, pcmBuffer + size, bufferSize - size, 0, 2, 1, &section); // Load some decoded PCM data into pcmBuffer.
				if (justRead > 0) {
					size += justRead;
				} else if (justRead == 0) { // If we've reached EOF
					cerr << "EOF from Ogg Vorbis. Done, son.\n";
					eof = true;
					break; // Break out of "while (size < bufferSize)" to avoid an infinite loop of ov_read errors.
				} else { // justRead < 0, indicating an error.
					printOVError("ov_read", true, justRead);
					eof = true;
					break;
				}
			}

			/* // Apply ReplayGain
			for (int j = 0; j < bufferSize; j++) {
				pcmBuffer[bufferSize] *= currentGain;
			} */

			alBufferData(buffers[i], format, pcmBuffer, size, ovInfo->rate); // Buffer the PCM data into an OpenAL buffer.
			printALError("alBufferData", true);
		}
		alSourceQueueBuffers(source, bufferCount, buffers); // Queue the buffer.
		printALError("alSourceQueueBuffers", true);
			
		if (eof) {
			return false;
		}
		return true;
	} else {
		return false;
	}
}

ALint OVStream::state() {
	ALint state;
    alGetSourcei(source, AL_SOURCE_STATE, &state);
    return state;
}

bool OVStream::getOVRemains() {
	return ovRemains;
}

void OVStream::closeOV() {
	// Close the Ogg Vorbis file.
	printOVError("ov_clear", 1, ov_clear(&ovFile));
	fprintf(stderr, "\n");
}

void OVStream::closeAL() {
	// Dismantle the OpenAL source, buffers, context, and device.
	cerr << "CloseAL called";
	alDeleteSources(1, &source);
    printALError("alDeleteSources", true);
	alDeleteBuffers(1, buffers);
    printALError("alDeleteBuffers", true);
	context = alcGetCurrentContext();
	printALError("alcGetCurrentContext", true);
	device = alcGetContextsDevice(context);
	printALError("alcGetContextsDevice", true);
	alcMakeContextCurrent(NULL);
	printALError("alcMakeContextCurrent", true);
	alcDestroyContext(context);
	printALError("alcDestroyContext", true);
	alcCloseDevice(device);
	printALError("alcCloseDevice", true);
}

string OVStream::niceALGetError() {
	ALenum error = alGetError();
	switch (error) {
		case AL_NO_ERROR:
			return "No error.";
			break;
		case AL_INVALID_NAME:
			return "Invalid name was passed.";
			break;
		case AL_INVALID_ENUM:
			return "Invalid enumeration was passed.";
			break;
		case AL_INVALID_VALUE:
			return "Invalid value was passed.";
			break;
		case AL_INVALID_OPERATION:
			return "Invalid operation was requested.";
			break;
		case AL_OUT_OF_MEMORY:
			return "OpenAL ran out of memory.";
			break;
		default:
			return "Unknown error.";
			break;
	}
}

void OVStream::printALError(char* function, bool newLine) {
	string error = niceALGetError();
	if (error.compare("No error.")) {
		if (newLine) {
			cerr << function << ": " << error << endl;
		} else {
			cerr << function << ": " << error;
		}
	}
}

string OVStream::niceOVError(int error) {
	switch (error) {
		case 0:
			return "No error.";
			break;
		case OV_EREAD:
			return "Error reading file.";
			break;
		case OV_ENOTVORBIS:
			return "The file doesn't contain Vorbis data.";
			break;
		case OV_EVERSION:
			return "Vorbis version mismatch.";
			break;
		case OV_EBADHEADER:
			return "Invalid Vorbis header.";
			break;
		case OV_EFAULT:
			return "Internal logic fault.";
			break;
		case OV_HOLE:
			return "Data interrupted.";
			break;
		case OV_EBADLINK:
			return "Invalid stream section, or link is corrupt.";
			break;
		case OV_EINVAL:
			return "Initial file headers corrupt.";
			break;
		default:
			return "Unknown error.";
			break;
	}
}

void OVStream::printOVError(char* function, bool newLine, int error) {
	string errorString = niceOVError(error);
	if (errorString.compare("No error.")) {
		if (newLine) {
			cerr << function << ": " << errorString << endl;
		} else {
			cerr << function << ": " << errorString;
		}
	}
}

// 