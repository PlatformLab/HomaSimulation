/*
 * Copyright (c) 1993-1995 Regents of the University of California.
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
 *      This product includes software developed by the University of
 *      California, Berkeley and the Network Research Group at
 *      Lawrence Berkeley Laboratory.
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
 */

#ifndef lint
static const char rcsid[] =
    "@(#) $Header: /cvsroot/otcl-tclcl/tclcl/Tcl.cc,v 1.76 2007/02/04 01:46:43 tom_henderson Exp $ (LBL)";
#endif

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <tcl.h>

#include "tclcl.h"
#include "tclcl-config.h"
#include "tclcl-internal.h"
#include <sys/types.h>
#include <assert.h>
#include "tracedvar.h"

/* WIN32: Moved tk.h to a point after tclcl.h since tclcl.h includes
 * windows.h which grumbles if I load tk.h before it
 */
#ifndef NO_TK
#include <tk.h>
#endif

#define MAX_CODE_TO_DUMP (8*1024)

class InstVar {
protected:
	InstVar(const char* name);
public:
	virtual ~InstVar();
	InstVar* next_;
	virtual void set(const char*) = 0;
	virtual const char* snget(char *wrk, int wrklen) = 0;
	void init(const char* var);
	inline const char* name() { return name_; }
	inline TracedVar* tracedvar() { return tracedvar_; }
	inline void tracedvar(TracedVar* v) { tracedvar_ = v; }
	static double time_atof(const char* s);
	static double bw_atof(const char* s);
private:
	static char* catch_var(ClientData, Tcl_Interp* tcl,
			       CONST84 char* name1, CONST84 char* name2, int flags);
protected:
	void catch_read(const char* name1, const char* name2);
	void catch_write(const char* name1, const char* name2);
	void catch_destroy(const char* name1, const char* name2);
	const char* name_;
	TracedVar* tracedvar_;
};
#define WRK_SMALL_SIZE 32
#define WRK_MEDIUM_SIZE 256

class TracedVarTcl : public TracedVar {
public:
	TracedVarTcl(const char* name);
	virtual ~TracedVarTcl();
	virtual char* value(char* buf, int buflen);
private:
	static char* catch_var(ClientData, Tcl_Interp* tcl,
			       CONST84 char* name1, CONST84 char* name2, int flags);
protected:
	void catch_write(const char* name1, const char*);
	void catch_destroy(const char* name1, const char*);
	CONST84 char* value_;
};


Tcl Tcl::instance_;

Tcl::Tcl() :
	tcl_(0),
	tkmain_(0),
	application_(0)
{
	Tcl_InitHashTable(&objs_, TCL_STRING_KEYS);
	bp_ = buffer_;
}

void Tcl::init(const char* application)
{
	init(Tcl_CreateInterp(), application);
}

extern EmbeddedTcl et_tclobject;

void Tcl::init(Tcl_Interp* tcl, const char* application)
{
	instance_.tcl_ = tcl;
	instance_.application_ = application;
	et_tclobject.load();
	TclClass::init();
}

TclObject* Tcl::lookup(const char* name)
{
	/*XXX use tcl hash table */
	Tcl_HashEntry* he = Tcl_FindHashEntry(&objs_, (char*)name);
	if (he != 0)
		return ((TclObject*)Tcl_GetHashValue(he));
	return (0);
}

void Tcl::enter(TclObject* o)
{
	int nw;
	Tcl_HashEntry* he = Tcl_CreateHashEntry(&objs_, (char*)o->name(),
						(int*)&nw);
	Tcl_SetHashValue(he, (char*)o);
}

void Tcl::remove(TclObject* o)
{
	Tcl_HashEntry* he = Tcl_FindHashEntry(&objs_, (char*)o->name());
	if (he == 0)
		abort();

	Tcl_DeleteHashEntry(he);
}

void Tcl::evalc(const char* s)
{
	unsigned int n = strlen(s) + 1;
	if (n < sizeof(buffer_) - (bp_ - buffer_)) {
		char* const p = bp_;
		bp_ += n;
		strcpy(p, s);
		eval(p);
		bp_ = p;
	} else {
		char* p = new char[n + 1];
		strcpy(p, s);
		eval(p);
		delete[] p;
	}
}

void Tcl::eval(char* s)
{
	int st = Tcl_GlobalEval(tcl_, s);
	if (st != TCL_OK) {
		int n = strlen(application_) + strlen(s);
		if (n > MAX_CODE_TO_DUMP) {
			s = "\n[code omitted because of length]\n";
			n = strlen(application_) + strlen(s);
		};
		char* wrk = new char[n + 80];
		sprintf(wrk, "tkerror {%s: %s}", application_, s);
		if (Tcl_GlobalEval(tcl_, wrk) != TCL_OK) {
			fprintf(stderr, "%s: tcl error on eval of: %s\n",
				application_, s);
			exit(1);
		}
		delete[] wrk;
		//exit(1);
	}
}

