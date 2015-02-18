/*
 * Copyright (c) 1991,1993 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *  This product includes software developed by the Computer Systems
 *  Engineering Group at Lawrence Berkeley Laboratory.
 * 4. Neither the name of the University nor of the Laboratory may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * @(#) $Header: /cvsroot/nsnam/nam-1/trace.cc,v 1.69 2002/05/03 01:16:39 buchheim Exp $ (LBL)
 */

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <fcntl.h>
#ifdef WIN32
#include <windows.h>
#include <io.h>
#else
#include <unistd.h>
#include <sys/file.h>
#endif
#include <sys/stat.h>
#include <ctype.h>

#include "trace.h"
#include "state.h"
#include "packet.h"
#include "address.h"
#include "nam_stream.h"

extern double time_atof(const char*);

class TraceClass : public TclClass {
 public:
  TraceClass() : TclClass("Trace") {}
  TclObject* create(int argc, const char*const* argv);
} trace_class;

TclObject* TraceClass::create(int argc, const char*const* argv) {
  if (argc != 6)
    return (0);
  /*
   * Open the trace file for reading and read its first line
   * to be able to determine needed values: trace start time,
   * trace duration, etc.
   */
  Trace* tr = new Trace(argv[4], argv[5]);
  if (!tr->valid()) {
    delete tr;
    tr = NULL;
  }
  return (tr);
}

//----------------------------------------------------------------------
// Trace::Trace(const char *fname, const char *animator) 
//   - Create a new tracefile parser and event handler for
//     file "fname" and connect it to the animator object
//----------------------------------------------------------------------
Trace::Trace(const char *fname, const char *animator) : 
  handlers_(0),
  nam_(0),
  skipahead_mode_(0),
  count_(0) {
  
  last_event_time_ = 0.0;

  // Connect to nam animator
  nam_ = (NetworkAnimator *)TclObject::lookup(animator);

  strncpy(fileName_, fname, sizeof(fileName_)-1);
  fileName_[sizeof(fileName_)-1] = 0;
  nam_stream_ = NamStream::open(fileName_);
  if (!nam_stream_->is_ok()) {
    delete nam_stream_;
    nam_stream_ = NULL;
    perror("nam: fdopen");
    // exit(1);
    return; // tr->valid() will check this
  }
  findLastLine();

  /*
   * Go to the beginning of the file, read the first line and
   * save its contents.
   */
  off_t pos = nam_stream_->seek(0, SEEK_SET);
  assert(pos == 0);

  /*
   * Minimum time on the nam time slider is the time of the first
   * trace event.
   */
  mintime_ = nextTime();
  now_ = mintime_;

  State::instance()->setTimes(mintime_, maxtime_);

  // Initialize ParseTable
  parse_table_ = new ParseTable(&pending_);
}

