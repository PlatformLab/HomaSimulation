#
# Copyright (c) 2007 Regents of the SIGNET lab, University of Padova.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. Neither the name of the University of Padova (SIGNET lab) nor the 
#    names of its contributors may be used to endorse or promote products 
#    derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED 
# TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR 
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR 
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, 
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; 
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR 
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF 
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#


AC_DEFUN([AC_ARG_WITH_DEI80211MR],[

DEI80211MR_PATH=''
DEI80211MR_CPPLAGS=''
DEI80211MR_LDFLAGS=''
DEI80211MR_LIBADD=''
DEI80211MR_DISTCHECK_CONFIGURE_FLAGS=''

AC_ARG_WITH([dei80211mr],
	[AS_HELP_STRING([--with-dei80211mr=<directory>],
			[use dei80211mr installation in <directory>, where it is expected to find ns, tcl, otcl and tclcl subdirs])],
	[
		if test "x$withval" != "xno" ; then

   		     if test -d $withval ; then

   			DEI80211MR_PATH="${withval}"

	    		if test ! -f "${DEI80211MR_PATH}/src/mac-802_11mr.h"  ; then
			  	AC_MSG_ERROR([could not find ${withval}/src/mac-802_11mr.h, 
  is --with-dei80211mr=${withval} correct?])
			fi		

			DEI80211MR_CPPFLAGS="$DEI80211MR_CPPFLAGS -I${DEI80211MR_PATH}/src "
			DEI80211MR_CPPFLAGS="$DEI80211MR_CPPFLAGS -I${DEI80211MR_PATH}/src/adt "

			DEI80211MR_LDFLAGS="-L${DEI80211MR_PATH}/src/"
			DEI80211MR_LIBADD="-ldei80211mr"

			DEI80211MR_DISTCHECK_CONFIGURE_FLAGS="--with-dei80211mr=$withval"


   		     else	

			AC_MSG_ERROR([dei80211mr path $withval is not a directory])
	
		     fi

		fi

		AC_SUBST(DEI80211MR_CPPFLAGS)
		AC_SUBST(DEI80211MR_LDFLAGS)
		AC_SUBST(DEI80211MR_LIBADD)
		AC_SUBST(DEI80211MR_DISTCHECK_CONFIGURE_FLAGS)

	])




])









AC_DEFUN([AC_CHECK_DEI80211MR],[

# if test "x$NS_CPPFLAGS" = x ; then
# 	true
# 	AC_MSG_ERROR([NS_CPPFLAGS is empty!])	
# fi

# if test "x$DEI80211MR_CPPFLAGS" = x ; then
# 	true
# 	AC_MSG_ERROR([DEI80211MR_CPPFLAGS is empty!])	
# fi	

# temporarily add NS_CPPFLAGS and DEI80211MR_CPPFLAGS to CPPFLAGS
BACKUP_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="$CPPFLAGS $NS_CPPFLAGS $DEI80211MR_CPPFLAGS"


AC_LANG_PUSH(C++)

AC_MSG_CHECKING([for dei8011mr headers])


AC_PREPROC_IFELSE(
		[AC_LANG_PROGRAM([[
		#include<peerstats.h>
		]],[[
		PeerStats p;
		]]  )],
		[
		 AC_MSG_RESULT([yes])
		 found_dei80211mr=yes
		[$1]
		],
		[
		 AC_MSG_RESULT([no])
		 found_dei80211mr=no
		[$2]
		#AC_MSG_ERROR([could not find dei80211mr])
		])


# AC_PREPROC_IFELSE(
# 		[AC_LANG_PROGRAM([[
# 		#include <mac.h>
# 		#include<mac-802_11mr.h>
# 		]],[[
# 		Mac802_11mr m;
# 		]]  )],
# 		[
# 		 AC_MSG_RESULT([yes])
# 		 found_dei80211mr=yes
# 		],
# 		[
# 		 AC_MSG_RESULT([no])
# 		 found_dei80211mr=no
# 		AC_MSG_ERROR([could not find dei80211mr])
# 		])
	
AM_CONDITIONAL([HAVE_DEI80211MR], [test x$found_dei80211mr = xyes])

# Restoring to the initial value
CPPFLAGS="$BACKUP_CPPFLAGS"

AC_LANG_POP(C++)

])


