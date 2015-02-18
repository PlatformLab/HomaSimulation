
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

#include <stdlib.h>
#include <sys/types.h>
#include <limits.h>

#include <assert.h>

#include <stdlib.h>  // for atof
#include <fcntl.h>	// for O_NONBLOCK
#include <sys/stat.h>	// for lstat()

#include <tclcl.h>
#include "trace.h"
#include "nam_stream.h"


/**********************************************************************/

class NamStreamClass : public TclClass {
public:
	NamStreamClass() : TclClass("NamStream") {}
	TclObject* create(int argc, const char*const* argv) {
		if (argc < 5) 
			return 0;
		return NamStream::open(argv[4]);
	}
} NamStream_class;

int
NamStream::command(int argc, const char *const *argv)
{
	Tcl& tcl = Tcl::instance();

	if (0) {
	} else if (argc == 2 && strcmp(argv[1], "gets") == 0) {
		if (eof())
			return TCL_ERROR;
		// return a line
		char *buf = tcl.buffer();
		assert(4096 > TRACE_LINE_MAXLEN+1);  // can the buffer handle us?
		gets(buf, TRACE_LINE_MAXLEN);
		buf[TRACE_LINE_MAXLEN] = 0;  // paranoia
		tcl.result(buf);
		return TCL_OK;
	} else if (argc == 2 && strcmp(argv[1], "close") == 0) {
		close();
		return TCL_OK;
	} else if (argc == 2 && strcmp(argv[1], "eof") == 0) {
		tcl.resultf("%d", eof());
		return TCL_OK;
	};
	return (TclObject::command(argc, argv));
}

NamStream *
NamStream::open(const char *fn)
{
	struct stat state;

	if (strcmp(fn, "-") == 0 || strcmp(fn, "-.nam") == 0)
		return NamStreamPipe::open_pipe("-");

#ifndef WIN32
	// Windows doesn't have the lstat and S_ISFIFO functions

	if (lstat(fn, &state) < 0)
		return NULL;
	if (S_ISFIFO(state.st_mode))
		return NamStreamPipe::open_pipe(fn);
#endif

	if (strcmp(fn + strlen(fn) - 3, ".gz") == 0 ||
	    strcmp(fn + strlen(fn) - 2, ".Z") == 0) {
#ifdef HAVE_ZLIB_H
		return new NamStreamCompressedFile(fn);
#else /* ! HAVE_ZLIB */
		fprintf(stderr, "nam not built with zlib; cannot read compressed files.\n");
		return NULL;
#endif /* HAVE_ZLIB */
	};
	/* hope we've got it now */
	return new NamStreamFile(fn);
}

/*
 * rgets (gets in reverse order)
 */
char *
NamStream::rgets(char *buf, int len)
{
	int ch;

	/*
	 * prior-line \n current-line \n next-line
	 *
	 * Initially the cursor is on the n of next-line.
	 * read in current-line (which is behind us)
	 * return it
	 * leave the cursor on c of current line.
	 */

	/* first make sure we back over the prior newline, if any */
	if (seek(-1, SEEK_CUR) < 0)
		return NULL;
	ch = get_char();
	if (seek(ch == '\n' ? -2 : -1, SEEK_CUR) < 0)
		return NULL;
	/* now loop backwards until we get to the newline separating
	 * prior and current.
	 */
	off_t pos = tell();
	for(;;) {
		ch = get_char();
		if (ch == '\n')
			break;
		if (pos == 0) {
			// beginning of file
			if (seek(-1, SEEK_CUR) < 0)
				return NULL;
			break;
		}
		// back up a char & try again
		pos--;
		if (seek(-2, SEEK_CUR) < 0)
			return NULL;
	};
	/* we're just passed the newline for prior-line, or we're at 0 */
	/* read current-line, then reset the pointer there */
	gets(buf, len);
	if (pos != seek(pos, SEEK_SET))
		return NULL;
	return buf;
}


/**********************************************************************/

