/*
 * Copyright (c) @ Regents of the University of California.
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
 * 	This product includes software developed by the MASH Research
 * 	Group at the University of California Berkeley.
 * 4. Neither the name of the University nor of the Research Group may be
 *    used to endorse or promote products derived from this software without
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
 * @(#) $Header: /cvsroot/otcl-tclcl/tclcl/tclcl-mappings.h,v 1.15 2001/08/21 01:26:42 lloydlim Exp $
 */

#ifndef Tclcl_mappings_h
#define Tclcl_mappings_h


class TclObject;
class Tcl;

template <class T>
class TclObjectHelper {
protected:
	typedef int (T::*TMethod)(int argc,const char*const* argv);
	
	static int dispatch_(ClientData clientData, Tcl_Interp * /*interp*/,
		     int argc, char *argv[]) {

		Tcl& tcl = Tcl::instance();
		T *o = (T*) tcl.lookup(argv[0]);
		if (o!=NULL) {
			TMethod *pMethod = (TMethod*) clientData;
			return (o->*(*pMethod))(argc-2, argv+2);
		}
		tcl.resultf("Could not find TclObject for '%s'", argv[0]);
		return TCL_ERROR;
	}
};


class TclArguments {
public:
	TclArguments(int argc, const char * const *argv)
		: current_(2), argc_(argc), argv_(argv) { }
	~TclArguments() { }

	int arg(int &value);
	int arg(unsigned int &value);
	int arg(unsigned short &value);
	int arg(double &value);
	int arg(const char *&value);
	int arg(TclObject *&value);
	
	inline int more_args() const {
		return (current_ < argc_);
	}
	inline int current() const { return current_; }
	void add_error() const;

private:
	int next(const char *&arg);

	int current_, argc_;
	const char * const *argv_;
};



/*-------------------------------------------------------------------------*/


#define DECLARE_OTCLCLASS(classname, otclname)                              \
                                                                            \
static class classname ## _Class : public TclClass,                         \
				   public TclObjectHelper<classname> {      \
public:                                                                     \
	typedef TclObjectHelper<classname> MyHelper;                        \
	typedef classname MyTclObject;                                      \
	                                                                    \
	classname ## _Class() : TclClass(otclname) { }                      \
	TclObject *create(int argc, const char*const* argv);                \
	void otcl_mappings();                                               \
} classname ## _class_obj


/*-------------------------------------------------------------------------*/


#define DEFINE_ABSTRACT_OTCL_CLASS(classname, otclname)                     \
DECLARE_OTCLCLASS(classname, otclname);                                     \
                                                                            \
inline TclObject *classname ## _Class::create(int /*argc*/,                 \
					      const char*const* /*argv*/) { \
        Tcl::instance().resultf("cannot create object of abstract class "   \
				"'%s'", classname_);                        \
	return NULL;                                                        \
}                                                                           \
                                                                            \
void classname ## _Class::otcl_mappings()


/*-------------------------------------------------------------------------*/


#define DEFINE_OTCL_CLASS(classname, otclname)                              \
DECLARE_OTCLCLASS(classname, otclname);                                     \
                                                                            \
inline TclObject *classname ## _Class::create(int /*argc*/,                 \
					      const char*const* /*argv*/) { \
	return (new MyTclObject);			                    \
}                                                                           \
                                                                            \
void classname ## _Class::otcl_mappings()


/*-------------------------------------------------------------------------*/
	

#define INSTPROC(c_method)                                                  \
{                                                                           \
	static MyHelper::TMethod method = &MyTclObject::c_method;           \
	OTclAddIMethod(class_, #c_method, MyHelper::dispatch_,              \
		       (void*) &method, NULL);                              \
}

#define INSTPROC_PUBLIC(c_method)  INSTPROC(c_method)
#define INSTPROC_PRIVATE(c_method) INSTPROC(c_method)


/*-------------------------------------------------------------------------*/


#define PROC(c_static_method)                                               \
{                                                                           \
	int (*method)(int,const char*const*) = &MyTclObject::c_static_method;\
	OTclAddPMethod((OTclObject*)class_, #c_static_method,               \
		       TclObject::dispatch_static_proc,                     \
		       (void*) method, NULL);                               \
}

#define PROC_PUBLIC(c_static_method)   PROC(c_static_method)
#define PROC_PRIVATE(c_static_method)  PROC(c_static_method)


/*-------------------------------------------------------------------------*/


#define BEGIN_PARSE_ARGS(argc, argv)                                        \
TclArguments _args_(argc, argv)


/*-------------------------------------------------------------------------*/


#define END_PARSE_ARGS                                                      \
if (_args_.more_args()) {                                                   \
	Tcl::instance().resultf("Extra arguments (starting at "             \
				"argument %d)", _args_.current()-1);        \
	_args_.add_error();                                                 \
	return TCL_ERROR;                                                   \
}


/*-------------------------------------------------------------------------*/


#define ARG(var)                                                            \
do {                                                                        \
	if (_args_.arg(var)==TCL_ERROR) {                                   \
		return TCL_ERROR;                                           \
	}                                                                   \
} while (0)


/*-------------------------------------------------------------------------*/
	

#define ARG_DEFAULT(var, default)                                           \
do {                                                                        \
    	if (!_args_.more_args()) {                                          \
                var = default;                                              \
	}                                                                   \
	else if (_args_.arg(var)==TCL_ERROR) {                              \
		return TCL_ERROR;                                           \
	}                                                                   \
} while (0)


/*-------------------------------------------------------------------------*/


#endif // Tclcl_mappings_h
	