void Tcl::eval()
{
	char* p = bp_;
	bp_ = p + strlen(p) + 1;
	/*XXX*/
	if (bp_ >= &buffer_[1024]) {
		fprintf(stderr, "bailing in Tcl::eval\n");
		assert(0);
		exit(1);
	}
	eval(p);
	bp_ = p;
}

void Tcl::error(const char* s)
{
	if (strlen(s) > MAX_CODE_TO_DUMP) {
		s = "\n[code omitted because of length]\n";
	};
	fprintf(stderr, "%s: \"%s\": %s\n", application_, s, tcl_->result);
	exit(1);
}

/*XXX should be driven from tcl not C...*/
#ifdef notdef
void Tcl::add_option(const char* name, const char* value)
{
	bp_[0] = toupper(application_[0]);
	sprintf(&bp_[1], "%s.%s", application_ + 1, name);
	Tk_AddOption(tkmain_, bp_, (char*)value, TK_USER_DEFAULT_PRIO + 1);
}

void Tcl::add_default(const char* name, const char* value)
{
	bp_[0] = toupper(application_[0]);
	sprintf(&bp_[1], "%s.%s", application_ + 1, name);
	Tk_AddOption(tkmain_, bp_, (char*)value, TK_STARTUP_FILE_PRIO + 1);
}

const char* Tcl::attr(const char* attr) const
{
	bp_[0] = toupper(application_[0]);
	strcpy(&bp_[1], application_ + 1);
	const char* cp = Tk_GetOption(tkmain_, (char*)attr, bp_);
	if (cp != 0 && *cp == 0)
		cp = 0;
	return (cp);
}
#endif


TclObject::TclObject() : instvar_(0), tracedvar_(0)
{
#if 0
	name_[0] = 0;
#else /* ! 0 */
	name_ = NULL;
#endif /* 0 */
}

TclObject::~TclObject()
{
	delete[] name_;
}

int TclObject::dispatch_static_proc(ClientData clientData,
				    Tcl_Interp * /*interp*/,
				    int argc, char *argv[])
{
	int (*proc)(int argc, const char * const *argv);
	proc = (int (*) (int, const char * const *)) clientData;
	return ((*proc)(argc - 2, argv + 2));
}

void TclObject::insert(InstVar* p)
{
	p->next_ = instvar_;
	instvar_ = p;
}

void TclObject::insert(TracedVar* var)
{
	var->owner(this);
	var->next_ = tracedvar_;
	tracedvar_ = var;
}

#ifdef notdef
int TclObject::callback(ClientData cd, Tcl_Interp*, int ac, char** av)
{
	TclObject* tc = (TclObject*)cd;
	return (tc->command(ac, (const char*const*)av));
}
#endif

void TclObject::name(const char* s)
{
#if 0
	// if TCLCL_NAME_LEN we should allow 10^10-1 (almost a billion) objects
	if (strlen(s) >= TCLCL_NAME_LEN)
		abort();
	strcpy(name_, s);
#else /* ! 0 */
	delete[] name_;
	name_ = new char[strlen(s) + 1];
	strcpy(name_, s);
#endif /* 0 */
}

int TclObject::command(int argc, const char*const* argv)
{
#ifdef notdef
	Tcl& t = Tcl::instance();
	char* cp = t.buffer();
	sprintf(cp, "%s: ", t.application());
	cp += strlen(cp);
	const char* cmd = argv[0];
	if (cmd[0] == '_' && cmd[1] == 'o' && class_name_ != 0)
		sprintf(cp, "\"%s\" (%s): ", class_name_, cmd);
	else
		sprintf(cp, "%s: ", cmd);
	cp += strlen(cp);
	if (argc >= 2)
		sprintf(cp, "no such method (%s)", argv[1]);
	else
		sprintf(cp, "requires additional args");

	t.result(t.buffer());
#endif

	if (argc > 2) {
		if (strcmp(argv[1], "trace") == 0) {
			TclObject* tracer = this;
			if (argc > 3)
				tracer = TclObject::lookup(argv[3]);
			return traceVar(argv[2], tracer);
		}
	}
	return (TCL_ERROR);
}

void
TclObject::create_instvar(const char* var)
{
	/*
	 * XXX can't use tcl.evalf() because it uses Tcl_GlobalEval
	 * and we need to run in the context of the method.
	 */
	char wrk[256];
	sprintf(wrk, "$self instvar %s", var);
	Tcl_Eval(Tcl::instance().interp(), wrk);
}

int
TclObject::create_framevar(const char *localName)
{
	/*
	 * XXX can't use tcl.evalf() because it uses Tcl_GlobalEval
	 * and we need to run in the context of the method.
	 * 
	 * XXX Should add a check to see if we already have this framevar.
	 * If so, don't do the following set stuff, otherwise it'll change
	 * the correct value to 0.
	 */
	Tcl_Interp* tcl = Tcl::instance().interp();
	CONST84 char *v = (char *) Tcl_GetVar(tcl, (CONST84 char*)localName, 0);
	if (v != 0)
		return (TCL_OK);
	char wrk[WRK_MEDIUM_SIZE];
	if (-1 == snprintf(wrk, WRK_MEDIUM_SIZE, "set %s 0", localName))
		return TCL_ERROR;
	return Tcl_Eval(Tcl::instance().interp(), wrk);
}

