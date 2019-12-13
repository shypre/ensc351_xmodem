/* myioFinal-prep.cpp -- Dec. 10 -- Copyright 2019 Craig Scratchley */
/* I will not examine you on the details of files posixThread.{hpp,cpp} */
#include <sys/socket.h>		// for AF_LOCAL, SOCK_STREAM
#include <condition_variable>
#include <shared_mutex>
#include <map>
#include <cstring>			// for memcpy ?
#include <atomic>
#include "posixThread.hpp"
#include <iostream>
// Pretend that we have a C++17 compiler instead of our C++14 one
#define shared_mutex shared_timed_mutex

/* Lock-free circular buffer.  This should be threadsafe if one thread is reading
 * and another is writing. */
template<class T>
class CircBuf {
	T *buf = nullptr;
	/* If read_pos == write_pos, the buffer is empty.
	 *
	 * There will always be at least one position empty, as a completely full
	 * buffer (read_pos == write_pos) is indistinguishable from an empty buffer.
	 *
	 * Invariants: read_pos < size, write_pos < size. */
	unsigned size = 0;
	std::atomic<unsigned> read_pos, write_pos;
public:
	CircBuf() {
		read_pos = write_pos = 0;
	}

	~CircBuf() {
		delete[] buf;
	}

	void reserve( unsigned n ) {
		read_pos = write_pos = 0;
		delete[] buf;
		buf = nullptr;
		/* Reserve an extra byte.  We'll never fill more than n bytes; the extra
		 * byte is to guarantee that read_pos != write_pos when the buffer is full,
		 * since that would be ambiguous with an empty buffer. */
		if( n != 0 ) {
			size = n+1;
			buf = new T[size];
		}
		else
			size = 0;
	}

	void get_write_pointers( T *pPointers[2], unsigned pSizes[2] ) {
		// wcs -- I have re-ordered the below two lines to optimize the code.
		const int wpos = write_pos.load( std::memory_order_relaxed );
		const int rpos = read_pos.load( std::memory_order_acquire );
		if( rpos <= wpos ) {
			/* The buffer looks like "eeeeDDDDeeee" or "eeeeeeeeeeee" (e = empty, D = data) */
			pPointers[0] = buf + wpos;
			pPointers[1] = buf;
			pSizes[0] = size - wpos;
			pSizes[1] = rpos;
		} else if( rpos > wpos ) {
			/* The buffer looks like "DDeeeeeeeeDD" (e = empty, D = data). */
			pPointers[0] = buf + wpos;
			pPointers[1] = nullptr;
			pSizes[0] = rpos - wpos;
			pSizes[1] = 0;
		}
		/* Subtract 1, to account for the element that we never fill. */
		if( pSizes[1] )
			pSizes[1] -= 1;
		else
			pSizes[0] -= 1;
	}

	void get_read_pointers( T *pPointers[2], unsigned pSizes[2] ) {
		const int rpos = read_pos.load( std::memory_order_relaxed );
		const int wpos = write_pos.load( std::memory_order_acquire );
		if( rpos <= wpos ) {
			/* The buffer looks like "eeeeDDDDeeee" or "eeeeeeeeeeee" (e = empty, D = data) */
			pPointers[0] = buf + rpos;
			pPointers[1] = nullptr;
			pSizes[0] = wpos - rpos;
			pSizes[1] = 0;
		} else if( rpos > wpos ) {
			/* The buffer looks like "DDeeeeeeeeDD" (e = empty, D = data). */
			pPointers[0] = buf + rpos;
			pPointers[1] = buf;
			pSizes[0] = size - rpos;
			pSizes[1] = wpos;
		}
	}

	/* Write buffer_size elements from buffer into the circular buffer object,
	 * and advance the write pointer.  Return the number of elements that were
	 * able to be written.  If
	 * the data will not fit entirely, as much data as possible will be fit
	 * in. */
	unsigned write( const T *buffer, unsigned buffer_size ) {
		using std::min;
		using std::max;
		T *p[2];
		unsigned sizes[2];
		get_write_pointers( p, sizes );
		buffer_size = min( buffer_size, sizes[0]+sizes[1]);//max_write_size=sizes[0]+sizes[1]
		const int from_first = min( buffer_size, sizes[0] );
		std::memcpy( p[0], buffer, from_first*sizeof(T) );
		if( buffer_size > sizes[0] )
			std::memcpy( p[1], buffer+from_first, max(buffer_size-sizes[0], 0u)*sizeof(T) );
		write_pos.store( (write_pos.load( std::memory_order_relaxed ) + buffer_size) % size,
						std::memory_order_release );
		return buffer_size;
	}

