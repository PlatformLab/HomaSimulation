/*
 * Copyright (c) 1993-1994 Regents of the University of California.
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
 *
 * @(#) $Header: /cvsroot/otcl-tclcl/tclcl/tclcl.h,v 1.33 2005/09/07 04:53:51 tom_henderson Exp $ (LBL)
 */

#ifndef lib_tclcl_h
#define lib_tclcl_h

#include <sys/types.h>
#include <string.h>
#include <tcl.h>
extern "C" {
#include <otcl.h>
}

#include "tclcl-config.h"
#include "tracedvar.h"
// tclcl-mappings.h included below, AFTER definition of class Tcl

struct Tk_Window_;

class Tcl {
    public:
	/* constructor should be private but SGIs C++ compiler complains*/
	Tcl();

	static void init(const char* application);
	static void init(Tcl_Interp*, const char* application);
	static inline Tcl& instance() { return (instance_); }
	inline int dark() const { return (tcl_ == 0); }
	inline Tcl_Interp* interp() const { return (tcl_); }
	
#if TCL_MAJOR_VERSION >= 8
	int evalObj(Tcl_Obj *pObj) { return Tcl_GlobalEvalObj(tcl_, pObj); }
	int evalObjs(int objc, Tcl_Obj **objv) {
		Tcl_Obj* pListObj = Tcl_NewListObj(objc, objv);
		int retcode = evalObj(pListObj);
		Tcl_DecrRefCount(pListObj);
		return retcode; 
	}
	Tcl_Obj* objResult() const { return Tcl_GetObjResult(tcl_); }
	int resultAs(int* pInt) {
		return Tcl_GetIntFromObj(tcl_, objResult(), pInt);
	}
	int resultAs(long* pLong) {
		return Tcl_GetLongFromObj(tcl_, objResult(), pLong);
	}
	int resultAs(double* pDbl) {
		return Tcl_GetDoubleFromObj(tcl_, objResult(), pDbl);
	}
	void result(Tcl_Obj *pObj) { Tcl_SetObjResult(tcl_, pObj); }
	inline const char* result() const { return (char *) Tcl_GetStringResult(tcl_); }
#else /* TCL_MAJOR_VERSION >= 8 */	
	/* may not work at all! */
	inline char* result() const { return (tcl_->result); }
#endif  /* TCL_MAJOR_VERSION >= 8 */
	inline void result(const char* p) { tcl_->result = (char*)p; }
	void resultf(const char* fmt, ...);
	inline void CreateCommand(const char* cmd, Tcl_CmdProc* cproc,
				  ClientData cd = 0,
				  Tcl_CmdDeleteProc* dproc = 0) {
		Tcl_CreateCommand(tcl_, (char*)cmd, cproc, cd, dproc);
	}
	inline void CreateCommand(Tcl_CmdProc* cproc,
				  ClientData cd = 0,
				  Tcl_CmdDeleteProc* dproc = 0) {
		Tcl_CreateCommand(tcl_, buffer_, cproc, cd, dproc);
	}
	inline void DeleteCommand(const char* cmd) {
		Tcl_DeleteCommand(tcl_, (char*)cmd);
	}
	inline void EvalFile(const char* file) {
		if (Tcl_EvalFile(tcl_, (char*)file) != TCL_OK)
			error(file);
	}
	inline const char* var(const char* varname, int flags = TCL_GLOBAL_ONLY) {
		return ((char *) Tcl_GetVar(tcl_, (char*)varname, flags));
	}
	/*
	 * Hooks for invoking the tcl interpreter:
	 *  eval(char*) - when string is in writable store
	 *  evalc() - when string is in read-only store (e.g., string consts)
	 *  [ eval(const char*) is a synonym ]
	 *  evalf() - printf style formatting of command
	 * Or, write into the buffer returned by buffer() and
	 * then call eval(void).
	 */
	void eval(char* s);
	void eval(const char* s) { evalc(s); };
	void evalc(const char* s);
	void eval();
	char* buffer() { return (bp_); }
	/*
	 * This routine used to be inlined, but SGI's C++ compiler
	 * can't hack stdarg inlining.  No big deal here.
	 */
	void evalf(const char* fmt, ...);

	inline void add_error(const char *string) {
		Tcl_AddErrorInfo(interp(), (char *) string);
	}
	void add_errorf(const char *fmt, ...);

	inline struct Tk_Window_* tkmain() const { return (tkmain_); }
	inline void tkmain(struct Tk_Window_* w) { tkmain_ = w; }
	void add_option(const char* name, const char* value);
	void add_default(const char* name, const char* value);
	const char* attr(const char* attr) const;
	const char* application() const { return (application_); }
	inline const char* rds(const char* a, const char* fld) const {
		return (Tcl_GetVar2(tcl_, (char*)a, (char*)fld,
				    TCL_GLOBAL_ONLY));
	}

	TclObject* lookup(const char* name);
	void enter(TclObject*);
	void remove(TclObject*);
    private:
	void error(const char*);

	static Tcl instance_;
	Tcl_Interp* tcl_;
	Tk_Window_* tkmain_;
	char* bp_;
	const char* application_;
	char buffer_[4096];
	Tcl_HashTable objs_;
};

#include "tclcl-mappings.h"

class InstVar;

class TclObject {
    public:
	virtual ~TclObject();
	inline static TclObject* lookup(const char* name) {
		return (Tcl::instance().lookup(name));
	}
	inline const char* name() { return (name_); }
	void name(const char*);
	/*XXX -> method?*/
	virtual int command(int argc, const char*const* argv);
	virtual void trace(TracedVar*);

