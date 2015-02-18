
/*
 * Copyright (c) 1998 University of Southern California.
 * All rights reserved.                                            
 *                                                                
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation, advertising
 * materials, and other materials related to such distribution and use
 * acknowledge that the software was developed by the University of
 * Southern California, Information Sciences Institute.  The name of the
 * University may not be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED "AS IS" AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 */

#ifndef nam_stream_h
#define nam_stream_h

#include <stdio.h>
#include <sys/types.h>
#ifdef WIN32
#include <windows.h>
#include <io.h>
#else
#include <unistd.h>
#endif

#include "nam.h"  // for die

class NamStream : public TclObject {
 protected:
	int is_open_;
	
 public:
	NamStream() : is_open_(0) {};
	// NamStream(int fd) : is_open_(0) {};
	NamStream(const char *fn) : is_open_(0) {}
	virtual ~NamStream() { if (is_open_) close(); };

	int command(int argc, const char*const* argv);
	static NamStream *open(const char *fn);

	int is_ok() { return is_open_; }
	virtual int seekable() { return 1; }

	virtual char *gets(char *buf, int len) { return NULL; };
	virtual char get_char() { return EOF; }
	virtual char *rgets(char *buf, int len);
	virtual off_t seek(off_t offset, int whence) { return -1; };
	virtual off_t tell() { return -1; };
	virtual int close() { is_open_ = 0; return 0; }
	virtual int eof() { return 1; };
	virtual int read(char *buf, int size) { return 0; }
};

class NamStreamFile : public NamStream {
	FILE *file_;

 public:
	// NamStreamFile(int fd);
	NamStreamFile(const char *fn);

	virtual int seekable() { return 1; }

	virtual char *gets(char *buf, int len);
	virtual char get_char();
	virtual off_t seek(off_t offset, int whence);
	virtual off_t tell();
	virtual int close();
	virtual int eof();
	virtual int read(char *buf, int size);
};

#ifdef HAVE_ZLIB_H
#include <zlib.h>

class NamStreamCompressedFile : public NamStream {
	gzFile file_;

 public:
	NamStreamCompressedFile(const char *fn);

	virtual int seekable() { return 0; }

	virtual char *gets(char *buf, int len);
	virtual char get_char();
	virtual off_t seek(off_t offset, int whence);
	virtual off_t tell();
	virtual int close();
	virtual int eof();
	virtual int read(char *buf, int size);
};
#endif

/*
 * Make front_ seem like a good, seekable file
 * when really it's a low-down pipe.
 * We do this by saving the output in back_.
 */
class NamStreamPipe : public NamStream {
 protected:
	int    front_;	// file descriptor of the pipe
	FILE  *back_;	// temporary backup file
	off_t  back_len_;	// file size of the backup file
	char  *pipename_;	// the pipe name

	// double linked list of all NamStreamPipe instances
	NamStreamPipe *prev_;
	NamStreamPipe *next_;
	static NamStreamPipe *head_;
	static int instances_;	// number of NamStreamPipe instances

	// timer to check pipe input periodically
	static Tcl_TimerToken timer_;
	static void timer_handler(ClientData data);
	static int  read_pipe();

	void insure_backing(off_t lim);

 public:
	NamStreamPipe(const char *fn);
	~NamStreamPipe();

	virtual int seekable() { return 0; }

	virtual char* gets(char *buf, int len);
	virtual char  get_char();
	virtual off_t seek(off_t offset, int whence);
	virtual off_t tell();
	virtual int   close();
	virtual int   eof();
	virtual int   read(char *buf, int size);

	/*
	 * The Tcl command to open a pipe stream is:
	 *     set stream [new NamStream $tracefile]
	 * that is, the instance construction command.
	 * Since this command can be executed several times on the same
	 * pipe, we need to make sure not to create duplicate instance.
	 */
	 static NamStreamPipe *open_pipe(const char *fn);
};

#endif /* nam_stream_h */