	/* Read buffer_size elements into buffer from the circular buffer object,
	 * and advance the read pointer.  Return the number of elements that were
	 * read.  If buffer_size elements cannot be read, as many elements as
	 * possible will be read */
	unsigned read( T *buffer, unsigned buffer_size ) {
		using std::max;
		using std::min;
		T *p[2];
		unsigned sizes[2];
		get_read_pointers( p, sizes );
		buffer_size = min( buffer_size, sizes[0]+sizes[1]);//max_read_size=sizes[0]+sizes[1];
		const int from_first = min( buffer_size, sizes[0] );
		std::memcpy( buffer, p[0], from_first*sizeof(T) );
		if( buffer_size > sizes[0] )
			std::memcpy( buffer+from_first, p[1], max(buffer_size-sizes[0], 0u)*sizeof(T) );
		read_pos.store( (read_pos.load( std::memory_order_relaxed ) + buffer_size) % size,
						std::memory_order_release );
		return buffer_size;
	}
}; /* Original source Copyright (c) 2004 Glenn Maynard.  See Craig for more details and code before improvements. */

using namespace std;
namespace{ //Unnamed (anonymous) namespace

class socketInfoClass;
typedef shared_ptr<socketInfoClass> socketInfoClassSp;
map<int, socketInfoClassSp> desInfoMap = {
		{0, nullptr}, // init for stdin, stdout, stderr
		{1, nullptr},
		{2, nullptr}
};

//	A shared mutex used to protect desInfoMap so only a single thread can modify the map at a time.
//  This also means that only one call to functions like mySocketpair() or myClose() can make progress at a time.
//  This mutex is also used to prevent a paired socket from being closed at the beginning of a myWrite or myTcdrain function.
shared_mutex mapMutex;

class socketInfoClass {
	int buffered = 0;
	CircBuf<char> circBuffer;
	condition_variable cvDrain;
	condition_variable cvRead;
	mutex socketInfoMutex;
public:
	int pair; 	// Cannot be private because myWrite and myTcdrain using it.
				// -1 when descriptor closed, -2 when paired descriptor is closed

	socketInfoClass(unsigned pairInit)
	:pair(pairInit) { circBuffer.reserve(300); } // note constant of 300

	 /* Function:  if necessary, make the calling thread wait for a reading thread to drain the data */
	int draining() { // operating on object for paired descriptor
		unique_lock<mutex> condlk(socketInfoMutex);
		if (pair >= 0 && buffered > 0)
			cvDrain.wait(condlk); //, [this] {return buffered <= 0 || pair < 0 ;});
			// what about spurious wakeup?
		if (pair == -2) {
			errno = EBADF; // check errno
			return -1;
		}
		return 0;
	}

	int writing(int des, const void* buf, size_t nbyte)	{
		// operating on object for paired descriptor
		lock_guard<mutex> condlk(socketInfoMutex);
		// int written = write(des, buf, nbyte);
		int written = circBuffer.write((const char*) buf, nbyte);
		buffered += written; // circBuffer won't return -1
		if (buffered >= 0)
			cvRead.notify_one();
		return written;
	}