	void bind(const char* var, TracedInt* val);
	void bind(const char* var, TracedDouble* val);
	void bind(const char* var, double* val);
	void bind_bw(const char* var, double* val);
	void bind_time(const char* var, double* val);
	void bind(const char* var, unsigned int* val);
	void bind(const char* var, int* val);
	void bind_bool(const char* var, int* val);
	void bind(const char* var, TclObject** val);
	void bind_error(const char* var, const char* error);
#if defined(HAVE_INT64)
	void bind(const char* var, int64_t* val);
#endif
	/* give an error message and exit if the old variable 
	   name is used either for read or write */
#define _RENAMED(oldname, newname) \
	bind_error(oldname, "variable "oldname" is renamed to "newname)


	virtual int init(int /*argc*/, const char*const* /*argv*/) {
		return (TCL_OK);
	}

	static TclObject *New(const char *className) {
		return New(className, NULL);
	}
	static TclObject *New(const char *className, const char *arg1, ...);
	static int Delete(TclObject *object);
	int Invoke(const char *method, ...);
	int Invokef(const char *format, ...);
	
	static int dispatch_static_proc(ClientData clientData,
					Tcl_Interp *interp,
					int argc, char *argv[]);

	void create_instvar(const char *var);
	int create_framevar(const char *localName);

	bool delay_bind(const char *varName, const char* localName, const char* thisVarName, double* val, TclObject *tracer);
        bool delay_bind(const char *varName, const char* localName, const char* thisVarName, unsigned int* val, TclObject *tracer);
	bool delay_bind_bw(const char *varName, const char* localName, const char* thisVarName, double* val, TclObject *tracer);
	bool delay_bind_time(const char *varName, const char* localName, const char* thisVarName, double* val, TclObject *tracer);
	bool delay_bind(const char *varName, const char* localName, const char* thisVarName, int* val, TclObject *tracer);
	bool delay_bind_bool(const char *varName, const char* localName, const char* thisVarName, int* val, TclObject *tracer);
	bool delay_bind(const char *varName, const char* localName, const char* thisVarName, TracedInt* val, TclObject *tracer);
	bool delay_bind(const char *varName, const char* localName, const char* thisVarName, TracedDouble* val, TclObject *tracer);

#if defined(HAVE_INT64)
	bool delay_bind(const char *varName, const char* localName, const char* thisVarName, int64_t* val, TclObject *tracer);
#endif

	virtual int delay_bind_dispatch(const char *varName, const char *localName, TclObject *tracer);
	virtual void delay_bind_init_all();
	void delay_bind_init_one(const char *varName);

	// Common interface for all the 'fprintf(stderr,...); abort();' stuff
	static void msg_abort(const char* fmt = NULL, ...);

protected:
	void init(InstVar*, const char* varname);
	TclObject();
	void insert(InstVar*);
	void insert(TracedVar*);
	void not_a_TracedVar(const char *name);
	void handle_TracedVar(const char *name, TracedVar *tv, TclObject *tracer);
	int traceVar(const char* varName, TclObject* tracer);

	// Enumerate through traced vars, and call their corresponding 
	// handlers. 
	int enum_tracedVars(); 

#if 0
	/* allocate in-line rather than with new to avoid pointer and malloc overhead. */
#define TCLCL_NAME_LEN 12
	char name_[TCLCL_NAME_LEN];
#else /* ! 0 */
	char *name_;
#endif /* 0 */
	InstVar* instvar_;
	TracedVar* tracedvar_;
};

/*
 * johnh xxx: delete this
 * #define DELAY_BIND_DISPATCH(VARNAME_P, LOCALNAME_P, VARNAME_STRING, BIND_FUNCTION, PTR_TO_FIELD) \
 *	if (strcmp(VARNAME_P, VARNAME_STRING) == 0) { \
 *		return BIND_FUNCTION(LOCALNAME_P, PTR_TO_FIELD); \
 *	}
 *
 * now standard is:
 * if (delay_bind(varName, localName, "foo_", &foo_, &tv)) return TCL_OK;
 */

class TclClass {
public:
	static void init();
	virtual ~TclClass();
protected:
	TclClass(const char* classname);
	virtual TclObject* create(int argc, const char*const*argv) = 0;
private:
	static int create_shadow(ClientData clientData, Tcl_Interp *interp,
				 int argc, CONST84 char *argv[]);
	static int delete_shadow(ClientData clientData, Tcl_Interp *interp,
				 int argc, CONST84 char *argv[]);
	static int dispatch_cmd(ClientData clientData, Tcl_Interp *interp,
				int argc, CONST84 char *argv[]);
	static int dispatch_init(ClientData clientData, Tcl_Interp *interp,
				 int argc, char *argv[]);
	static int dispatch_instvar(ClientData clientData, Tcl_Interp *interp,
				 int argc, CONST84 char *argv[]);
	static TclClass* all_;
	TclClass* next_;
protected:
	virtual void otcl_mappings() { }
	virtual void bind();
	virtual int method(int argc, const char*const* argv);
	void add_method(const char* name);
	static int dispatch_method(ClientData, Tcl_Interp*, int ac, CONST84 char** av);

	OTclClass* class_;
	const char* classname_;
};

class EmbeddedTcl {
    public:
	inline EmbeddedTcl(const char* code) { code_ = code; }
	void load();
	int load(Tcl_Interp* interp);
        const char* get_code() { return code_; }
    private:
	const char* code_;
};

/*
 * A simple command interface.
 */
class TclCommand {
public:
	virtual ~TclCommand();
protected:
	TclCommand(const char* cmd);
	virtual int command(int argc, const char*const* argv) = 0;
private:
	const char* name_;
	static int dispatch_cmd(ClientData clientData, Tcl_Interp *interp,
				int argc, CONST84 char *argv[]);
};

#endif
