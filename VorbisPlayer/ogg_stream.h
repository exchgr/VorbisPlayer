#pragma once
#include <iostream>
#include <fstream>
#include <string>
using namespace std;

#include <al.h>
#include <ogg\ogg.h>
#include <vorbis\codec.h>
#include <vorbis\vorbisenc.h>
#include <vorbis\vorbisfile.h>

class ogg_stream
{
	public:
		void openFile(string path);
		void releaseFile();
		void displayInfo();
		bool play();
		bool isPlaying();
		bool updateStream();

	protected:
		bool stream(ALuint buffer);
		void emptyQueue();
		void checkALError();
		string errorStringify(int code);

	private:
		FILE* oggFile;
		OggVorbis_File oggStream;
		vorbis_info* vorbisInfo;
		vorbis_comment* vorbisComment;

		ALuint buffers[2]; // front and back buffers
		ALuint source;
		ALenum format;
};