// Enumerating through all traced variables, but still use the trace() 
// callback. 
int TclObject::enum_tracedVars()
{
	for (InstVar* p = instvar_; p != 0; p = p->next_) {
		if (p->tracedvar() && p->tracedvar()->tracer())
			p->tracedvar()->tracer()->trace(p->tracedvar());
	}

	TracedVar* var = tracedvar_;
	for ( ;  var != 0;  var = var->next_) 
		if (var->tracer()) 
			var->tracer()->trace(var);
	return TCL_OK;
}

int TclObject::traceVar(const char* varName, TclObject* tracer)
{
	// first check for delay-bound variables
	int e = delay_bind_dispatch(varName, varName, tracer);
	if (e == TCL_OK)  // was delay-bound and is now taken care of
		return e;

	// now handle regular bound variables
	for (InstVar* p = instvar_; p != 0; p = p->next_) {
		if (strcmp(p->name(), varName) == 0) {
			if (p->tracedvar()) {
				p->tracedvar()->tracer(tracer);
				tracer->trace(p->tracedvar());
				return TCL_OK;
			}
			Tcl& tcl = Tcl::instance();
			tcl.resultf("trace: %s is not a TracedVar", varName);
			return TCL_ERROR;
		}
	}

	TracedVar* var = tracedvar_;
	for ( ;  var != 0;  var = var->next_) {
		if (strcmp(var->name(), varName) == 0) {
			var->tracer(tracer);
			tracer->trace(var);
			return TCL_OK;
		}
	}

	// XXX Introduce the var into OTcl scope before tracing it
	OTclObject *otcl_object = 
		OTclGetObject(Tcl::instance().interp(), name_);
	int result = OTclOInstVarOne(otcl_object, Tcl::instance().interp(),
				     "1", (char *)varName, (char *)varName, 0);
	if (result == TCL_OK) {
		var = new TracedVarTcl(varName);
		insert(var);
		var->tracer(tracer);
		tracer->trace(var);
	}
	return result;
}


void TclObject::trace(TracedVar*)
{
	fprintf(stderr, "SplitObject::trace called in the base class of %s\n",
		name_);
}

void TclObject::msg_abort(const char *fmt, ...)
{
	if (fmt != NULL) {
		va_list ap;
		va_start(ap, fmt);
		vprintf(fmt, ap);
	}
	::abort();
}



TclClass* TclClass::all_;

TclClass::TclClass(const char* classname) : class_(0), classname_(classname)
{
	if (Tcl::instance().interp()!=NULL) {
		// the interpreter already exists!
		// this can happen only (?) if the class is created as part
		// of a dynamic library

		bind();
	} else {
		// the interpreter doesn't yet exist
		// add this class to a linked list that is traversed when
		// the interpreter is created
		
		next_ = all_;
		all_ = this;
	}
}

TclClass::~TclClass()
{
	TclClass** p = &all_;
	while (*p != this && *p != NULL)
		p = &(*p)->next_;
	if (*p != NULL) {
		*p = (*p)->next_;
	}
}

void TclClass::init()
{
	for (TclClass* p = all_; p != 0; p = p->next_)
		p->bind();
}

int TclClass::dispatch_cmd(ClientData clientData, Tcl_Interp *,
			   int argc, CONST84 char *argv[])
{
	TclObject* o = (TclObject*)clientData;
	return (o->command(argc - 3, argv + 3));
}

int TclClass::create_shadow(ClientData clientData, Tcl_Interp *interp,
			    int argc, CONST84 char *argv[])
{
	TclClass* p = (TclClass*)clientData;
	TclObject* o = p->create(argc, argv);
	Tcl& tcl = Tcl::instance();
	if (o != 0) {
		o->name(argv[0]);
		tcl.enter(o);
		if (o->init(argc - 2, argv + 2) == TCL_ERROR) {
			tcl.remove(o);
			delete o;
			return (TCL_ERROR);
		}
		tcl.result(o->name());
		OTclAddPMethod(OTclGetObject(interp, argv[0]), "cmd",
			       (Tcl_CmdProc *) dispatch_cmd, (ClientData)o, 0);
		OTclAddPMethod(OTclGetObject(interp, argv[0]), "instvar",
			       (Tcl_CmdProc *) dispatch_instvar, (ClientData)o, 0);
		o->delay_bind_init_all();
		return (TCL_OK);
	} else {
		tcl.resultf("new failed while creating object of class %s",
			    p->classname_);
		return (TCL_ERROR);
	}
}

/*
 * Sigh.  Much of this routine duplicates
 * OTclOInstVarMethod.  I did that rather than
 * build and eval the appropriate Tcl code to get
 * the control I wanted over calling my superclass's instvar.
 */