#if 0
NamStreamFile::NamStreamFile(int fd) : NamStream(fd)
{
	file_ = fdopen(fd, "r");
	is_open_ = (NULL != file_);
}
#endif /* 0 */

NamStreamFile::NamStreamFile(const char *fn) : NamStream(fn)
{
#ifdef WIN32
	// Open in raw binary mode; we'll get rid of \r\n manually.
	// Otherwise ftell() and fseek() does not work properly. :(
	file_ = fopen(fn, "rb");
#else
	file_ = fopen(fn, "r");
#endif
	is_open_ = (NULL != file_);
	if (!is_open_) 
		perror(fn);
}

char *
NamStreamFile::gets(char *buf, int len) {
  char ch = fgetc(file_);
  if (ch == '\n') {
    return fgets(buf, len-1, file_);
  } else {
    ungetc(ch, file_);
    return fgets(buf, len, file_);
  }
}

char
NamStreamFile::get_char()
{
	return getc(file_);
}

off_t
NamStreamFile::seek(off_t offset, int whence)
{
	if (0 == fseek(file_, offset, whence))
		return ftell(file_);
	else return -1;
}

off_t
NamStreamFile::tell()
{
	return ftell(file_);
}

int
NamStreamFile::close()
{
	// fclose(NULL) crashes on Windows, so double check
	if (file_ == NULL) return 0;

	int e = fclose(file_);
	file_ = NULL;
	return e;
}

int
NamStreamFile::eof()
{
	// feof(NULL) crashes on Windows, so double check
	return (file_ == NULL) || feof(file_);
}

int
NamStreamFile::read(char *buf, int size)
{
      return fread(buf, 1, size, file_);
}



/**********************************************************************/

#ifdef HAVE_ZLIB_H

/*
 * Beware:
 * nam *requires* zlib-1.1.3, as we trip over bugs in 1.1.2's gz* functions.
 */

NamStreamCompressedFile::NamStreamCompressedFile(const char *fn) : NamStream(fn)
{
#ifndef ZLIB_VERSION
	die("zlib version not specified.\n");
	int a, b, c;
	if (3 != sscanf(ZLIB_VERSION, "%d.%d.%d", &a, &b, &c)) {
		die("zlib version: unknown format.\n");
	};
	if (!(a > 1 ||
	    (a == 1 && b > 1) ||
	      (a == 1 && b == 1 && c >= 3)))
		die("zlib version is too old, nam requires 1.1.3.\n");
#endif

	file_ = gzopen(fn, "r");
	is_open_ = (NULL != file_);
}

char *
NamStreamCompressedFile::gets(char *buf, int len)
{
	char *b = gzgets(file_, buf, len);
	return b;
}

char 
NamStreamCompressedFile::get_char()
{
	return gzgetc(file_);
}

off_t
NamStreamCompressedFile::seek(off_t offset, int whence)
{
	if (whence == SEEK_END) {
		/*
		 * zlib doesn't support SEEK_END :-<
		 * Walk our way to the end-of-file.
		 */
		off_t p = gzseek(file_, 0, SEEK_SET), q;
#define STEP (16*1024)
		char buf[STEP];
		int count;
		for (;;) {
			/*
			 * Sigh.  We actually move all the bytes.  XXX
			 * (we can't lseek because we'd lseek
			 * past eof without knowing).
			 */
			count = gzread(file_, buf, STEP);
			if (count <= 0)
				break;
			p += count;
		};
		q = gztell(file_);
		assert (p == q);
		return q;
	} else {
		return gzseek(file_, offset, whence);
	};
}

off_t
NamStreamCompressedFile::tell()
{
	return gztell(file_);
}

int
NamStreamCompressedFile::close()
{
	int e = gzclose(file_);
	file_ = NULL;
	return e;
}

int
NamStreamCompressedFile::eof()
{
	return gzeof(file_);
}

int
NamStreamCompressedFile::read(char *buf, int size)
{
	int e = gzread(file_, buf, size);
	return e;
}


#endif /* HAVE_ZLIB */



