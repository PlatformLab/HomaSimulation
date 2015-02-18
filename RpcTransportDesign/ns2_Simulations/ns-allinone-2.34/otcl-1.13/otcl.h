/* -*- Mode: c++ -*-
 *
 *  $Id: otcl.h,v 1.5 2003/10/12 21:35:00 xuanc Exp $
 *  
 *  Copyright 1993 Massachusetts Institute of Technology
 * 
 *  Permission to use, copy, modify, distribute, and sell this software and its
 *  documentation for any purpose is hereby granted without fee, provided that
 *  the above copyright notice appear in all copies and that both that
 *  copyright notice and this permission notice appear in supporting
 *  documentation, and that the name of M.I.T. not be used in advertising or
 *  publicity pertaining to distribution of the software without specific,
 *  written prior permission.  M.I.T. makes no representations about the
 *  suitability of this software for any purpose.  It is provided "as is"
 *  without express or implied warranty.
 * 
 */

#ifndef _otcl_h_
#define _otcl_h_

#include <tcl.h>

/* compatible char definition for versions < 8.4 */
/* NOTE: tcl8.3.2 defines CONST, but used it in other places...? */
#if TCL_MAJOR_VERSION < 8 || TCL_MAJOR_VERSION == 8 && TCL_MINOR_VERSION < 4
  #define CONST84
  #define CONST84_RETURN
#endif

struct OTclObject;
struct OTclClass;

extern struct OTclObject*
OTclAsObject(Tcl_Interp* in, ClientData cd);

extern struct OTclClass*
OTclAsClass(Tcl_Interp* in, ClientData cd);

extern struct OTclObject*
OTclGetObject(Tcl_Interp* in, CONST84 char* name);

extern struct OTclClass*
OTclGetClass(Tcl_Interp* in, CONST84 char* name);

extern struct OTclObject*
OTclCreateObject(Tcl_Interp* in, CONST84 char* name, struct OTclClass* cl);

extern struct OTclClass*
OTclCreateClass(Tcl_Interp* in, CONST84 char* name, struct OTclClass* cl);

extern int
OTclDeleteObject(Tcl_Interp* in, struct OTclObject* obj);

extern int
OTclDeleteClass(Tcl_Interp* in, struct OTclClass* cl);

extern void
OTclAddPMethod(struct OTclObject* obj, char* nm, Tcl_CmdProc* proc,
	       ClientData cd, Tcl_CmdDeleteProc* dp);

extern void
OTclAddIMethod(struct OTclClass* cl, char* nm, Tcl_CmdProc* proc,
	       ClientData cd, Tcl_CmdDeleteProc* dp);

extern int
OTclRemovePMethod(struct OTclObject* obj, char* nm);

extern int
OTclRemoveIMethod(struct OTclClass* cl, char* nm);

extern int
OTclNextMethod(struct OTclObject* obj, Tcl_Interp* in,
	       int argc, CONST84 char*argv[]);

extern CONST84_RETURN char*
OTclSetInstVar(struct OTclObject* obj, Tcl_Interp* in,
	       CONST84 char* name, CONST84 char* value, int flgs);

extern CONST84_RETURN char*
OTclGetInstVar(struct OTclObject* obj, Tcl_Interp* in,
	       CONST84 char* name, int flgs);

extern int
OTclUnsetInstVar(struct OTclObject* obj, Tcl_Interp* in,
		 CONST84 char* name, int flgs);

extern int
OTclOInstVarOne(struct OTclObject* obj, Tcl_Interp *in, char *frameName, 
		CONST84 char *varName, CONST84 char *localName, int flags);
		 
extern void
OTclSetObjectData(struct OTclObject* obj, struct OTclClass* cl,
		  ClientData data);

extern int
OTclGetObjectData(struct OTclObject* obj, struct OTclClass* cl,
		  ClientData* data);

extern int
OTclUnsetObjectData(struct OTclObject* obj, struct OTclClass* cl);

extern int
Otcl_Init(Tcl_Interp* in);

#endif /* _otcl_h_ */