int
TclClass::dispatch_instvar(ClientData /*cd*/, Tcl_Interp* in,
			   int argc, CONST84 char *argv[])
{
	int i;
	int result;
	Tcl& tcl = Tcl::instance();
	// XXX:
	// There seems to be something silly about these next two lines.
	// Maybe a TclObject should have a method to return the OTclObject?
	OTclObject *otcl_object = OTclGetObject(in, argv[0]);
	TclObject* tcl_object = tcl.lookup(argv[0]);
	int need_parse = 0;

	for (i = 4; i < argc; i++) {
		int ac;
		CONST84 char **av;
		CONST84 char *varName, *localName;
		if (strcmp(argv[i], "-parse-part1") == 0) {
			need_parse = 1;
			continue;
		};
		result = Tcl_SplitList(in, argv[i], &ac, (const char ***) &av);
		if (result != TCL_OK) break;
		if (ac == 1) {
			varName = localName = av[0];
		} else if (ac == 2) {
			varName = av[0];
			localName = av[1];
		} else {
			Tcl_ResetResult(in);
			Tcl_AppendResult(in, "expected ?inst/local? or ?inst? ?local? but got ",
					 argv[i]);
			ckfree((char*)av);
			result = TCL_ERROR;
			break;
		};
		// handle arrays in instvars if -parse-part1 was specified.
		if (need_parse) {
			const char *p = strchr (localName, '(');
			if (p)
				((char*) localName)[p-localName] = '\0';
		};
		if (TCL_OK != (result = tcl_object->delay_bind_dispatch(varName, localName, NULL)))
			result = OTclOInstVarOne(otcl_object, in, "1", varName, localName, 0);
		ckfree((char*)av);
	}
	return result;
}

int TclClass::delete_shadow(ClientData, Tcl_Interp*,
			   int argc, CONST84 char *argv[])
{
	Tcl& tcl = Tcl::instance();
	if (argc != 4) {
		tcl.result("XXX delete-shadow");
		return (TCL_ERROR);
	}
	TclObject* o = tcl.lookup(argv[0]);
	/*
	 * Delete shadow if it exists.  Shadow might not exist
	 * because of error condition in constructor.
	 */
	if (o != 0) {
		tcl.remove(o);
		delete o;
	}
	return (TCL_OK);
}

void TclClass::bind()
{
	Tcl& tcl = Tcl::instance();
	tcl.evalf("SplitObject register %s", classname_);
	class_ = OTclGetClass(tcl.interp(), (char*)classname_);
	OTclAddIMethod(class_, "create-shadow",
		       (Tcl_CmdProc *) create_shadow, (ClientData)this, 0);
	OTclAddIMethod(class_, "delete-shadow",
		       (Tcl_CmdProc *) delete_shadow, (ClientData)this, 0);
	otcl_mappings();
}

int TclClass::method(int, const char*const*)
{
	/*XXX*/
	return (TCL_ERROR);
}

void TclClass::add_method(const char* name)
{
	OTclAddPMethod((OTclObject*)class_, (char*)name,
		       (Tcl_CmdProc *) dispatch_method, (ClientData)this, 0);
}

int TclClass::dispatch_method(ClientData cd, Tcl_Interp*, int ac, CONST84 char** av)
{
	TclClass* tc = (TclClass*)cd;
	return (tc->method(ac, (const char*const*)av));
}

void EmbeddedTcl::load()
{
	Tcl::instance().evalc(code_);
}

int EmbeddedTcl::load(Tcl_Interp* interp)
{
	return Tcl_Eval(interp, (char*)code_);
}

TclCommand::TclCommand(const char* cmd) : name_(cmd)
{
	Tcl::instance().CreateCommand(cmd, (Tcl_CmdProc *) dispatch_cmd, (ClientData)this, 0);
}

TclCommand::~TclCommand()
{
	Tcl::instance().DeleteCommand(name_);
}

int TclCommand::dispatch_cmd(ClientData clientData, Tcl_Interp*,
			   int argc, CONST84 char *argv[])
{
	TclCommand* o = (TclCommand*)clientData;
	return (o->command(argc, argv));
}


TracedVarTcl::TracedVarTcl(const char* name) : TracedVar(), value_(0)
{
	char* s = new char[strlen(name) + 1];
	strcpy(s, name);
	name_ = s;
	Tcl& tcl = Tcl::instance();
	Tcl_TraceVar(tcl.interp(), (char*)name,
		     TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
		     (Tcl_VarTraceProc *) catch_var, (ClientData)this);
}

TracedVarTcl::~TracedVarTcl()
{
	delete[] (char*)name_;
}

char* TracedVarTcl::value(char* buf, int buflen)
{
	if (buf) {
		if (value_ != NULL)
			strncpy(buf, value_, buflen);
		else 
			buf[0] = 0;
	}
	return buf;
}

void TracedVarTcl::catch_write(const char* name1, const char*)
{
	if (tracer() == 0)
		return;
	Tcl_Interp* tcl = Tcl::instance().interp();
	value_ = (char *) Tcl_GetVar(tcl, (CONST84 char*)name1, 0);
	if (value_ != 0)
		tracer()->trace(this);
}

