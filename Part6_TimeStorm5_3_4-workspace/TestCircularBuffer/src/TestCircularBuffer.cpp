//============================================================================
// Name        : TestCircularBuffer.cpp
// Author      : W. Craig Scratchley
// Version     :
// Copyright   : Copyright (C) 2017-2019 - W. Craig Scratchley

#include <iostream>
#include <thread>

#include "RageUtil_CircularBuffer.h"

using namespace std;

CircBuf<unsigned char> buffer;

//#define BUF_SIZE 10000
#define BUF_SIZE 1

void thread2Func(void)
{
	cout << "!!!thread2 start!!!" << endl; // prints !!!Hello World!!!
	unsigned char count=0;

	while (true) {
		unsigned char buf[BUF_SIZE];
		int bufSize = rand() % (BUF_SIZE + 1) ;
		for (int i=0; i<bufSize; ++i) {
			buf[i] = count++;
		};
		int written = buffer.write(buf, bufSize);
		count -= (bufSize - written);
		// cout << "Count, written " << (unsigned) count << ", " << written << endl;
	}
}

int main() {
	cout << "!!!main start!!!" << endl;
	buffer.reserve(BUF_SIZE);

	auto startTime = std::chrono::system_clock::now();
	thread thread2(thread2Func);

	unsigned long long count=0;
	while (true) {
		unsigned char buf[BUF_SIZE];
		int bufSize = rand() % (BUF_SIZE + 1) ;
		int numRead = buffer.read(buf, bufSize);
		// cout << "Count, numRead " << count << ", " << numRead << endl;
		for (int i=0; i<numRead; ++i) {
			if (buf[i] != (unsigned char) count++) {
				auto endTime = std::chrono::system_clock::now();
				auto timeItTook = endTime - startTime;
				cout << "Problem.  Count was: " << (count - 1) << endl;
				cout << "Problem occurred after " << chrono::duration_cast<chrono::minutes>(timeItTook).count() << " minutes." << endl;
				exit(EXIT_FAILURE);
				//count += numRead - i - 1;
				//break;
			};
		};
	}

	return 0;
}