	int reading(int des, void * buf, int n, int min, int time, int timeout,
			shared_lock<shared_mutex> &desInfoLk) {
		int bytesRead;
		unique_lock<mutex> condlk(socketInfoMutex);
		desInfoLk.unlock();
		if (buffered >= min || pair == -2) {
			// wcsReadcond should not wait in this situation.
			bytesRead = circBuffer.read((char *) buf, n);
			// bytesRead = read(des, buf, n); // at least min will be waiting.
			// bytesRead = wcsReadcond(des, buf, n, min, time, timeout);
			if (bytesRead > 0) {
				buffered -= bytesRead;
				if (!buffered /* && pair >= 0 */)
					cvDrain.notify_all();
			}
			else if (pair == -2) {
				errno = ECONNRESET;
				return -1;
			}
		}
		else {
			if (buffered < 0) {
				cout << "Currently only supporting one reading call at a time" << endl;
				exit(EXIT_FAILURE);
			}
			if (time != 0 || timeout != 0) {
				cout << "Currently only supporting no timeouts or immediate timeout" << endl;
				exit(EXIT_FAILURE);
			}
			buffered -= min;
			cvDrain.notify_all(); // buffered must now be <= 0

			cvRead.wait(condlk, [this] {return buffered >= 0 || pair < 0;});
			if (pair == -1) {
				errno = EBADF; // check errno value
				return -1;
			}
			bytesRead = circBuffer.read((char *) buf, n);
			// bytesRead = read(des, buf, n);
			// bytesRead = wcsReadcond(des, buf, n, min, time, timeout);
			buffered -= (bytesRead - min);
			if (!buffered /* && pair >= 0 */)
				// myTcdrain thread(s) might have snuck in at the end of cvRead.wait()
				cvDrain.notify_all();
			if (bytesRead == 0) {
				errno = ECONNRESET;
				return -1;
			}
		}
		return bytesRead;
	} // .reading()

	int closing(int des) {
		// mapMutex already locked at this point, so no mySocketpair or other myClose
		if(pair != -2) { // pair has not already been closed
			socketInfoClassSp des_pair(desInfoMap[pair]);
			unique_lock<mutex> condlk(socketInfoMutex, defer_lock);
			unique_lock<mutex> condPairlk(des_pair->socketInfoMutex, defer_lock);
			lock(condPairlk, condlk);
			pair = -1; // this is first socket in the pair to be closed
			des_pair->pair = -2; // paired socket will be the second of the two to close.
			if (des_pair->buffered < 0) {
				// no more data will be written from des
				des_pair->buffered = 0; // pretend just enough data was written
				// notify thread(s) waiting on reading on paired descriptor
				des_pair->cvRead.notify_all();
			}
			else if (des_pair->buffered > 0) {
				// there shouldn't be any threads waiting in myTcdrain on des, but just in case.
				des_pair->cvDrain.notify_all();
			}
			if (buffered > 0) {
				// by closing the socket we are throwing away any buffered data.
				// notification will be sent immediately below to any myTcdrain waiters on paired descriptor.
				buffered = 0;
				cvDrain.notify_all();
			}
			else if (buffered < 0) {
				// there shouldn't be any threads waiting in myReadond() on des, but just in case.
				buffered = 0;
				cvRead.notify_all();
			}
		}
		return 0;
	} // .closing()
}; // socketInfoClass

// get shared pointer for des
socketInfoClassSp getDesInfoP(int des) {
	auto iter = desInfoMap.find(des);
	if (iter == desInfoMap.end())
		return nullptr; // des not in use
	else
		return iter->second; // return the shared pointer
}
} // unnamed namespace

/* see https://developer.blackberry.com/native/reference/core/com.qnx.doc.neutrino.lib_ref/topic/r/readcond.html */
int myReadcond(int des, void * buf, int n, int min, int time, int timeout) {
    shared_lock<shared_mutex> desInfoLk(mapMutex);
	socketInfoClassSp desInfoP = getDesInfoP(des);
	if (!desInfoP) {
		desInfoLk.unlock();
		// not an open socket [created with mySocketpair()]
		errno = EBADF; return -1;
	}
	return desInfoP->reading(des, buf, n, min, time, timeout, desInfoLk);
}

ssize_t myWrite(int des, const void* buf, size_t nbyte) {
    shared_lock<shared_mutex> desInfoLk(mapMutex);
	socketInfoClassSp desInfoP = getDesInfoP(des);
	if(desInfoP) {
		if (desInfoP->pair >= 0) {
			// locking mapMutex above makes sure that desinfoP->pair is not closed here
			auto desPairInfoSp = desInfoMap[desInfoP->pair]; // make a local shared pointer
			return desPairInfoSp->writing(des, buf, nbyte);
		}
		else {
			errno = EPIPE; return -1;
		}
	}
	errno = EBADF; return -1;
}

