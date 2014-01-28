#pragma once
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

class OVStream {
	public:
		OVStream(int argc, char** argv);
		~OVStream();
		void openAL();
		void openOV(char* path);
		void play();
		bool playInLoop();
		void stop();
		void pause();
		void back();
		void next();
		bool getOVRemains();
		void closeOV();
		void closeAL();
		ALenum state();
		bool stream(ALuint* buffers, int bufferCount, bool fill);
		void seek(int seconds);
		string niceTime();
		string niceTimeRemaining();
	protected:
		string niceALGetError();
		void printALError(char* function, bool newLine);
		string niceOVError(int error);
		void printOVError(char* function, bool newLine, int error);
		void playInThread();

		int position;
		int time;
		int argc;
		char** argv;
		ALCdevice* device;
		ALCcontext* context;
		ALuint buffers[2];
		ALuint source;
		ALenum format;
		OggVorbis_File ovFile;
		vorbis_info* ovInfo;
		vorbis_comment* ovComment;
		bool first;
		bool ovRemains;
		static const int bufferSize = 4096 * 16;
		boost::thread playThread;
};