/**********************************************************************
 * Implementation of class NamStreamPipe
 * - The read operation called by nam is always performed on the backup
 *   file.
 * - Pipe data are copied to backup file and never returned to nam
 *   directly.
 * - Pipes are checked when timer expires or nam executes read
 *   operations.
 *********************************************************************/

// static data members
NamStreamPipe* NamStreamPipe::head_ = NULL;
int            NamStreamPipe::instances_ = 0;
Tcl_TimerToken NamStreamPipe::timer_ = NULL;

/**********************************************************************
 * Timer handler that checks data availability of all pipes.
 **********************************************************************/
void
NamStreamPipe::timer_handler(ClientData data)
{
	if (read_pipe())
		timer_ = Tcl_CreateTimerHandler(10, timer_handler, NULL);
	else	timer_ = NULL;
}

/**********************************************************************
 * Read currently opened pipes.
 *
 * RETURN: the number of currently opened pipes.
 **********************************************************************/
#ifdef PIPE_BUF
# define BUF_SIZE	PIPE_BUF
#else
# define BUF_SIZE	8192
#endif

int
NamStreamPipe::read_pipe()
{
	NamStreamPipe *p;
	int l, n, fileopen = 0;
	off_t off;
	static char buf[BUF_SIZE];

	for (p = head_; p; p = p->next_) {	// polling for all pipes
		if (p->front_ < 0)
			continue;
		l = ::read(p->front_, buf, BUF_SIZE);
		if (l < 0) {
			fileopen++;
			continue;
		}
		if (l == 0) {	// end of file
			::close(p->front_);
			p->front_ = -1;
			continue;
		}

		// there are data ready for read, copy to backup file
		off = ftell(p->back_);
		fseek(p->back_, 0, SEEK_END);
		n = fwrite(buf, l, 1, p->back_);
		if (n <= 0) {	// fail to write to backup file
			::close(p->front_);
			p->front_ = -1;
			// XXX notify user
			die("NamStreamPipe::read_pipe: tmpfile write problem.\n");
		}
		else {
			p->back_len_ += l;
			fileopen++;
		}
		if (-1 == fseek(p->back_, off, SEEK_SET)) {
			// XXX notify user
			die("NamStreamPipe::read_pipe: fseek problem.\n");
		}
	}
	return fileopen;
}

/**********************************************************************
 * Reentrant function for opening nam pipe stream.
 *
 * RETURN: pointer to an NamStreamPipe instance that is associated
 *         with the specified pipe.
 **********************************************************************/
NamStreamPipe*
NamStreamPipe::open_pipe(const char *fn)
{
	NamStreamPipe *p;

	for (p = head_; p; p = p->next_) {
		if (! strcmp(fn, p->pipename_))
			return p;
	}
	return new NamStreamPipe(fn);
}

NamStreamPipe::NamStreamPipe(const char *fn) :
	NamStream(fn), front_(-1),back_(NULL), back_len_(0),
	pipename_(NULL), prev_(NULL), next_(NULL)
{
#ifndef WIN32
	// Windows doesn't have fcntl() function
	
	// open pipe and temporary file
	if (! strcmp(fn, "-"))	{	// stdin
		int flag = fcntl(0, F_GETFL, 0);
		if (flag < 0)
			goto exception;
		if (fcntl(0, F_SETFL, flag | O_NONBLOCK) < 0)
			goto exception;
		front_ = 0;
	}
	else	front_ = ::open(fn, O_RDONLY | O_NONBLOCK);
	back_ = tmpfile();
#endif

	if ((front_ < 0) || (back_ == NULL))
		goto exception;

	pipename_ = strdup(fn);
	is_open_ = 1;
	next_ = head_;
	if (next_)
		next_->prev_ = this;
	head_ = this;
	instances_++;

	if (! timer_) {	// start the timer if it's idle
		timer_ = Tcl_CreateTimerHandler(10, timer_handler, NULL);
	}
	return;

exception:
	if (front_ > 0)
		::close(front_);
	if (back_)
		fclose(back_);
}

