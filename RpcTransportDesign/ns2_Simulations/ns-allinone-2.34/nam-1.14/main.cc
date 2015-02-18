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
 *	This product includes software developed by the Computer Systems
 *	Engineering Group at Lawrence Berkeley Laboratory.
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
 * @(#) $Header: /cvsroot/nsnam/nam-1/main.cc,v 1.57 2003/10/11 22:56:50 xuanc Exp $ (LBL)
 */

#include <stdlib.h>
#ifndef WIN32
#include <unistd.h>
#else
#include <windows.h>
#endif

#include "netview.h"
#include "tclcl.h"
#include "trace.h"
#include "paint.h"
#include "state.h"
#include "parser.h"
 
//#include "../tcl-debug-2.0/tcldbg.h"
 
extern "C" {
#include <tk.h>
}

static void
usage()
{
	fprintf(stderr, "\
Usage: nam [-a -S -s -f init_script -d display -j jump -r rate -k initPort] \
tracefiles\n\
\n\
-a: create a new nam instance\n\
-S: synchronize X\n\
-s: synchronize multiple traces\n\
-j: startup time\n\
-r: initial animation rate\n\
-f: initialization OTcl script\n\
-k: initial socket port number\n");

        exit(1);
}

#ifdef WIN32
extern "C" int getopt(int, char**, char*);
#endif

extern "C" char *optarg;
extern "C" int optind;

const char* disparg(int argc, const char*const* argv, const char* optstr)
{
	const char* display = 0;
	int op;
	while ((op = getopt(argc, (char**)argv, (char*)optstr)) != -1) {
		if (op == 'd') {
			display = optarg;
			break;
		}
	}
	optind = 1;
	return (display);
}

const char* namearg(int argc, const char*const* argv, const char* optstr)
{
	const char* appname = 0;
	int op;
	while ((op = getopt(argc, (char**)argv, (char*)optstr)) != -1) {
		if (op == 'N') {
			appname = optarg;
			break;
		}
	}
	optind = 1;
	return (appname);
}

#include "bitmap/play.xbm"
#include "bitmap/back.xbm"
#include "bitmap/stop.xbm"
#include "bitmap/eject.xbm"
#include "bitmap/rew.xbm"
#include "bitmap/ff.xbm"
#include "bitmap/monitors.xbm"
#include "bitmap/time.xbm"
#include "bitmap/zoomin.xbm"
#include "bitmap/zoomout.xbm"
#include "bitmap/pullright.xbm"
#include "bitmap/mark1.xbm"
#include "bitmap/mark2.xbm"
#include "bitmap/mark3.xbm"
#include "bitmap/mark4.xbm"
#include "bitmap/mark5.xbm"
#include "bitmap/mark6.xbm"
#include "bitmap/mark7.xbm"
#include "bitmap/mark8.xbm"
#include "bitmap/updir.xbm"
//#include "bitmap/edit.xbm"
#include "bitmap/nodeup.xbm" 
#include "bitmap/nodedown.xbm" 
#include "bitmap/select.xbm"
#include "bitmap/addnode.xbm"
#include "bitmap/addlink.xbm"
#include "bitmap/cut.xbm"
#include "bitmap/delete.xbm"
#include "bitmap/netedit.xbm"
#include "bitmap/netview.xbm"

void loadbitmaps(Tcl_Interp* tcl)
{
//  	Tk_DefineBitmap(tcl, Tk_GetUid("edit"),
//  			edit_bits, edit_width, edit_height);
	Tk_DefineBitmap(tcl, Tk_GetUid("netedit"),
			netedit_bits, netedit_width, netedit_height);
	Tk_DefineBitmap(tcl, Tk_GetUid("netview"),
			netview_bits, netview_width, netview_height);
	Tk_DefineBitmap(tcl, Tk_GetUid("nodeup"),
			nodeup_bits, nodeup_width, nodeup_height);
	Tk_DefineBitmap(tcl, Tk_GetUid("nodedown"),
			nodedown_bits, nodedown_width, nodedown_height);

	Tk_DefineBitmap(tcl, Tk_GetUid("play"),
			play_bits, play_width, play_height);
	Tk_DefineBitmap(tcl, Tk_GetUid("back"),
			back_bits, back_width, back_height);
	Tk_DefineBitmap(tcl, Tk_GetUid("stop"),
			stop_bits, stop_width, stop_height);
	Tk_DefineBitmap(tcl, Tk_GetUid("eject"),
			eject_bits, eject_width, eject_height);

	Tk_DefineBitmap(tcl, Tk_GetUid("rew"),
			rew_bits, rew_width, rew_height);
	Tk_DefineBitmap(tcl, Tk_GetUid("ff"),
			ff_bits, ff_width, ff_height);
	Tk_DefineBitmap(tcl, Tk_GetUid("monitors"),
			monitors_bits, monitors_width, monitors_height);
	Tk_DefineBitmap(tcl, Tk_GetUid("time"),
			time_bits, time_width, time_height);
	Tk_DefineBitmap(tcl, Tk_GetUid("zoomin"),
			zoomin_bits, zoomin_width, zoomin_height);
	Tk_DefineBitmap(tcl, Tk_GetUid("zoomout"),
			zoomout_bits, zoomout_width, zoomout_height);
	Tk_DefineBitmap(tcl, Tk_GetUid("pullright"),
			pullright_bits, pullright_width, pullright_height);

  // Used in nam editor toolbar
  Tk_DefineBitmap(tcl, Tk_GetUid("select"),
                  select_bits, select_width, select_height);
  Tk_DefineBitmap(tcl, Tk_GetUid("addnode"),
                  addnode_bits, addnode_width, addnode_height);
  Tk_DefineBitmap(tcl, Tk_GetUid("addlink"),
                  addlink_bits, addlink_width, addlink_height);
  Tk_DefineBitmap(tcl, Tk_GetUid("cut"),
                  cut_bits, cut_width, cut_height);
  Tk_DefineBitmap(tcl, Tk_GetUid("delete"),
                  delete_bits, delete_width, delete_height);

  Tk_DefineBitmap(tcl, Tk_GetUid("mark1"),
                  mark1_bits, mark1_width, mark1_height);
  Tk_DefineBitmap(tcl, Tk_GetUid("mark2"),
                  mark2_bits, mark2_width, mark2_height);
  Tk_DefineBitmap(tcl, Tk_GetUid("mark3"),
                  mark3_bits, mark3_width, mark3_height);
  Tk_DefineBitmap(tcl, Tk_GetUid("mark4"),
                  mark4_bits, mark4_width, mark4_height);
  Tk_DefineBitmap(tcl, Tk_GetUid("mark5"),
                  mark5_bits, mark5_width, mark5_height);
  Tk_DefineBitmap(tcl, Tk_GetUid("mark6"),
                  mark6_bits, mark6_width, mark6_height);
  Tk_DefineBitmap(tcl, Tk_GetUid("mark7"),
                  mark7_bits, mark7_width, mark7_height);
  Tk_DefineBitmap(tcl, Tk_GetUid("mark8"),
                  mark8_bits, mark8_width, mark8_height);
  Tk_DefineBitmap(tcl, Tk_GetUid("updir"),
                  updir_bits, updir_width, updir_height);
}

void adios()
{
	exit(0);
}

static int cmd_adios(ClientData , Tcl_Interp* , int , CONST84 char **)
{
	adios();
	/*NOTREACHED*/
	return (0);
}

extern "C" char version[];

static int cmd_version(ClientData , Tcl_Interp* tcl, int , CONST84 char **)
{
	tcl->result = version;
	return (TCL_OK);
}

char* parse_assignment(char* cp)
{
	cp = strchr(cp, '=');
	if (cp != 0) {
		*cp = 0;
		return (cp + 1);
	} else
		return ("true");
}

static void process_geometry(Tk_Window tk, char* geomArg)
{
	/*
	 * Valid formats:
	 *   <width>x<height>[+-]<x>[+-]<y> or
	 *   <width>x<height> or
	 *   [+-]x[+-]y
	 */
	Tcl &tcl = Tcl::instance();
	// xxx: geomArg could have bogus stuff in it (security hole)
	// but nam doesn't run trusted, so no problem.
	tcl.evalf("wm geometry %s %s", Tk_PathName(tk), geomArg);
	// tclcl will report the error, if any.
}

// What is it used for???
// extern "C" void Blt_Init(Tcl_Interp*);

#ifdef WIN32
EXTERN int platformInit(Tcl_Interp* interp);
#endif

/* TkPlatformInit was moved to tkUnixInit.c */

#if defined(WIN32) && defined(STATIC_LIB)
#include <tkWin.h>
#include <stdlib.h>

extern "C" {
	extern BOOL APIENTRY Tk_LibMain(HINSTANCE hInstance, DWORD reason, 
					LPVOID reserved);
	extern BOOL APIENTRY Tcl_LibMain(HINSTANCE hInstance, DWORD reason, 
					 LPVOID reserved);
	/* procedure to call before exiting to clean up */
	void static_exit(void) {
		HINSTANCE hInstance = Tk_GetHINSTANCE();
		Tcl_LibMain(hInstance, DLL_PROCESS_DETACH, NULL);
		Tk_LibMain(hInstance, DLL_PROCESS_DETACH, NULL);
	}
}
#endif /* defined(WIN32) && defined(STATIC_LIB) */

#ifdef HAVE_LIBTCLDBG
extern "C" {
	extern int Tcldbg_Init(Tcl_Interp *);   // hackorama
}
#endif

void
die(char *s)
{
	fprintf(stderr, "%s", s);
	exit (1);
}

/*ARGSUSED*/
int 
main(int argc, char **argv) {
	const char* script = 0;	// configurations to be loaded
	const char* optstr = "d:M:j:pG:r:u:X:t:i:P:g:N:c:S:f:asmk:zk:xp";
	TraceEvent te;  // Used to display parsetable
	ParseTable pt(&te);  // Used to display parsetable
	/*
	 * We have to initialize libtcl and libtk if we are under Win32
	 * and we are using static version of libtcl8.0p2 and libtk8.0p2.
	 * Because in those distributions, Sun only supports DLL, but 
	 * not static lib. They require initializations in DllMain().
	 * The Berkeley folks (tecklee) built static versions by 
	 * forcing calls to DllMain() inside WinMain(). Because nam 
	 * is built as a console app in win32, we have to do those 
	 * initializations here, in main().
	 */
#if defined(WIN32) && defined(STATIC_LIB)
	HINSTANCE hInstance = GetModuleHandle(NULL);
	Tcl_LibMain(hInstance, DLL_PROCESS_ATTACH, NULL);
	Tk_LibMain(hInstance, DLL_PROCESS_ATTACH, NULL);
	atexit(static_exit);
#endif

	/*
	 * Process display option here before the rest of the options
	 * because it's needed when creating the main application window.
	 */
	const char* display = disparg(argc, argv, optstr);

	// hold pointer to application name for send
	const char* appname = namearg(argc, argv, optstr);
	if (!appname) 
		appname = "nam";
#ifdef notdef
	fprintf(stderr, "Application name is %s\n", appname);
#endif

	Tcl_Interp *interp = Tcl_CreateInterp();
#if 0
	if (Tcl_Init(interp) == TCL_ERROR) {
		printf("%s\n", interp->result);
		abort();
	}
#endif

#if TCL_MAJOR_VERSION < 8
        Tcl_SetVar(interp, "tcl_library", "./lib/tcl7.6", TCL_GLOBAL_ONLY);
        Tcl_SetVar(interp, "tk_library", "./lib/tk4.2", TCL_GLOBAL_ONLY);
#else
        Tcl_SetVar(interp, "tcl_library", "", TCL_GLOBAL_ONLY);
        Tcl_SetVar(interp, "tk_library", "", TCL_GLOBAL_ONLY);
                                                                                
        // this seems just a hack, should NOT have hard-coded library path!
        // why there's no problem with old  TCL/TK?
        // xuanc, 10/3/2003
        //Tcl_SetVar(interp, "tcl_library", "./lib/tcl8.0", TCL_GLOBAL_ONLY);
        //Tcl_SetVar(interp, "tk_library", "./lib/tk8.0", TCL_GLOBAL_ONLY);
#endif

	if (Otcl_Init(interp) == TCL_ERROR) {
		printf("%s\n", interp->result);
		abort();
	}
#ifdef HAVE_LIBTCLDBG
	if (Tcldbg_Init(interp) == TCL_ERROR) {
		return TCL_ERROR;
	}
#endif
	Tcl::init(interp, appname);
	Tcl& tcl = Tcl::instance();

	tcl.evalf(display? "set argv \"-name %s -display %s\"" :
			   "set argv \"-name %s\"", 
		  appname, display, appname);
	Tk_Window tk = 0;
#ifdef WIN32
	Tcl_SetVar(interp, "tcl_library", ".", TCL_GLOBAL_ONLY);
	Tcl_SetVar(interp, "tk_library", ".", TCL_GLOBAL_ONLY);
#endif
	if (Tk_Init(tcl.interp()) == TCL_OK)
		tk = Tk_MainWindow(tcl.interp());
	if (tk == 0) {
		fprintf(stderr, "nam: %s\n", interp->result);
		exit(1);
	}
	tcl.tkmain(tk);

	extern EmbeddedTcl et_tk, et_nam;
	et_tk.load();
	et_nam.load();

	int op;
	int cacheSize = 100;
	char* graphInput = new char[256];
	char* graphInterval = new char[256];
	char* buf = new char[256];
	char* args = new char[256];
	graphInput[0] = graphInterval[0] = buf[0] = args[0] = 0;

	while ((op = getopt(argc, (char**)argv, (char*)optstr)) != -1) {
		switch (op) {
			
		default:
			usage();

		case 'd':
		case 'N':
			/* already handled before */
			break;

/*XXX move to Tcl */
#ifdef notyet
		case 'M':
			tcl.add_option("movie", optarg);
			break;


		case 'p':
			tcl.add_option("pause", "1");
			break;

		case 'G':
			tcl.add_option("granularity", optarg);
			break;

		case 'X': {
			const char* value = parse_assignment(optarg);
			tcl.add_option(optarg, value);
		}
			break;

		case 'P':
			/* Peer name, obsoleted */
			sprintf(buf, "p %s ", optarg);
			strcat(args, buf);
			break;

		case 't':
			/* Use tkgraph. Must supply tkgraph input filename. */
			sprintf(graphInput, "g %s ", optarg);
			strcat(args, graphInput);
			break;
#endif

		case 'a': 
			/* 
			 * Create a whole new instance.
			 */
			strcat(args, "a 1 ");
			break;

		case 'c':
			cacheSize = atoi(optarg);
			break;

		case 'f':
		case 'u':
			script = optarg;
			/* Also pass it to OTcl */
			sprintf(buf, "f %s ", optarg);
			strcat(args, buf);
			break;

		case 'g':
			process_geometry(tk, optarg);
			break;

		case 'i':
			/*
			 * Interval value for graph updates: default is
			 * set by nam_init.
			 */
			sprintf(graphInterval, "i %s ", optarg);
			strcat(args, graphInterval);
			break;

		case 'j':
			/* Initial startup time */
			sprintf(buf, "j %s ", optarg);
			strcat(args, buf);
			break;

		case 'k': 
			/* Initial socket port number */
			sprintf(buf, "k %s ", optarg);
			strcat(args, buf);
			break;
		
		case 'm':
			/* Multiple traces */
			/* no longer needed, but option is still allowed */
			/* for compatibility reasons */
			break;

		case 'r': 
			/* Initial animation rate */
			sprintf(buf, "r %s ", optarg);
			strcat(args, buf);
			break;

		case 's':
			/* synchronize all windows together */
			strcat(args, "s 1 ");
			break;
		case 'z': 
			/* set nam validation test on */
			strcat(args, "z 1 ");
			break;

#ifndef WIN32
		case 'S':
			XSynchronize(Tk_Display(tk), True);
			break;
#endif
		case 'x':
			pt.printLatex(stdout); 
			exit(0);
			break;
		case 'p':
			pt.print(stdout); 
			exit(0);
			break;
		}
	}

	if (strlen(graphInterval) && !strlen(graphInput)) {
		fprintf(stderr, "nam: missing graph input file\n");
		exit(1);
	}

	loadbitmaps(interp);

	char* tracename = NULL;		/* trace file */

	/*
	 * Linux libc-5.3.12 has a bug where if no arguments are processed
	 * optind stays at zero.  Work around that problem here.
	 * The work-around seems harmless in other OSes so it's not ifdef'ed.
	 */
	if ((optind == -1) || (optind == 0))
		optind = 1;

	/*
	 * Make sure the base name of the trace file
	 * was specified.
	 */
	//	if (argc - optind < 1)
	//	usage();

	tracename = argv[optind];
//XXX need to port this
#ifndef WIN32
	if ((tracename != NULL) ) {
		if (access(tracename, R_OK) < 0) {
			tracename = new char[strlen(argv[optind]) + 4];
			sprintf(tracename, "%s.nam", argv[optind]);
		}
	}
#endif

	tcl.CreateCommand("adios", cmd_adios);
	tcl.CreateCommand("version", cmd_version);

#ifdef WIN32
	platformInit(interp);
#endif

	// XXX inappropriate to do initialization in this way?
	FILE *fp = fopen(".nam.tcl", "r");
	if (fp != NULL) {
		fclose(fp);
		tcl.EvalFile(".nam.tcl");
	}

	// User-supplied configuration files
	// option '-u' and '-f' are merged together
	// Evaluation is moved into OTcl
// 	if (script != NULL) {
// 		fp = fopen(script, "r");
// 		if (fp != NULL) {
// 			fclose(fp);
// 			tcl.EvalFile(script);
// 		} else {
// 			fprintf(stderr, "No configuration file %s\n",
// 				script);
// 		}
// 	}

	Paint::init();
	State::init(cacheSize);

	// -- Start TclDebugger
	//Tcldbg_Init(interp);

	tcl.eval("set nam_local_display 0");

	if (tracename != NULL) {
		while (tracename) {

		// Any backslash characters in the filename must be
		// escaped before being passed to Tcl.
			char * new_tracename =
				(char*)malloc(2 * strlen(tracename) + 1);
			char * temp_tracename = new_tracename;
			while (*tracename) {
				if (*tracename == '\\')
					*(temp_tracename++) = '\\';
				*(temp_tracename++) = *tracename;
				++tracename;
			}
			*temp_tracename = 0;
			tracename= new_tracename;


		 // Jump to nam-lib.tcl
			tcl.evalf("nam_init %s %s", tracename, args);
			tracename = argv[++optind];
		}
	} else {
		// Jump to nam-lib.tcl
		tcl.evalf("nam_init \"\" %s", args);
	}
 

	tcl.eval("set nam_local_display");
	if (strcmp(tcl.result(),"1") == 0) {
		Tk_MainLoop();
	}

	return (0);
}
