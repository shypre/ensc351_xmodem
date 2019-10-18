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
#include <condition_variable>
//debug
#include <iostream>
#include <cstdlib>


struct file_des_lock
{
    int file_des;
    int file_des_pair;
    std::mutex my_mutex;
    std::condition_variable my_cond;
    int buffered_bytes = 0;
    bool is_socket;

    file_des_lock(int new_file_des, int new_file_des_pair, bool is_socket)
    {
        file_des = new_file_des;
        file_des_pair = new_file_des_pair;
        this->is_socket = is_socket;
        //debug
        std::cout << "myIO: new des obj: " << new_file_des << ", pair: " << new_file_des_pair << ", is_socket: " << is_socket << std::endl;
    }

    bool buffer_is_empty()
    {
        //debug
        std::cout << "des: " << file_des << ", buffered_bytes: " << buffered_bytes << std::endl;
        return buffered_bytes == 0;
    }
};

struct file_des_list
{
    std::vector<file_des_lock*> file_des_lock_list;
    std::mutex list_mutex;

    file_des_lock* object_with(int des)
    {
        std::lock_guard<std::mutex> my_lock(list_mutex);
        int pos = contains(des);
        if (pos != -1)
        {
            return file_des_lock_list[pos];
        }
        return nullptr;
    }

    bool insert(int des, int des_pair, bool is_socket)
    {
        std::lock_guard<std::mutex> my_lock(list_mutex);
        int pos = contains(des);
        if (pos == -1)
        {
            file_des_lock_list.push_back(new file_des_lock(des, des_pair, is_socket));
            return true;
        }
        return false;
    }

    bool remove(int des)
    {
        std::lock_guard<std::mutex> my_lock(list_mutex);
        int pos = contains(des);
        if (pos != -1)
        {
            //debug
            std::cout << "myIO: remove des obj: " << des << std::endl;
            delete file_des_lock_list[pos];
            file_des_lock_list.erase(file_des_lock_list.begin() + pos);
            return true;
        }
        return false;
    }

private:
    int contains(int des)
    {
        for (int i = 0; i < file_des_lock_list.size(); ++i)
        {
            if (file_des_lock_list[i]->file_des == des)
            {
                return i;
            }
        }
        return -1;
    }
};


file_des_list my_file_des_list;


//forward declaration for implementation below
int myReadcond(int des, void * buf, int n, int min, int time, int timeout);

//create socketpair between term and medium
int mySocketpair( int domain, int type, int protocol, int des[2] )
{
	int returnVal = socketpair(domain, type, protocol, des);
	if (returnVal != 0)
	{
	    std::cerr << "Socket pair creation failed" << std::endl;
	}
	else
	{
	    //std::lock_guard<std::mutex> my_lg(file_des_list_mutex);
	    my_file_des_list.insert(des[0], des[1], true);
	    my_file_des_list.insert(des[1], des[0], true);
	}
	return returnVal;
}

//opens a file from filesystem
int myOpen(const char *pathname, int flags, mode_t mode)
{
    int file_des = open(pathname, flags, mode);
    //std::lock_guard<std::mutex> my_lg(file_des_list_mutex);
    my_file_des_list.insert(file_des, -1, false);
	return file_des;
}

//create a file on filesystem
int myCreat(const char *pathname, mode_t mode)
{
    int file_des = creat(pathname, mode);
    //std::lock_guard<std::mutex> my_lg(file_des_list_mutex);
    my_file_des_list.insert(file_des, -1, false);
    return file_des;
}

ssize_t myRead( int des, void* buf, size_t nbyte )
{
    // file and socket descriptors won't collide: https://stackoverflow.com/questions/13378035/socket-and-file-descriptors
    // ... deal with reading from descriptors for files
    //should never be null
    file_des_lock* file_des_obj = my_file_des_list.object_with(des);
    if (file_des_obj->is_socket == false)
    {
        std::lock_guard<std::mutex> my_lg(file_des_obj->my_mutex);
        //debug
        std::cout << "myIO: myRead(): " << des << std::endl;
        int bytes_read = read(des, buf, nbyte );
        return bytes_read;
    }
    // myRead (for our socketpairs) reads a minimum of 1 byte
    //otherwise it must be a socket
    else
    {
        return myReadcond(des, buf, nbyte, 1, 0, 0);
    }
}

/*
ssize_t myRead( int fildes, void* buf, size_t nbyte )
{
	return read(fildes, buf, nbyte );
}
*/

ssize_t myWrite( int fildes, const void* buf, size_t nbyte )
{
    file_des_lock* my_sock = my_file_des_list.object_with(fildes);
    //should never happen
    if (my_sock == nullptr)
    {
        std::cerr << "Descriptor " << fildes << " does not exist" << std::endl;
    }
    std::lock_guard<std::mutex> my_lock(my_sock->my_mutex);
    //debug
    std::cout << "myIO: myWrite(): " << fildes << std::endl;

    int bytes_written = write(fildes, buf, nbyte );

    if (my_sock->is_socket == true)
    {
        my_sock->buffered_bytes += bytes_written;
    }

	return bytes_written;
}

int myClose( int fd )
{
    //std::lock_guard<std::mutex> my_lg(file_des_list_mutex);
    //debug
    std::cout << "myIO: myClose(): " << fd << std::endl;
    int result = close(fd);
    if (result != -1)
    {
        my_file_des_list.remove(fd);
    }
	return result;
}

//blocks thread that just wrote data into a socket until all data that it wrote is read out by the other end
int myTcdrain(int des)
{ //is also included for purposes of the course.
    //debug
    std::cout << "myIO: myTcdrain(): " << des << std::endl;
    file_des_lock* file_des_obj = my_file_des_list.object_with(des);
    std::unique_lock<std::mutex> my_lock(file_des_obj->my_mutex);
    file_des_obj->my_cond.wait(my_lock, [file_des_obj]{/*while(!(file_des_obj->buffer_is_empty())){sleep(3);}*/ return file_des_obj->buffer_is_empty();});
	return 0;
}

/* See:
 *  https://developer.blackberry.com/native/reference/core/com.qnx.doc.neutrino.lib_ref/topic/r/readcond.html
 *
 *  */
int myReadcond(int des, void * buf, int n, int min, int time, int timeout)
{

    //debug
    std::cout << "myIO: myReadcond(): " << des << std::endl;

    int bytes_read = wcsReadcond(des, buf, n, min, time, timeout );
    std::cout << "myIO: myReadcond read: " << bytes_read << std::endl;


    //race condition occurs where the des in the des_list is removed by Medium during blocking wcsreadcond() and dereferencing file_des_obj_pair here results in SIGSEGV
    //this should prevent that from happening
    file_des_lock* file_des_obj = my_file_des_list.object_with(des);
    if (file_des_obj == nullptr)
    {
        return bytes_read;
    }
    file_des_lock* file_des_obj_pair = my_file_des_list.object_with(file_des_obj->file_des_pair);
    if (file_des_obj_pair == nullptr)
    {
        return bytes_read;
    }

    std::lock_guard<std::mutex> my_pair_lock(file_des_obj_pair->my_mutex);
    file_des_obj_pair->buffered_bytes -= bytes_read;


    file_des_obj_pair->my_cond.notify_one();

	return bytes_read;
}

