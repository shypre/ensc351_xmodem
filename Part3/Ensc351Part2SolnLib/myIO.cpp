//============================================================================
//
//% Student Name 1: student1
//% Student 1 #: 123456781
//% Student 1 userid (email): stu1 (stu1@sfu.ca)
//
//% Student Name 2: student2
//% Student 2 #: 123456782
//% Student 2 userid (email): stu2 (stu2@sfu.ca)
//
//% Below, edit to list any people who helped you with the code in this file,
//%      or put 'None' if nobody helped (the two of) you.
//
// Helpers: _everybody helped us/me with the assignment (list names or put 'None')__
//
// Also, list any resources beyond the course textbooks and the course pages on Piazza
// that you used in making your submission.
//
// Resources:  ___________
//
//%% Instructions:
//% * Put your name(s), student number(s), userid(s) in the above section.
//% * Also enter the above information in other files to submit.
//% * Edit the "Helpers" line and, if necessary, the "Resources" line.
//% * Your group name should be "P3_<userid1>_<userid2>" (eg. P3_stu1_stu2)
//% * Form groups as described at:  https://courses.cs.sfu.ca/docs/students
//% * Submit files to courses.cs.sfu.ca
//
// File Name   : myIO.cpp
// Version     : September, 2019
// Description : Wrapper I/O functions for ENSC-351
// Copyright (c) 2019 Craig Scratchley  (wcs AT sfu DOT ca)
//============================================================================

#include <unistd.h>			// for read/write/close
#include <fcntl.h>			// for open/creat
#include <sys/socket.h> 		// for socketpair
#include "SocketReadcond.h"
#include <vector>
#include <mutex>
#include <thread>


int mySocketpair( int domain, int type, int protocol, int des[2] )
{
	int returnVal = socketpair(domain, type, protocol, des);
	return returnVal;
}

int myOpen(const char *pathname, int flags, mode_t mode)
{
	return open(pathname, flags, mode);
}

int myCreat(const char *pathname, mode_t mode)
{
	return creat(pathname, mode);
}

ssize_t myRead( int fildes, void* buf, size_t nbyte )
{
	return read(fildes, buf, nbyte );
}

ssize_t myWrite( int fildes, const void* buf, size_t nbyte )
{
	return write(fildes, buf, nbyte );
}

int myClose( int fd )
{
	return close(fd);
}

int myTcdrain(int des)
{ //is also included for purposes of the course.

	return 0;
}

/* See:
 *  https://developer.blackberry.com/native/reference/core/com.qnx.doc.neutrino.lib_ref/topic/r/readcond.html
 *
 *  */
int myReadcond(int des, void * buf, int n, int min, int time, int timeout)
{
	return wcsReadcond(des, buf, n, min, time, timeout );
}
