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

SUBDIRS = adt

AUTOMAKE_OPTIONS = subdir-objects

lib_LTLIBRARIES = libdei80211mr.la
check_PROGRAMS = test_power_profile test_per

TESTS = test_power_profile 

libdei80211mr_la_SOURCES = mac-802_11mr.cc mac-802_11mr.h \
			mac-timersmr.cc mac-timersmr.h \
			libinit.cc \
			power_profile.cc power_profile.h \
			wireless-phymr.cc wireless-phymr.h \
			PER.cc PER.h \
			adt/Position.h  adt/Position.cc  \
			adt/Object.h  adt/Object.cc  \
			adt/NSNode.h  adt/NSNode.cc  \
			adt/DLList.h  adt/DLList.cc  \
			wireless-channelpa.h wireless-channelpa.cc  \
			deipropagation.h \
			papropagation.h  papropagation.cc \
			peerstats.cc peerstats.h \
			peerstatsdb_static.cc peerstatsdb_static.h \
			Mac80211EventHandler.h   Mac80211EventHandler.cc \
			arf.h arf.cc \
			rateadapter.cc rateadapter.h \
			phymodes.h \
			ra-snr.cc ra-snr.h \
			rbar.h rbar.cc


libdei80211mr_la_CPPFLAGS = @NS_CPPFLAGS@ -I$(srcdir)/adt
libdei80211mr_la_LDFLAGS = @NS_LDFLAGS@ 
libdei80211mr_la_LIBADD =  @NS_LIBADD@ 

nodist_libdei80211mr_la_SOURCES = InitTcl.cc 

BUILT_SOURCES = InitTcl.cc 

CLEANFILES = InitTcl.cc 

TCL_FILES = multirate-defaults.tcl \
	per_table_80211g_Trivellato.tcl \
	per_table_80211b_intersil_HFA3861B.tcl

InitTcl.cc: Makefile $(TCL_FILES)
		cat $(TCL_FILES) | @TCL2CPP@ InitTclCode > InitTcl.cc

test_power_profile_SOURCES =  power_profile.cc power_profile.h test_power_profile.cc
test_power_profile_CPPFLAGS = @NS_CPPFLAGS@ -I$(srcdir)/adt -DCOMPILING_TEST_PROGRAM
test_power_profile_LDFLAGS = -Wl,--warn-unresolved-symbols

test_per_SOURCES = test_PER.cc \
		adt/Position.h  adt/Position.cc  \
		adt/Object.h  adt/Object.cc  \
		adt/NSNode.h  adt/NSNode.cc  \
		adt/DLList.h  adt/DLList.cc \
		PER.cc PER.h \
		power_profile.cc power_profile.h

test_per_CPPFLAGS = @NS_CPPFLAGS@ -I$(srcdir)/adt -DCOMPILING_TEST_PROGRAM
test_per_LDFLAGS = -Wl,--warn-unresolved-symbols


EXTRA_DIST = $(TCL_FILES)
