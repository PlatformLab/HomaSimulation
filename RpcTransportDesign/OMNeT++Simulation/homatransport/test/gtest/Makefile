## Make file for test source code

CC=g++

## Binary directory containing the executables
BINDIR=obj

## gtest directory
GTEST_DIR = ../../../../../share/googletest/googletest

## all source files in ./ directory or other directories
SOURCE=TestMain.cc \
	UnschedByteAllocatorTest.cc \
	HomaTransportTest.cc \
	$(GTEST_DIR)/src/gtest-all.cc

## source directories other than current directory
GTEST_SRC_DIR=$(GTEST_DIR)/src

#------------------------------------------------------------------------------
############## Copied from the omnet++ auto-generated makefiles# ##############

# Pull in OMNeT++ configuration (Makefile.inc or configuser.vc)

ifneq ("$(OMNETPP_CONFIGFILE)","")
CONFIGFILE = $(OMNETPP_CONFIGFILE)
else
ifneq ("$(OMNETPP_ROOT)","")
CONFIGFILE = $(OMNETPP_ROOT)/Makefile.inc
else
CONFIGFILE = $(shell opp_configfilepath)
endif
endif

ifeq ("$(wildcard $(CONFIGFILE))","")
$(error Config file '$(CONFIGFILE)' does not exist -- add the OMNeT++ bin directory to the path so that opp_configfilepath can be found, or set the OMNETPP_CONFIGFILE variable to point to Makefile.inc)
endif

include $(CONFIGFILE)
#This includes the CFLAGS from omnet++ CONFIGFILE
#------------------------------------------------------------------------------

## All include directories
IFLAGS=-I. \
	-I../../src \
	-I../../../omnetpp-4.6/include \
	-I$(GTEST_DIR) \
	-I../../../inet/src

##preporcessor flags
CPPFLAGS += -isystem $(GTEST_DIR)/include -pthread
CPPFLAGS += $(CXXFLAGS)

## obj directory in the source directory will contain the object files
OBJS=$(join $(addsuffix obj/, $(dir $(SOURCE))), $(notdir $(SOURCE:.cc=.o)))

## .dep dependency directory in source directory will contain make .h
## dependencies information
DEPENDS = $(join $(addsuffix .dep/, $(dir $(SOURCE))), $(notdir $(SOURCE:.cc=.d)))

## The ultimate binary target file
TARGET=$(BINDIR)/libTestMain.so

## default rule
all: $(TARGET)
	@true

## Linking rule for making the target
$(TARGET): $(OBJS)
	@echo "======================"
	@echo "Object files generated"
	@echo "======================"
	@echo "\"$(OBJS)\""

## Generic compiling rule
%.o: %.cc
	@mkdir -p $(dir $@)
	@echo "============"
	@echo "compiling $@"
	$(CC) $(CFLAGS) $(CPPFLAGS) $(IFLAGS) -c $< -o $@

## Rule for object files in the current source directory
## object files are located in obj directory under the source directory
obj/%.o: %.cc
	@mkdir -p $(dir $@)
	@echo "============"
	@echo "compiling $@"
	$(CC) $(CFLAGS) $(CPPFLAGS) $(IFLAGS) -c $< -o $@

$(GTEST_SRC_DIR)/obj/%.o: $(GTEST_SRC_DIR)/%.cc
	@mkdir -p $(dir $@)
	@echo "============"
	@echo "compiling $@"
	$(CC) $(IFLAGS) $(CFLAGS) $(CPPFLAGS) -c $< -o $@


## Make dependancy rules for current source directory
## dependency .d files are stored under .dep directory in
## source directory
.dep/%.d: %.cc
	@mkdir -p $(dir $@)
	@echo "============"
	@echo "Building dependencies file for $*.o"
	@$(SHELL) -ec '$(CC) -M $(IFLAGS) $(CFLAGS) $(CPPFLAGS) $< | sed "s^$*.o^obj/$*.o^" > $@'

$(GTEST_SRC_DIR)/.dep/%.d: $(GTEST_SRC_DIR)/%.cc
	@mkdir -p $(dir $@)
	@echo "============"
	@echo "Building dependencies file for $*.o"
	@$(SHELL) -ec '$(CC) -M $(IFLAGS) $(CFLAGS) $(CPPFLAGS) $< | sed "s^$*.o^$(GTEST_SRC_DIR)/obj/$*.o^" > $@'

## Include the dependency files
-include $(DEPENDS)

.PHONY: clean

clean:
	@-rm -rf $(TARGET) $(OBJS) $(DEPENDS) work

