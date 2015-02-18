/* 
 * tclAppInit.c --
 *
 *	Provides a default version of the Tcl_AppInit procedure.
 *
 * Copyright (c) 1993 The Regents of the University of California.
 * Copyright (c) 1994 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef lint
/* static char sccsid[] = "@(#) tclAppInit.c 1.11 94/12/17 16:14:03"; */
#endif /* not lint */

#include <tcl.h>
#include <otcl.h>

#if TCL_MAJOR_VERSION < 7
  #error Tcl distribution TOO OLD
#endif


#ifdef TESTCAPI

#include <stdlib.h>
#include <time.h>

typedef struct {
  time_t started;
  int prior;
} timerdata;


#ifdef STATIC_LIB
#include <tclWinInt.h>
#include <stdlib.h>
extern BOOL APIENTRY
Tcl_LibMain(HINSTANCE hInstance,DWORD reason,LPVOID reserved);

/* procedure to call before exiting to clean up */
void static_exit(void){
	HINSTANCE hInstance=TclWinGetTclInstance();
	Tcl_LibMain(hInstance, DLL_PROCESS_DETACH, NULL);
}
#endif

static int
TimerInit(ClientData cd, Tcl_Interp* in, int argc, char*argv[]) {
  struct OTclObject* timer = OTclAsObject(in, cd);
  struct OTclClass* tcl = OTclGetClass(in, "Timer");
  timerdata* data;

  if (!timer || !tcl) return TCL_ERROR;
  data = (timerdata*)ckalloc(sizeof(timerdata));
  data->started = time(0);
  data->prior = 0;
  (void)OTclSetObjectData(timer, tcl, (ClientData)data);
  if (!OTclSetInstVar(timer, in, "running", "0", TCL_LEAVE_ERR_MSG))
    return TCL_ERROR;
  return OTclNextMethod(timer, in, argc, argv);
}


static int
TimerDestroy(ClientData cd, Tcl_Interp* in, int argc, char*argv[]) {
  struct OTclObject* timer = OTclAsObject(in, cd);
  struct OTclClass* tcl = OTclGetClass(in, "Timer");
  timerdata* data;

  if (!timer || !tcl) return TCL_ERROR;
  if (!OTclGetObjectData(timer, tcl, (ClientData*)(&data))) return TCL_ERROR;

  (void)OTclUnsetObjectData(timer, tcl);
  ckfree(data);
  return OTclNextMethod(timer, in, argc, argv);
}


static int
TimerStart(ClientData cd, Tcl_Interp* in, int argc, char*argv[]) {
  struct OTclObject* timer = OTclAsObject(in, cd);
  struct OTclClass* tcl = OTclGetClass(in, "Timer");
  timerdata* data;

  if (!timer || !tcl || argc>5) return TCL_ERROR;
  if (!OTclGetObjectData(timer, tcl, (ClientData*)(&data))) return TCL_ERROR;  

  if (data->started == 0) data->started = time(0);
  if (!OTclSetInstVar(timer, in, "running", "1", TCL_LEAVE_ERR_MSG))
    return TCL_ERROR;  
  return TCL_OK;
}


static int
TimerRead(ClientData cd, Tcl_Interp* in, int argc, char*argv[]) {
  struct OTclObject* timer = OTclAsObject(in, cd);
  struct OTclClass* tcl = OTclGetClass(in, "Timer");
  timerdata* data;
  char val[20];
  int total;

  if (!timer || !tcl || argc>5) return TCL_ERROR;
  if (!OTclGetObjectData(timer, tcl, (ClientData*)(&data))) return TCL_ERROR;
  
  total = data->prior;
  if (data->started) total += (int)(time(0) - data->started);
  (void)sprintf(val, "%d", total);
  Tcl_SetResult(in, val, TCL_VOLATILE);
  return TCL_OK;
}