int myTcdrain(int des) {
    shared_lock<shared_mutex> desInfoLk(mapMutex);
	socketInfoClassSp desInfoP = getDesInfoP(des);
	if(desInfoP) {
		if (desInfoP->pair == -2)
			return 0; // paired descriptor is closed.
		else { // pair == -1 won't be in *desInfoP now
			// locking mapMutex above makes sure that desinfoP->pair is not closed here
			auto desPairInfoP = desInfoMap[desInfoP->pair]; // make a local shared pointer
			desInfoLk.unlock();
			return desPairInfoP->draining();
		}
	}
	errno = EBADF; return -1;
}

int mySocketpair(int domain, int type, int protocol, int des[2]) {
	lock_guard<shared_mutex> desInfoLk(mapMutex);
	int returnVal = socketpair(domain, type, protocol, des);
	if(returnVal != -1) {
		desInfoMap[des[0]] = make_shared<socketInfoClass>(des[1]);
		desInfoMap[des[1]] = make_shared<socketInfoClass>(des[0]);
	}
	return returnVal;
}

int myClose(int des) {
	int retVal = 1;
	lock_guard<shared_mutex> desInfoLk(mapMutex);
	auto iter = desInfoMap.find(des);
	if (iter != desInfoMap.end()) { // if in the map
		if (iter->second) // if shared pointer exists
			retVal = iter->second->closing(des);
		desInfoMap.erase(des);
	}
	if (retVal == 1) { // if not-in-use
		errno = EBADF;
		retVal = -1;
	}
	return retVal;
}

static int daSktPr[2]; // Descriptor Array for Socket Pair
char	B[200]; // initially zeroed (filled with NUL characters)

void coutThreadFunc(void) {
	int RetVal;
	CircBuf<char> buffer;
	buffer.reserve(5); // note constant of 5
	buffer.write("123456789", 10); // don't forget nul termination character
	RetVal = buffer.read(B, 200);
	cout << "Output Line 1 - RetVal: " << RetVal << " B: " << B << endl;

    RetVal = myReadcond(daSktPr[1], B, 200, 12, 0, 0);

	cout << "Output Line 2 - RetVal: " << RetVal << " B: " << B << endl;
	myWrite(daSktPr[0], "ab", 3); // don't forget nul termination character
    RetVal = myReadcond(daSktPr[1], B, 200, 12, 0, 0);

	cout << "Output Line 3 - RetVal: " << RetVal;
	if (RetVal == -1)
		cout << " Error:  " << strerror(errno) << endl;
	else
		cout << " B: " << B << endl;
    RetVal = myReadcond(daSktPr[1], B, 200, 12, 0, 0);
	cout << "Output Line 4 - RetVal: " << RetVal;
	if (RetVal == -1)
		cout << " Error:  " << strerror(errno) << endl;
	else
		cout << " B: " << B << endl;
	RetVal = myWrite(daSktPr[0], "123456789", 10); // don't forget null termination character
	cout << "Output Line 5 - RetVal: " << RetVal;
	if (RetVal == -1)
		cout << " Error:  " << strerror(errno) << endl;
	else cout << endl;
}
int main() { // launch configurations ensure all threads will run on same core with a realtime policy.
	try {
		pthreadSupport::setSchedPrio(60);
		mySocketpair(AF_LOCAL, SOCK_STREAM, 0, daSktPr);
		myWrite(daSktPr[0], "abc", 3);
		pthreadSupport::posixThread coutThread(SCHED_FIFO, 50, coutThreadFunc); // lower
		myTcdrain(daSktPr[0]);

		myWrite(daSktPr[0], "123456789", 10); // don't forget nul termination character
		pthreadSupport::setSchedPrio(40); // lower priority still

		myClose(daSktPr[0]);
		coutThread.join();
		return 0;
	}
	catch (system_error& error) {
		cout << "Error: " << error.code() << " - " << error.what() << '\n';
		cout << "Have you launched process with a realtime scheduling policy?" << endl;
	}
	catch (...) { throw; }
}




