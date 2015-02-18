#
# Makefile for Internetwork Topology Models
#

# $Id: Makefile,v 1.1 1996/10/04 14:01:24 ewz Exp $

# Note: If you are using Solaris, you MUST edit the Makefile to 
#       uncomment the two Solaris lines below, and comment the SunOS line.
# 

LIBS = -lm -lgb

GT_ITM = ..
IDIR = $(GT_ITM)/include
LDIR = $(GT_ITM)/lib
BDIR = $(GT_ITM)/bin

CFLAGS = -I$(IDIR) -L$(LDIR) $(SYS)
CC = gcc 

CN = sgb2ns.o
HN = sgb2hierns.o
COMN = sgb2comns.o

CH = $(IDIR)/geog.h $(IDIR)/gb_graph.h $(IDIR)/gb_save.h

all:	sgb2comns sgb2hierns sgb2ns

sgb2ns: $(CN)
	$(CC) $(CFLAGS) -o $(BDIR)/sgb2ns $(CN) $(LIBS) 

sgb2hierns: $(HN)
	$(CC) $(CFLAGS) -o $(BDIR)/sgb2hierns $(HN) $(LIBS)

sgb2comns: $(COMN)
	$(CC) $(CFLAGS) -o $(BDIR)/sgb2comns $(COMN) $(LIBS)

sgb2ns.o: $(CH)
sgb2hierns.o : $(CH)
sgb2comns.o : $(CH)

clean:  
	\rm -f *.o

veryclean:  clean
	\rm -f $(BDIR)/*

