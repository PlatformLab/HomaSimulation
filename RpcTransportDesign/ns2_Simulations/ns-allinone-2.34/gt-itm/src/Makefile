#
# Makefile for Internetwork Topology Models
#

# $Id: Makefile,v 1.1 1996/10/04 14:01:24 ewz Exp $

# Note: If you are using Solaris, you MUST edit the Makefile to 
#       uncomment the two Solaris lines below, and comment the SunOS line.
# 

LIBS = -lm -lgb
 
BSDF = -DFBSD

IDIR = ../include
LDIR = ../lib
BDIR = ../bin

CFLAGS = -I$(IDIR) -L$(LDIR) $(SYS) $(BSDF)
CC = gcc 

GO = itm.o geog.o ts.o dfs.o
CO = sgb2alt.o 
EO = edriver.o eval.o

GH = $(IDIR)/geog.h $(IDIR)/gb_graph.h $(IDIR)/gb_flip.h
CH = $(IDIR)/geog.h $(IDIR)/gb_graph.h $(IDIR)/gb_save.h
EH = $(IDIR)/gb_graph.h $(IDIR)/gb_save.h $(IDIR)/gb_dijk.h \
     $(IDIR)/eval.h


all:	itm sgb2alt edriver

itm:	$(GO)
	$(CC) $(CFLAGS) -o $(BDIR)/itm $(GO) $(LIBS)

itm.o:	$(GH)

geog.o:	$(GH)

ts.o: $(GH)	

dfs.o: $(GH)

sgb2alt.o: $(CH)

sgb2alt: $(CO)
	$(CC) $(CFLAGS) -o $(BDIR)/sgb2alt $(CO) $(LIBS) 

eval.o: $(IDIR)/gb_graph.h $(IDIR)/gb_dijk.h $(IDIR)/eval.h

edriver.o: $(EH)

edriver: $(EO)
	$(CC) $(CFLAGS) -o $(BDIR)/edriver $(EO) $(LIBS)

clean:  
	\rm -f *.o

veryclean:  clean
	\rm -f $(BDIR)/*