static int
TimerStop(ClientData cd, Tcl_Interp* in, int argc, char*argv[]) {
  struct OTclObject* timer = OTclAsObject(in, cd);
  struct OTclClass* tcl = OTclGetClass(in, "Timer");
  timerdata* data;

  if (!timer || !tcl || argc>5) return TCL_ERROR;
  if (!OTclGetObjectData(timer, tcl, (ClientData*)(&data))) return TCL_ERROR;

  if (data->started != 0) {
    data->prior += (int)(time(0) - data->started);
    data->started = 0;
  }
  return TCL_OK;
}


static int
TestCAPI_Init(Tcl_Interp* in) {
  struct OTclClass* class = OTclGetClass(in, "Class");
  struct OTclClass* object = OTclGetClass(in, "Object");
  struct OTclClass* timer;
  struct OTclObject* dawn;
 
  if (!class || !object) return TCL_ERROR;
  timer = OTclCreateClass(in, "Timer", class);
  if (!timer) return TCL_ERROR;
  OTclAddIMethod(timer, "start", TimerStart, 0, 0);
  OTclAddIMethod(timer, "read", TimerRead, 0, 0);
  OTclAddIMethod(timer, "stop", TimerStop, 0, 0);
  OTclAddIMethod(timer, "init", TimerInit, 0, 0);
  OTclAddIMethod(timer, "destroy", TimerDestroy, 0, 0);

  dawn = OTclCreateObject(in, "dawnoftime", timer);
  if (!dawn) return TCL_ERROR;
  if (Tcl_Eval(in, "dawnoftime start") != TCL_OK) return TCL_ERROR;
  return TCL_OK;
}

#endif


/*
 * The following variable is a special hack that is needed in order for
 * Sun shared libraries to be used for Tcl.
 */

#ifdef NEED_MATHERR
extern int matherr();
int *tclDummyMathPtr = (int *) matherr;
#endif

/*
 *----------------------------------------------------------------------
 *
 * main --
 *
 *	This is the main program for the application.
 *
 * Results:
 *	None: Tcl_Main never returns here, so this procedure never
 *	returns either.
 *
 * Side effects:
 *	Whatever the application does.
 *
 *----------------------------------------------------------------------
 */

#if TCL_MAJOR_VERSION == 7 && TCL_MINOR_VERSION < 4

extern int main();
int *tclDummyMainPtr = (int *) main;

#else

int
main(argc, argv)
    int argc;			/* Number of command-line arguments. */
    char **argv;		/* Values of command-line arguments. */
{
#ifdef STATIC_LIB
    HINSTANCE hInstance=GetModuleInstance(NULL);
    Tcl_LibMain(hInstance, DLL_PROCESS_ATTACH, NULL);
    atexit(static_exit);
#endif

    Tcl_Main(argc, argv, Tcl_AppInit);
    return 0;			/* Needed only to prevent compiler warning. */
}

#endif


/*
 *----------------------------------------------------------------------
 *
 * Tcl_AppInit --
 *
 *	This procedure performs application-specific initialization.
 *	Most applications, especially those that incorporate additional
 *	packages, will have their own version of this procedure.
 *
 * Results:
 *	Returns a standard Tcl completion code, and leaves an error
 *	message in interp->result if an error occurs.
 *
 * Side effects:
 *	Depends on the startup script.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_AppInit(interp)
    Tcl_Interp *interp;		/* Interpreter for application. */
{
    if (Tcl_Init(interp) == TCL_ERROR) {
	return TCL_ERROR;
    }

    if (Otcl_Init(interp) == TCL_ERROR) {
      return TCL_ERROR;
    }

#ifdef TESTCAPI
    if (TestCAPI_Init(interp) == TCL_ERROR) {
      return TCL_ERROR;
    }
#endif

#if TCL_MAJOR_VERSION == 7 && TCL_MINOR_VERSION < 5
    tcl_RcFileName = "~/.tclshrc";
#else
    Tcl_SetVar(interp, "tcl_rcFileName", "~/.tclshrc", TCL_GLOBAL_ONLY);
#endif

     return TCL_OK;
}