void TracedVarTcl::catch_destroy(const char* /*name1*/, const char*)
{
	delete this;
}

char* TracedVarTcl::catch_var(ClientData clientData, Tcl_Interp*,
			      CONST84 char* name1, CONST84 char* name2, int flags)
{
	TracedVarTcl* p = (TracedVarTcl*)clientData;
	if (flags & TCL_TRACE_WRITES)
		p->catch_write(name1, name2);
	else if ((flags & TCL_TRACE_UNSETS) && (flags & TCL_TRACE_DESTROYED))
		p->catch_destroy(name1, name2);
	return (0);
}


/*XXX should be easy to extend to arrays*/

char* InstVar::catch_var(ClientData clientData, Tcl_Interp*,
			 CONST84 char* name1, CONST84 char* name2, int flags)
{
	InstVar* p = (InstVar*)clientData;
	if (flags & TCL_TRACE_WRITES)
		p->catch_write(name1, name2);
	else if (flags & TCL_TRACE_READS)
		p->catch_read(name1, name2);
	else if ((flags & TCL_TRACE_UNSETS) && (flags & TCL_TRACE_DESTROYED))
		p->catch_destroy(name1, name2);

	return (0);
}

void InstVar::catch_read(const char* name1, const char* name2)
{
	char wrk[WRK_SMALL_SIZE];
	Tcl_Interp* tcl = Tcl::instance().interp();
	// tcl will copy the value out of wrk
	(void)Tcl_SetVar2(tcl, (char*)name1, (char*)name2, (char*)snget(wrk, WRK_SMALL_SIZE), 0);
}

void InstVar::catch_write(const char* name1, const char*)
{
	Tcl_Interp* tcl = Tcl::instance().interp();
	const char* v = (char *) Tcl_GetVar(tcl, (CONST84 char*)name1, 0);
	if (v != 0)
		set(v);
}

/*
 * catch_destroy only gets called for instvars allocated on the stack
 * (classinstvars).  Regular instvars are deallocated by TclObject.
 * Gee, I hope there's not a race there.
 */
void InstVar::catch_destroy(const char* /*name1*/, const char*)
{
	delete this;
}

InstVar::InstVar(const char* name) : name_(name), tracedvar_(0)
{
	Tcl& tcl = Tcl::instance();
	Tcl_TraceVar(tcl.interp(), (CONST84 char*)name,
		     TCL_TRACE_READS|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
		     (Tcl_VarTraceProc *) catch_var, (ClientData)this);
}

InstVar::~InstVar()
{
	/*XXX do an untrace?*/
}

/*
 * Initialize the instance variable to the value stored in its class;
 * this way we can easily create defaults for each instance
 * variable that used by C.  We call this routine after the
 * trace is set up so that the trace is invoked and the
 * C variable is initialized.
 */
void InstVar::init(const char* var)
{
	char wrk[256];
	sprintf(wrk, "$self init-instvar %s", var);
	if (Tcl_Eval(Tcl::instance().interp(), wrk) != TCL_OK) {
		/*XXX can only happy if TclObject::init-instvar broken */
		Tcl::instance().evalf("puts stderr \"init-instvar: $errorInfo\"");
		exit(1);
	}
}

class InstVarTclObject : public InstVar {
public:
  InstVarTclObject(const char* name, TclObject** val) 
    : InstVar(name), val_(val) {}
  
  const char* snget(char *wrk, int wrklen) {
    if (-1 == snprintf(wrk, wrklen, "%s", (*val_)->name()))
      abort();
    return (wrk);
  }
  void set(const char* s) {
    *val_ = TclObject::lookup(s);
  }
protected:
  TclObject** val_;
};

class InstVarReal : public InstVar {
 public:
  InstVarReal(const char* name, double* val)
		: InstVar(name), val_(val) {}
	const char* snget(char *wrk, int wrklen) {
		if (-1 == snprintf(wrk, wrklen, "%.17g", *val_))
			abort();
		return (wrk);
	}
	void set(const char* s) {
		*val_ = atof(s);
	}
 protected:
	double* val_;
};

class InstVarBandwidth : public InstVarReal {
 public:
	InstVarBandwidth(const char* name, double* val)
		: InstVarReal(name, val) { }
	void set(const char* s) {
		*val_ = bw_atof(s);
	}
};

class InstVarTime : public InstVarReal {
 public:
	InstVarTime(const char* name, double* val)
		: InstVarReal(name, val) { }
	void set(const char* s) {
		*val_ = time_atof(s);
	}
};

class InstVarInt : public InstVar {
 public:
	InstVarInt(const char* name, int* val)
		: InstVar(name), val_(val) {}
	const char* snget(char *wrk, int wrklen) {
		if (-1 == snprintf(wrk, wrklen, "%d", *val_))
			abort();
		return (wrk);
	}
	void set(const char* s) {
		*val_ = strtol(s, (char**)0, 0);
	}
 protected:
	int* val_;
};