//----------------------------------------------------------------------
// Trace::~Trace()
//----------------------------------------------------------------------
Trace::~Trace() {
  delete parse_table_;
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
int
Trace::valid() {
  return (nam_stream_ && nam_stream_->is_ok());
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
int
Trace::ReadEvent(double now, TraceEvent& e) {
  /*
   * If specified time is after the next event's time, read
   * the next line and return 1. Otherwise, just return 0.
   */
  if (direction_ == FORWARDS) {
    if ((pending_.time != TIME_EOF) && (pending_.time < now)) {
      e = pending_;
      scan();
      return (1);
    }
  } else {
    /*going backwards!*/
    /*
     * two occasions: 1) pending_.time > now, 
     * 2) pending_.time = -1, which means go to the end of file
     */
    if ((pending_.time > now) || (pending_.time == TIME_EOF)) {
      e = pending_;
      scan();
      if (pending_.time == TIME_EOF)
        pending_.time = TIME_BOF;
      return (1);
    }
  }
  return (0);
}

//---------------------------------------------------------------------
// int
// Trace::nextLine()
//
// - This needs to be cleaned up and it may effect other parts of
//   the file parsing code.  It is not very efficient with the amount
//   of seeking going on.  
//---------------------------------------------------------------------
int
Trace::nextLine() {
  char * position;

  if (direction_ == FORWARDS) {
    // moving forward through the trace file
    // We need to read in one line at a time
    nam_stream_->gets(pending_.image, sizeof(pending_.image));

    ++lineno_;
    if (nam_stream_->eof()) {
      Tcl& tcl = Tcl::instance();
      tcl.evalf("%s stopmaxtime %f", nam()->name(), pending_.time);
      return 0;
    }

  } else {
    // moving backwards through the trace file 
    position = nam_stream_->rgets(pending_.image, sizeof(pending_.image));

    lineno_--;
    if (position == NULL ) {
      fprintf(stderr, "nam file reading error.\n");
      return -1; // signal reading error
    }

    if (nam_stream_->tell() == 0) {
      // Beginning of file reached so we need to stop
      return -1;
    }  
  }

  return(nam_stream_->tell());
}

//---------------------------------------------------------------------
// void
// Trace::scan()
//   - scan for the next valid nam event line in file
//   - Once everything becomes stable you can to get rid of all the
//     data type specific scan functions
//---------------------------------------------------------------------
void
Trace::scan() {
  char * time;
  /*
   * Read the next line in the trace file and store its contents
   * depending on line type.
   */
  while (1) {
    TraceEvent & p = pending_;
    time = p.image;

    p.offset = nextLine();

    if (p.offset <= 0) {
      // We reached the beginning of the file
      pending_.time = TIME_EOF;
      // Reset the last event time tracker
      last_event_time_ = 0.0;
      //fprintf(stderr, "EOF Reached.\n");
      return;
    }

    // -----------------------------------------------
    // This -t * line skipping is a hack to
    // enable the wireless code to work properly.
    // All the -t * lines are parsed in tcl files
    // before the animation starts and reparsing them
    // seems to screw up the wireless stuff.
    // ------------------------------------------------
    
    // Skip over -t * lines
    while (*time != '\0') {
      // Find the -t flag
      if (*time == '-') {
        time++;
        if (*time == 't') {
          // Eat the whitespace
          time++;
          while (*time == ' ' || *time == '\t') {
            time++;
          } 

          // We should be pointing to the
          // -t flag value now
          if (*time == '*') {
            break;
          }
        }
      } else {
        time++;
      }
    }
    
    if (*time == '*') {
      // Notify user if their tracefile has initialization events that
      // are mixed in with animation events
      switch (direction_) {
        case FORWARDS:
          // last_event_time_ is initialized to 0.0 since events cannot happen before this time.
          if (last_event_time_ != 0.0) {
            fprintf(stderr, "Warning: Initialization event is mixed in with animation events.\n");
            fprintf(stderr, "%s", p.image);
            fprintf(stderr, "The above event should occur before all -t <time> animation events.\n\n");
            fprintf(stderr, "last_time_event_ %f\n", last_event_time_);
          }
          break;
      }
      last_event_time_ = 0.0;

      if (p.offset <= 0) {
	      pending_.time = TIME_EOF;
	      //fprintf(stderr, "EOF Reached.\n");
	      return;
      }
      continue;
    } 

    if (parse_table_->parseLine(p.image)) {
      switch (direction_) {
        // Notify user if their tracefile has records that are out of order
        case FORWARDS:
          if (pending_.time < last_event_time_) {
            fprintf(stderr, "Warning: Tracefile events are not sorted by time.\n");
            fprintf(stderr, "%s", p.image);
            fprintf(stderr, "The above event should occur at or after -t %f.\n\n",
                            last_event_time_);
          }
          break;
        case BACKWARDS:
          if (pending_.time > last_event_time_ && last_event_time_ != 0.0) {
            fprintf(stderr, "Warning: Tracefile events are not sorted by time.\n");
            fprintf(stderr, "%s", p.image);
            fprintf(stderr, "The above event should occur at or before -t %f.\n\n",
                            last_event_time_);
          }
          break;
      }
      last_event_time_ = pending_.time;
      return;

    } else {
      fprintf(stderr, "Parsing error in event.\n");
    }
    
  } // end while
  /* NOTREACHED */
}



//----------------------------------------------------------------------
// void
// Trace::findLastLine()
//   - Go to the specified trace file's last line, verify that
//     the file isn't empty and that its last line is correctly
//     formatted. Compute the time interval for the entire trace
//     assuming it started at time 0.
//----------------------------------------------------------------------
void Trace::findLastLine() {
  off_t pos;

  // should initialize it
  direction_ = FORWARDS;
  pending_.time = 0.0;
  pending_.image[0] = 0;

  /*
   * Fake it?
   */
  if (!nam_stream_->seekable()) {
    maxtime_ = 0.0;
    return;
  }

  /*
   * If we can, find the end of time.
   */
  char buf[TRACE_LINE_MAXLEN+1];

  /* is there stuff in the file */
  nam_stream_->seek(0, SEEK_END);
  if (nam_stream_->tell() <= 1) {
    fprintf(stderr, "nam: empty trace file `%s'.", fileName_);
    exit(1);
  }

  /*
   * If the file is larger than 'buf', go to the end of the file
   * at the point just large enough to fit into 'buf'.
   */
  if (nam_stream_->tell() > (off_t)(sizeof(buf) - 1)) {
    pos = nam_stream_->seek(- (sizeof(buf) - 1), SEEK_CUR);
    assert(pos != -1);
  } else 
    /* If the file is smaller than 'buf', go to the head of file */
    nam_stream_->seek(0, SEEK_SET);

  int n = nam_stream_->read(buf, sizeof(buf) - 1);
  if (n < 0) {
    perror("read");
    exit(1);
  }
  buf[n] = 0;

  /*
   * This next check is bogus, but zlib fails it.
   * Go figure.
   * Fortunately compressed files should not be seekable.
   */
  pos = nam_stream_->tell();
  assert(pos != -1);

  /* Error if the last line doesn't have '\n' in it. */
  char *cp = strrchr(buf, '\n');
  if (cp == 0) {
    fprintf(stderr, "nam: error, missing newline on the last line of `%s'.",
      fileName_);
    exit(1);
  }

  /*
   * Look for the first '\n' and check if the line following
   * it has the correct type, i.e., it starts with any of the
   * characters in [h+-dv].
   */
  *cp = 0;
  cp = strrchr(buf, '\n');
  if (cp == 0 || strchr("h+-dvrRanlfDEGPAmVNXWT", *++cp) == 0) {
    fprintf(stderr, "nam: no dynamic animation events in `%s'.\n", fileName_);
    maxtime_ = 0.0;
    return;
    //fprintf(stderr, "     Unknown event %s\n", buf);
    //exit(1);
  }

  /*
   * Compute the time interval from the beginning of the trace
   * to the end.
   * XXX this should include the duration of the last event but
   *     since we changed the format of 'size' from 'time' to
   *     'bytes', we can no longer figure this out.
   */
  double time = 0.;
  (void)sscanf(++cp, " -t %lg", &time);
  maxtime_ = time;
}

/* Go to the beginning of the file, read the first line and process it. */
void Trace::rewind(long offset)
{
  nam_stream_->seek(offset, SEEK_SET);
  //lineno_ = 0;
  scan();
}

/*
 * Put the specified trace handler to the beginning of the trace 
 * handler list.
 */
void Trace::addHandler(TraceHandler* th)
{
  TraceHandlerList* p = new TraceHandlerList;
  p->th = th;
  p->next = handlers_;
  handlers_ = p;
}

//----------------------------------------------------------------------
// void
// Trace::settime(double now, int timeSliderClicked)
//   - Set the current trace time to 'now'.
//----------------------------------------------------------------------
void Trace::settime(double now, int timeSliderClicked) {
  if (now < now_) {
#ifdef OLDWAY
    for (TraceHandlerList* p = handlers_; p != 0; p = p->next)
      p->th->reset(now);
    StateInfo si;
    State::instance()->get(now, si);
    rewind(si.offset);
#endif
    direction_=BACKWARDS;
  } else {
    direction_=FORWARDS;
  }

  now_ = now;

  /*
   * Scan the trace file until the event occurring at time 'now'
   * is read. For each event scanned, pass it on to all trace
   * handlers in the trace handler list so they can handle
   * them as needed.
   */
  Tcl& tcl = Tcl::instance();
  Tcl_Interp* interp = tcl.interp();
  static char event[128];
  TraceEvent e;

  while (ReadEvent(now, e)) {
     //if (e.time == TIME_BOF) {
     if (pending_.time == TIME_BOF) {
       // Reached the beginning of file during backtracking
       // Rewind to the beginning
       direction_ = FORWARDS;
       rewind(0);
       continue;
     }

    if (skipahead_mode_) {
      count_ = 0;
      skipahead_mode_ = 0;
      tcl.evalf("%s normalspeed", nam()->name());
    }
     
    // Find Handler for this pending event?
    for (TraceHandlerList* p = handlers_; p != 0; p = p->next)
      p->th->handle(e, now, direction_);

    if (e.tt == 'h' && (e.pe.src == 0 || e.pe.dst == 0)) {
      sprintf(event, "%d %d %.6f %d/", e.pe.src, e.pe.dst, 
                                       e.time, e.pe.pkt.id);
      if (timeSliderClicked)
        tcl.result((const char *)event);
      else
        Tcl_AppendResult(interp, event, 0);
    }
  }

  for (TraceHandlerList* p = handlers_; p != 0; p = p->next)
    p->th->update(now);

  if (Packet::count() == 0 && ++count_ == 2) {
    count_ = 0;
    skipahead_mode_ = 1;
    tcl.evalf("%s speedup %g", nam()->name(), pending_.time);
  }

}

/*
 * Process 'nam trace' commands:
 *   mintime - return the trace start time
 *   maxtime - return the trace duration
 *   connect - add the specified trace handler to the trace handler list
 *   settime - reset current time to specified time and adjust the
 *             display as needed
 *   nxtevent - read next event in trace file and animate from that point
 *   reset    - reset nam tracefile reading
 */
int Trace::command(int argc, const char* const* argv)
{
  Tcl& tcl = Tcl::instance();

  if (argc == 2) {
    if (strcmp(argv[1], "mintime") == 0) {
      char* bp = tcl.buffer();
      sprintf(bp, "%g", mintime_);
      tcl.result(bp);
      return (TCL_OK);
    } else if (strcmp(argv[1], "maxtime") == 0) {
      char* bp = tcl.buffer();
      sprintf(bp, "%g", maxtime_);
      tcl.result(bp);
      return (TCL_OK);
    } else if (strcmp(argv[1], "nxtevent") == 0) {
      char* bp = tcl.buffer();
      sprintf(bp, "%g", pending_.time);
      tcl.result(bp);
      return (TCL_OK);
    } else if (strcmp(argv[1], "reset") == 0) {
      nam_stream_->close();
      nam_stream_ = NULL;
      nam_stream_ = NamStream::open(fileName_);
      if (nam_stream_->is_ok()) {
        delete nam_stream_;
        nam_stream_ = NULL;
        perror("nam: fdopen");
        // exit(1);
        return TCL_ERROR; // tr->valid() will check this
      }
                        return (TCL_OK);
    }
  }
  if (argc == 3) {
    if (strcmp(argv[1], "connect") == 0) {
      /*
       * Get the handler for the specified trace and
       * add it to the trace's list of handlers.
       */
      TraceHandler* th = (TraceHandler*)TclObject::lookup(argv[2]);
      if (th == 0) {
        tcl.resultf("nam connect: no such trace %s",
              argv[2]);
        return (TCL_ERROR);
      }
      addHandler(th);
      return (TCL_OK);
    }
  }
  if (argc == 4 ) {
    if (strcmp(argv[1], "settime") == 0) {
      /*
       * Set current time to the specified time and update
       * the display to reflect the new current time.
       */
      double now = atof(argv[2]);
      int timeSliderClicked = atoi(argv[3]);
      settime(now, timeSliderClicked);
      return (TCL_OK);
    }
  }
  return (TclObject::command(argc, argv));
}
