/*
 * Medium.cpp
 *
 *      Author: Craig Scratchley
 *      Copyright(c) 2019 Craig Scratchley
 */

#include <fcntl.h>
#include <unistd.h> // for write()
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/select.h>
#include "Medium.h"
#include "myIO.h"
#include "VNPE.h"
#include "AtomicCOUT.h"

#include "PeerX.h"

//debug
#include <iostream>

// Uncomment the line below to turn on debugging output from the medium
//#define REPORT_INFO

#define SEND_EXTRA_ACKS

//This is the kind medium.

using namespace std;

Medium::Medium(int d1, int d2, const char *fname)
:Term1D(d1), Term2D(d2), logFileName(fname)
{
	byteCount = 1;
	ACKforwarded = 0;
	ACKreceived = 0;
	sendExtraAck = false;

	mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
	logFileD = PE2(myCreat(logFileName, mode), logFileName);
}

Medium::~Medium() {
}

bool Medium::MsgFromTerm2()
{
	blkT bytesReceived;
	int numOfByteReceived;
	int byteToCorrupt;

	// could read in bytes one-by-one if desired
//	if (!(numOfByteReceived = PE(myRead(Term2D, bytesReceived, 1))))
    COUT << "Medium: myRead(term2) " << endl;
	if (!(numOfByteReceived = PE(myRead(Term2D, bytesReceived, (BLK_SZ_CRC+1) / 2)))) {
		COUT << "Medium thread: TERM2's socket closed, Medium terminating" << endl;
		return true;
	}
	//debug
	COUT << "Medium: received num bytes from term2: " << numOfByteReceived << endl;

	byteCount += numOfByteReceived;
	if (byteCount >= 392) {
		byteCount = byteCount - 392;
		//byteToCorrupt = numOfByteReceived - byteCount;
		byteToCorrupt = numOfByteReceived - byteCount - 1;
		bytesReceived[byteToCorrupt] = (255 - bytesReceived[byteToCorrupt]);
#ifdef REPORT_INFO
		COUT << "<" << byteToCorrupt << "x>" << flush;
#endif
	}

	PE_NOT(myWrite(logFileD, bytesReceived, 1), 1);
	//Forward the bytes to RECEIVER,
    COUT << "Medium: myWrite() to term1, num bytes: " << 1 << endl;
	PE_NOT(myWrite(Term1D, bytesReceived, 1), 1);

	if (sendExtraAck) {
#ifdef REPORT_INFO
			COUT << "{" << "+A" << "}" << flush;
#endif
		COUT << "Medium: extra acks" << endl;
		uint8_t buffer = ACK;
		PE_NOT(myWrite(logFileD, &buffer, 1), 1);
		//Write the buffer to term2,
	    COUT << "Medium: myWrite() to term2, num bytes: " << 1 << endl;
		PE_NOT(myWrite(Term2D, &buffer, 1), 1);

		sendExtraAck = false;
	}

	PE_NOT(myWrite(logFileD, &bytesReceived[1], numOfByteReceived-1), numOfByteReceived-1);
	//Forward the bytes to RECEIVER,
    COUT << "Medium: myWrite() to term1, num bytes: " << numOfByteReceived-1 << endl;
	PE_NOT(myWrite(Term1D, &bytesReceived[1], numOfByteReceived-1), numOfByteReceived-1);

	return false;
}

bool Medium::MsgFromTerm1()
{
	uint8_t buffer[CAN_LEN];
    COUT << "Medium: myRead(term1)" << endl;
	int numOfByte = PE(myRead(Term1D, buffer, CAN_LEN));
	if (numOfByte == 0) {
		COUT << "Medium thread: TERM1's socket closed, Medium terminating" << endl;
		return true;
	}
    //debug
    COUT << "Medium: received num bytes from term1: " << numOfByte << endl;

	/*note that we record the errors in the log file*/
	if(buffer[0]==ACK)
	{
		ACKreceived++;

		if((ACKreceived%10)==0)
		{
			ACKreceived = 0;
			buffer[0]=NAK;
#ifdef REPORT_INFO
			COUT << "{" << "AxN" << "}" << flush;
#endif
		}
#ifdef SEND_EXTRA_ACKS
		else/*actually forwarded ACKs*/
		{
			ACKforwarded++;

			if((ACKforwarded%6)==0)/*Note that this extra ACK is not an ACK forwarded from receiver to the sender, so we don't increment ACKforwarded*/
			{
				ACKforwarded = 0;
				sendExtraAck = true;
			}
		}
#endif
	}

	PE_NOT(myWrite(logFileD, buffer, numOfByte), numOfByte);

	//Forward the buffer to term2,
    COUT << "Medium: myWrite() to term2, num bytes: " << numOfByte << endl;
	PE_NOT(myWrite(Term2D, buffer, numOfByte), numOfByte);
	return false;
}

void Medium::run()
{
	fd_set cset;
	FD_ZERO(&cset);

	bool finished=false;
	while(!finished) {

		//note that the file descriptor bitmap must be reset every time
		FD_SET(Term1D, &cset);
		FD_SET(Term2D, &cset);

		int rv = PE(select( max(Term2D,
							Term1D)+1, &cset, NULL, NULL, NULL ));
		if( rv == 0 ) {
			// timeout occurred
			CERR << "The medium should not timeout" << endl;
			exit (EXIT_FAILURE);
		} else {
			if( FD_ISSET( Term1D, &cset ) ) {
				finished = MsgFromTerm1();
			}
			if( FD_ISSET( Term2D, &cset ) ) {
				finished |= MsgFromTerm2();
			}
		}
	};
	PE(myClose(logFileD));
    COUT << "Medium: myClose(term1)" << endl;
	PE(myClose(Term1D));
    COUT << "Medium: myClose(term2)" << endl;
	PE(myClose(Term2D));
}