class InstVarUInt : public InstVar {
 public:
        InstVarUInt(const char* name, unsigned int* val)
		: InstVar(name), val_(val) {}
	const char* snget(char *wrk, int wrklen) {
		if (-1 == snprintf(wrk, wrklen, "%u", *val_))
			abort();
		return (wrk);
	}
	void set(const char* s) {
		*val_ = strtoul(s, (char**)0, 0);
	}
 protected:
	unsigned int* val_;
};

#if defined(HAVE_INT64)
class InstVarInt64 : public InstVar {
 public:
	InstVarInt64(const char* name, int64_t* val) 
		: InstVar(name), val_(val) {}
	const char* snget(char *wrk, int wrklen) {
		if (-1 == snprintf(wrk, wrklen, 
				   STRTOI64_FMTSTR, *val_))
			abort();
		return (wrk);
	}
	void set(const char* s) {
		*val_ = STRTOI64(s, (char**)0, 0);
	}
 protected:
	int64_t* val_;
};
#endif	

class InstVarBool : public InstVarInt {
 public:
	InstVarBool(const char* var, int* val) : InstVarInt(var, val) {}
	void set(const char* s) {
		int v;

		if (isdigit(*s))
			v = atoi(s);
		else switch (*s) {
		case 't':
		case 'T':
			v =  1;
			break;
		default:
			v = 0;
			break;
		}

		*val_ = v;
	}
};

class InstVarError : public InstVar {
 public:
	InstVarError(const char* name, const char* errmsg)
		: InstVar(name), errmsg_(errmsg) {}
	const char* snget(char *wrk, int wrklen) {
		fprintf(stderr, "\nERROR: %s\n\n", errmsg_);
		abort();
		// To make MSVC happy
		return NULL;
	}
	void set(const char* s) {
		fprintf(stderr, "\nERROR: %s\n\n", errmsg_);
		abort();
	}
 protected:
	const char* errmsg_;
};

class InstVarTracedInt : public InstVar {
 public:
	InstVarTracedInt(const char* name, TracedInt* val) : InstVar(name), val_(val) {
		tracedvar(val);
	}
	const char* snget(char *wrk, int wrklen) {
		return (val_->value(wrk, wrklen));
	}
	void set(const char* s) {
		*val_ = strtol(s, (char**)0, 0);
	}
 protected:
	TracedInt* val_;
};

class InstVarTracedReal : public InstVar {
 public:
	InstVarTracedReal(const char* name, TracedDouble* val) : InstVar(name), val_(val) { 
		tracedvar(val);
	}
	const char* snget(char *wrk, int wrklen) {
		return (val_->value(wrk, wrklen));
	}
	void set(const char* s) {
		*val_ = atof(s);
	}
 protected:
	TracedDouble* val_;
};


double InstVar::bw_atof(const char* s) 
{
	char wrk[32];
	char* cp = wrk;
	while (isdigit(*s) || *s == 'e' || *s == '+' ||
	       *s == '-' || *s == '.')
		*cp++ = *s++;
	*cp = 0;
	double v = atof(wrk);
	switch (s[0]) {
	case 'k':
	case 'K':
		v *= 1e3;
		break;
	case 'm':
	case 'M':
		v *= 1e6;
		break;
	case 'g':
	case 'G':
		v *= 1e9;
		break;
	case 't':
	case 'T':
		v *= 1e12;
		break;
	case 'p':
	case 'P':
		v *= 1e15;
		break;
	}
	if (s[0] != 0 && s[1] == 'B')
		v *= 8;

	return (v);
}

double InstVar::time_atof(const char* s)
{
	char wrk[32];
	char* cp = wrk;
	while (isdigit(*s) || *s == 'e' || *s == '+' || *s == '-' || *s == '.')
		*cp++ = *s++;
	*cp = 0;
	double v = atof(wrk);
	switch (*s) {
	case 'm':
		v *= 1e-3;
		break;
	case 'u':
		v *= 1e-6;
		break;
	case 'n':
		v *= 1e-9;
		break;
	case 'p':
		v *= 1e-12;
		break;
	}
	return (v);
}


void TclObject::init(InstVar* v, const char* var)
{
	insert(v);
	v->init(var);
}

#define TOB(FUNCTION, C_TYPE, INSTVAR_TYPE, OTHER_STUFF) \
	void TclObject::FUNCTION(const char* var, C_TYPE* val) \
	{ \
		  create_instvar(var); \
		  OTHER_STUFF; \
 		  init(new INSTVAR_TYPE(var, val), var); \
	}

TOB(bind, double, InstVarReal, ;)
TOB(bind_bw, double, InstVarBandwidth, ;)
TOB(bind_time, double, InstVarTime, ;)
TOB(bind, int, InstVarInt, ;)
TOB(bind, unsigned int, InstVarUInt, ;)
TOB(bind_bool, int, InstVarBool, ;)
TOB(bind, TclObject*, InstVarTclObject, ;)
TOB(bind, TracedInt, InstVarTracedInt, val->name(var); val->owner(this);)
TOB(bind, TracedDouble, InstVarTracedReal, val->name(var); val->owner(this);)

