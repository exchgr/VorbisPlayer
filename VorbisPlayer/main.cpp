#include <iostream>
using namespace std;

#include "OVStream.h"
#include <conio.h>
#include <windows.h>
#include <boost\thread.hpp>

int main(int argc, char** argv)
{
	char key = NULL;
	string timeString;
	int time;
	OVStream ovStream(argc, argv);

	while (key != 'q') {
		boost::this_thread::sleep(boost::posix_time::milliseconds(1));
		if (_kbhit()) {
			key = _getch();
			cerr << "key: " << key << endl;
			switch (key) {
				case 'p':
					if (ovStream.state() != AL_PLAYING) {
						cerr << "playing\n";
						ovStream.play();
					} else if (ovStream.state() == AL_PLAYING) {
						cerr << "pausing\n";
						ovStream.pause();
					}
					cerr << "state: " << ovStream.state() << endl;
					key = NULL;
					break;
				case 's':
					cerr << "stopping\n";
					ovStream.stop();
					cerr << "state: " << ovStream.state() << endl;
					key = NULL;
					break;
				case 'n':
					cerr << "next\n";
					ovStream.next();
					key = NULL;
					break;
				case 'b':
					cerr << "back\n";
					ovStream.back();
					key = NULL;
					break;
				case 't': // testing whether OVStream::seek() works. Not permanent.
					cerr << "Enter time in seconds: ";
					cin >> timeString;
					time = atoi(timeString.c_str()); // stringstream wouldn't work inside of a switch/case.
					ovStream.seek(time);
					break;
				case 'q':
					ovStream.stop();
					cerr << "exiting\n";
					break;
			}
		}
	}
	return 0;
}