#!/bin/sh
#
# Merging nam layout file with original nam trace and dump to stdout
#
# $Header: /cvsroot/nsnam/nam-1/bin/merge-layout.sh,v 1.2 2001/04/19 20:14:12 haoboy Exp $

if [ $# -ne 2 ] ; then 
	echo "Usage: merge-layout.sh <orig_trace> <layout>"
	echo "  Merged trace file will be dumped to stdout."
	exit 1
fi

egrep -E '^[nl][[:blank:]]-t[[:blank:]]\*' $2
egrep -v -E '^[nl][[:blank:]]-t[[:blank:]]\*' $1
exit 0