void TclObject::bind_error(const char* name, const char* errmsg) {
	create_instvar(name);
	insert(new InstVarError(name, errmsg));
}
#if defined(HAVE_INT64)
TOB(bind, int64_t, InstVarInt64, )
#endif

int
TclObject::delay_bind_dispatch(const char* /*varName*/, const char* /*localName*/, TclObject * /*tracer*/)
{
	return TCL_ERROR;  // terminate search
}


void
TclObject::delay_bind_init_all()
{
}

/*
 * sigh... I'd like to call both these functions delay_bind_init,
 * but gcc 2.7.2.3 apparently isn't
 * distinguishing the two based on signature.
 */
void
TclObject::delay_bind_init_one(const char *varName)
{
	char wrk[WRK_MEDIUM_SIZE];
	if (-1 == snprintf(wrk, WRK_MEDIUM_SIZE, "$self init-instvar %s", varName))
		abort();
	if (Tcl_Eval(Tcl::instance().interp(), wrk) != TCL_OK)
		abort();
}

void
TclObject::not_a_TracedVar(const char *varName)
{
	fprintf(stderr, "TclObject: %s is not a TracedVar.\n", varName);
	abort();
}

void
TclObject::handle_TracedVar(const char *name, TracedVar *val, TclObject *tracer)
{
	/*
	 * Remember what the variable is called.
	 * We assume name is a pointer to static storage
	 * and so won't free it.
	 */
	val->name(name);
	// It's not clear that owner is ever used, but we set it anyway
	// for compability.
	val->owner(this);
	// hook the traced var into the tracing system
	val->tracer(tracer);
	tracer->trace(val);
}

/*
 * Traced vars and delay_binding:
 *
 * Without delay-binding, tracevars end up linked into
 * the object's instvar_ and tracedvar_ chains as part of bind().
 * This info is then used in two places:
 * - TclObject::enum_tracedVars (to list them all)
 * - TclObject::traceVar (to see if a given thing we're trying to trace
 *		shoud be, called from TclObject::command's "trace" cmd
 *
 * With delay binding this approach doesn't work, since we don't
 * have InstVar structures in existance at all times.  (This is good---
 * it saves memory.)  Instead, we call delay_bind_dispatch() to
 * search the object hierarchy for a delay-bound variable when a script
 * does "instvar foo_".  This is the only way to search the hierarchy,
 * so we make it also work the trace command.
 *
 * (Design aside:  it would be more elegant to put all the class
 * variables (i.e., the delay_bound ones) in a class-wide hash and do
 * a quick lookup on them through the hash table, like what happens
 * for objects.  Unfortuantely, we can't do this, primarily because we
 * need offsets into objects (like in Xt), but C++ doesn't allow void
 * O::* pointers just typed O::*'s, and C++ O::* pointers are REALLY
 * ugly because they have to handle * multiple inheritance.)
 *
 *
 * There's probably a better way to do this with templates
 * (but we don't allow templates currently for portability).
 *
 * TclObject::delay_bind returns a boolean if it was handled our not
 * see ~ns-2/agent.cc for an example of how to use delay_bind_init_all
 * and delay_bind_dispatch.
 */
#define TODB(FUNCTION, C_TYPE, INSTVAR_TYPE, TRACEDVAR_CODE) \
bool \
TclObject:: FUNCTION (const char* varName, const char* localName, \
		      const char* thisVarName, \
		      C_TYPE *val, TclObject *tracer) \
{ \
	if (strcmp(varName, thisVarName) != 0) return false; \
	if (tracer) { \
		/* traced var request */ \
		TRACEDVAR_CODE; \
	}  else { \
		/* just a binding */ \
		if (TCL_OK != create_framevar(localName)) abort(); \
		(void) (new INSTVAR_TYPE (localName, val)); \
	}; \
	return true; \
}

// These macros are quite ugly in that they the xxx_TracedVars
// reference params of the function above...
TODB(delay_bind, double, InstVarReal, not_a_TracedVar(thisVarName))
TODB(delay_bind_bw, double, InstVarBandwidth, not_a_TracedVar(thisVarName))
TODB(delay_bind_time, double, InstVarTime, not_a_TracedVar(thisVarName))
TODB(delay_bind, int, InstVarInt, not_a_TracedVar(thisVarName))
TODB(delay_bind, unsigned int, InstVarUInt, not_a_TracedVar(thisVarName))
TODB(delay_bind_bool, int, InstVarBool, not_a_TracedVar(thisVarName))
TODB(delay_bind, TracedInt, InstVarTracedInt, handle_TracedVar(thisVarName,val,tracer))
TODB(delay_bind, TracedDouble, InstVarTracedReal, handle_TracedVar(thisVarName,val,tracer))

