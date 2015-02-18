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
 */
static const char rcsid[] =
    "@(#) $Header: /cvsroot/otcl-tclcl/tclcl/iohandler.cc,v 1.8 1997/12/25 04:21:40 tecklee Exp $ (LBL)";

#include "tclcl.h"
#include "iohandler.h"

#ifdef WIN32
#include "tk.h"
#include <stdlib.h>
#include <winsock.h>
#endif

#ifdef WIN32

#if TK_MAJOR_VERSION >= 8
extern "C" HINSTANCE Tk_GetHINSTANCE();
#else
extern "C" HINSTANCE TkWinGetAppInstance();
#define Tk_GetHINSTANCE() TkWinGetAppInstance()
#endif

LRESULT CALLBACK
IOHandler::WSocketHandler(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	IOHandler* p;

	switch (message) {
	case WM_CREATE:{
		CREATESTRUCT *info = (CREATESTRUCT *) lParam;
		SetWindowLong(hwnd, GWL_USERDATA, (DWORD)info->lpCreateParams);
		return 0;
	}	

	case WM_DESTROY:
		return 0;
	
	case WM_WSOCK_READY:
		p = (IOHandler *) GetWindowLong(hwnd, GWL_USERDATA);
		if (p == NULL) {
			fprintf(stderr, "IOHandler: no GWL_USERDATA\n");
		}
		else {
			p->dispatch(TK_READABLE);
			return 0;
		}
		break;
	}
	return DefWindowProc(hwnd, message, wParam, lParam);
}
#endif

#ifdef WIN32
ATOM IOHandler::classAtom_ = 0;
#endif

IOHandler::IOHandler() : fd_(-1)
#ifdef WIN32
    , hwnd_(0)
#endif
{
#ifdef WIN32
        if (0==classAtom_) {    
                WNDCLASS cl;
                /*
                 * Register the Message window class.
                 */                
                cl.style = CS_HREDRAW | CS_VREDRAW;
                cl.lpfnWndProc = IOHandler::WSocketHandler;
                cl.cbClsExtra = 0;
                cl.cbWndExtra = 0;
                cl.hInstance = Tk_GetHINSTANCE();
                cl.hIcon = NULL;
                cl.hCursor = NULL;
                cl.hbrBackground = NULL;
                cl.lpszMenuName = NULL;
                cl.lpszClassName = "TclCLWSocket";
                classAtom_ = RegisterClass(&cl);
                if (0==classAtom_) {
                        displayErr("register class WSocket");
                }                
        }
#endif
}

IOHandler::~IOHandler()
{
	if (fd_ >= 0)
		unlink();
}

void IOHandler::link(int fd, int mask)
{
	fd_ = fd;
#ifdef WIN32
	int status;
	int flags = 0;
	if (TK_READABLE & mask)
	  flags |= FD_READ;
	if (TK_WRITABLE & mask)
	  flags |= FD_WRITE;

	hwnd_ = CreateWindow("TclCLWSocket", "",
			     WS_POPUP | WS_CLIPCHILDREN,
			     CW_USEDEFAULT, CW_USEDEFAULT, 0, 0,
			     NULL,
			     NULL, Tk_GetHINSTANCE(), this);
        if (hwnd_ == NULL) {
                displayErr("IOHandler::link() CreateWindow");
                exit(1);
        }
//	ShowWindow(hwnd_, SW_HIDE);

        if ((status = WSAAsyncSelect(fd, hwnd_, WM_WSOCK_READY,
				     flags)) > 0) {
                displayErr("IOHandler::link() WSAAsyncSelect");
                exit(1);
        }
#elif TCL_MAJOR_VERSION >= 8
	Tcl_CreateFileHandler(fd,
			      mask, callback, (ClientData)this);
#else
	Tcl_CreateFileHandler(Tcl_GetFile((ClientData)fd, TCL_UNIX_FD),
			      mask, callback, (ClientData)this);
#endif
}

void IOHandler::unlink()
{
#ifdef WIN32
	if (fd_ >= 0 && hwnd_ != 0) {
		/* cancel callback */
		(void) WSAAsyncSelect(fd_, hwnd_, 0, 0);	    
		if (hwnd_) {
			(void) DestroyWindow(hwnd_);
			hwnd_ = 0;
			fd_ = -1;
		}
	}
#else
	if (fd_ >= 0) {
#if TCL_MAJOR_VERSION < 8
		Tcl_DeleteFileHandler(Tcl_GetFile((ClientData)fd_,
						  TCL_UNIX_FD));
#else 
		Tcl_DeleteFileHandler(fd_);  
#endif
		fd_ = -1;
	}
#endif
}

#ifndef WIN32
void IOHandler::callback(ClientData cd, int mask)
{
	IOHandler* p = (IOHandler*)cd;
	p->dispatch(mask);
}
#endif