/**********************************************************************
 * Destructor:
 **********************************************************************/
NamStreamPipe::~NamStreamPipe()
{
	fclose(back_);
	if (front_ > 0)
		::close(front_);
	if (next_)
		next_->prev_ = prev_;
	if (prev_)
		prev_->next_ = next_;
	if (pipename_)
		delete pipename_;
	instances_--;
}

/**********************************************************************
 * Read a line. Nam requires the following semantics:
 * RETURN: NULL to indicate end-of-file
 * RETURN: a COMPLETE line, i.e. a complete nam command. It can't return
 *         a partial line, otherwise nam will complain because nam
 *         assumes NamStream is line-buffered.
 * We assume that there always is new data in the pipe. It's the
 * responsibility of the pipe data producer (i.e. the application that
 * generates the nam events to the pipe) to ensure we always have new
 * input. If we have no new input, then we simply block.
 **********************************************************************/
char *
NamStreamPipe::gets(char *buf, int len)
{
	char *ret = buf;
	off_t off = ftell(back_);

	read_pipe();

	/*
	 * Case 1: The pipe is just opened to read, but the other end
	 *         of the pipe has not open it to write. So, read_pipe()
	 *         will close it.
	 *         => Reopen the pipe, and return a fake initial command.
	 * Case 2: Both ends have opened the pipe, but the writer has not
	 *         produced any data.
	 *         => Return a fake initial command.
	 * The fake return value needs to be an initial command. Comment
	 * lines or empty lines are not accepted by nam (e.g. see animator.tcl)
	 *
	 */
	if (back_len_ == 0) {
#ifndef WIN32
		// Windows doesn't have O_NONBLOCK
		if (front_ == -1) {   // it may be closed by read_pipe()
			front_ = ::open(pipename_, O_RDONLY | O_NONBLOCK);
		}
#endif
		strcpy(buf, "V -t * -v 1.0a9 -a 0\n");
		return buf;
	}

	while (1) {
	/*
	 * Case 3: The backup file is over, but there is no new data in pipe.
	 *         => Busy waiting for new input.	XXX
	 *            This is not a good idea. Since nam is a
	 *            single-thread application, busy waiting makes nam
	 *            unable to handle other events (e.g. Window, I/O events).
	 *            The problem here is that we don't know what to return
	 *            when there is no new input from the pipe.
	 *            (see netmodel.cc#NetModel::handle())
	 */
		if (! fgets(buf, len, back_)) {	// EOF backup file
			if (front_ == -1)
				return NULL;
			else	read_pipe();
		}
	/*
	 * Case 4: The backup file is near to over and the last line is
	 *         not complete.
	 *         => Busy waiting for new input.	XXX
	 */
		else {
			int n = strlen(buf) - 1;
			if (buf[n] == '\n')	// here is the normal case
				return buf;
			if (front_ == -1)
				return NULL;
			fseek(back_, off, SEEK_SET);
			read_pipe();
		}
	}
	return ret;
}

/**********************************************************************
 * Read a block of data.
 * XXX Not finished. Don't know what semantics is required by nam.
 **********************************************************************/
int
NamStreamPipe::read(char *buf, int len)
{
	read_pipe();
	return fread(buf, 1, len, back_);
}

/**********************************************************************
 * Read a character.
 * XXX Not finished. Don't know what semantics is required by nam.
 **********************************************************************/
char
NamStreamPipe::get_char()
{
	read_pipe();
	return fgetc(back_);
}

off_t
NamStreamPipe::seek(off_t offset, int whence)
{
	read_pipe();
	if (0 == fseek(back_, offset, whence))
		return ftell(back_);
	else return -1;
}

off_t
NamStreamPipe::tell()
{
	return ftell(back_);
}

int
NamStreamPipe::close()
{
	fseek(back_, 0, SEEK_SET);
	return 0;
}

int
NamStreamPipe::eof()
{
	if (front_ == -1) {
		return feof(back_);
	}
	else return 0;
}
