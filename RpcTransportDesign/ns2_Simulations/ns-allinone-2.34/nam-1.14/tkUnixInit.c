#ifdef WIN32
#include <windows.h>
#endif

#include <tcl.h>
#include <tk.h>
#include <string.h>

int
#if (TK_MAJOR_VERSION < 8)
TkPlatformInit(Tcl_Interp *interp)
#else
TkpInit(Tcl_Interp *interp)
#endif
{
#ifndef WIN32
	{
		extern void TkCreateXEventSource(void);
		TkCreateXEventSource();
	}
#endif
        return (TCL_OK);
}

#if (TK_MAJOR_VERSION == 8)
void
TkpGetAppName(Tcl_Interp* interp, Tcl_DString* namePtr)
{
  const char *name;
  char *p;
#ifdef WIN32
    int argc;
    char** argv;
#endif

    name = Tcl_GetVar(interp, "argv0", TCL_GLOBAL_ONLY);
#ifdef WIN32
    if (name != NULL) {
	Tcl_SplitPath(name, &argc, &argv);
	if (argc > 0) {
	    name = argv[argc-1];
	    p = strrchr(name, '.');
	    if (p != NULL) {
		*p = '\0';
	    }
	} else {
	    name = NULL;
	}
    }
#endif

    if ((name == NULL) || (*name == 0)) {
	name = "tk";
    }
#ifndef WIN32
    else {
	p = strrchr(name, '/');
	if (p != NULL) {
	    name = p+1;
	}
    }
#endif

    Tcl_DStringAppend(namePtr, name, -1);

#ifdef WIN32
    if (argv != NULL) {
	ckfree((char *)argv);
    }
#endif    
    
}
void
TkpDisplayWarning(char* msg, char* title)
{
#ifdef WIN32
    MessageBox(NULL, msg, title, MB_OK | MB_ICONEXCLAMATION | MB_SYSTEMMODAL
	    | MB_SETFOREGROUND | MB_TOPMOST);
#else
    Tcl_Channel errChannel = Tcl_GetStdChannel(TCL_STDERR);
    if (errChannel) {
	Tcl_Write(errChannel, title, -1);
	Tcl_Write(errChannel, ": ", 2);
	Tcl_Write(errChannel, msg, -1);
	Tcl_Write(errChannel, "\n", 1);
    }
#endif
}
#endif