#if defined(HAVE_INT64)
TODB(delay_bind, int64_t, InstVarInt64, not_a_TracedVar(thisVarName))
#endif

TclObject *
TclObject::New(const char *className, const char * arg1, ...)
{
	Tcl_DString buf;
	const char *string;
	Tcl &tcl = Tcl::instance();
	int result;
	va_list ap;
	
	va_start(ap, arg1);
	Tcl_DStringInit(&buf);
	Tcl_DStringAppendElement(&buf, "new");
	Tcl_DStringAppendElement(&buf, (char*)className);
	string = arg1;
	while (string!=NULL) {
		Tcl_DStringAppendElement(&buf, (char*)string);
		string = va_arg(ap, const char *);
	}
	va_end(ap);

	result = Tcl_Eval(tcl.interp(), Tcl_DStringValue(&buf));
	Tcl_DStringFree(&buf);

	if (result==TCL_ERROR) {
		return NULL;
	}
	else {
		return tcl.lookup(tcl.result());
	}
}


int
TclObject::Delete(TclObject *object)
{
	Tcl &tcl = Tcl::instance();

	if (object->name()==NULL) {
		// this object does not have a corresponding OTcl object
		delete object;
		tcl.result("");
		return TCL_OK;
	}
	return Tcl_VarEval(tcl.interp(), "delete ", object->name(), NULL);
}


int
TclObject::Invoke(const char *method, ...)
{
	Tcl_DString buf;
	const char *string;
	Tcl &tcl = Tcl::instance();
	int result;
	va_list ap;

	if (name()==NULL) {
		// this object does not have a corresponding OTcl object
		tcl.result("no otcl object associated with C++ TclObject");
		tcl.add_error("\ninvoked from withing TclObject::invoke()");
		return TCL_ERROR;
	}
	
	va_start(ap, method);
	Tcl_DStringInit(&buf);
	Tcl_DStringAppendElement(&buf, (char*) name());
	Tcl_DStringAppendElement(&buf, (char*) method);
	while (	(string = va_arg(ap, const char *))!=NULL ) {
		Tcl_DStringAppendElement(&buf, (char*) string);
	}
	va_end(ap);

	result = Tcl_Eval(tcl.interp(), Tcl_DStringValue(&buf));
	Tcl_DStringFree(&buf);
	return result;
}


int
TclObject::Invokef(const char *format, ...)
{
	static char buffer[1024]; /* XXX: individual command should not be
				   * larger than 1023 */

	sprintf(buffer, "%s ", name());
	va_list ap;
	va_start(ap, format);
	vsprintf(&buffer[strlen(buffer)], format, ap);
	return Tcl_Eval(Tcl::instance().interp(), buffer);
}


int
TclArguments::next(const char *&arg)
{
	if (!more_args()) {
		Tcl::instance().result("too few arguments");
		add_error();
		return TCL_ERROR;
	}
	
	arg = argv_[current_++];
	if (arg==NULL) {
		Tcl::instance().result("null argument");
		add_error();
		return TCL_ERROR;
	}
	return TCL_OK;
}


void
TclArguments::add_error() const
{
	Tcl::instance().add_errorf("\ninvoked from within '%s %s'",
				   argv_[0], argv_[1]);
}


int
TclArguments::arg(int &value)
{
	Tcl &tcl = Tcl::instance();
	const char *arg;
	if (next(arg)==TCL_ERROR) {
		return TCL_ERROR;
	}

	if (Tcl_GetInt(tcl.interp(), (char*)arg, &value)==TCL_ERROR) {
		add_error();
		return TCL_ERROR;
	}
	return TCL_OK;
}


int
TclArguments::arg(unsigned int &value)
{
	int iValue;
	if (arg(iValue)==TCL_ERROR) return TCL_ERROR;
	value = (unsigned int) iValue;
	return TCL_OK;
}


int
TclArguments::arg(unsigned short &value)
{
	int iValue;
	if (arg(iValue)==TCL_ERROR) return TCL_ERROR;
	value = (unsigned short) iValue;
	return TCL_OK;
}


int
TclArguments::arg(double &value)
{
	Tcl &tcl = Tcl::instance();
	const char *arg;
	if (next(arg)==TCL_ERROR) {
		return TCL_ERROR;
	}

	if (Tcl_GetDouble(tcl.interp(), (char*) arg, &value)==TCL_ERROR) {
		add_error();
		return TCL_ERROR;
	}
	return TCL_OK;
}


int
TclArguments::arg(TclObject *&value)
{
	Tcl &tcl = Tcl::instance();
	const char *arg;
	if (next(arg)==TCL_ERROR) {
		return TCL_ERROR;
	}

	value = tcl.lookup(arg);
	if (value==NULL) {
		tcl.resultf("Invalid object name '%s'", arg);
		add_error();
		return TCL_ERROR;
	}
	return TCL_OK;
}


int
TclArguments::arg(const char *&value)
{
	if (next(value)==TCL_ERROR) {
		return TCL_ERROR;
	}
	return TCL_OK;
}



