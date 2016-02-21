## Make file for test source code

CC=g++

## Binary directory containing the executables
BINDIR=obj

## gtest directory
GTEST_DIR = ../../../../../share/googletest/googletest

## all source files in ./ directory or other directories
SOURCE=TestMain.cc \
	UnschedByteAllocatorTest.cc \
	../../src/application/MsgSizeDistributions.cc \
	../../src/application/WorkloadSynthesizer.cc \
	../../src/common/Lagger.cc \
	../../src/common/Util.cc \
    ../../src/transport/HomaConfigDepot.cc \
	../../src/transport/HomaPkt.cc \
	../../src/transport/HomaTransport.cc \
	../../src/transport/PriorityResolver.cc \
	../../src/transport/PseudoIdealPriorityTransport.cc \
	../../src/transport/TrafficPacer.cc \
	../../src/transport/UnschedByteAllocator.cc \
	../../src/transport/WorkloadEstimator.cc \
	../../src/application/AppMessage_m.cc \
	../../src/transport/HomaPkt_m.cc \
	$(GTEST_DIR)/src/gtest-all.cc

## source directories other than current directory 
SRC_DIR=../../src/transport
SRC_DIR1=../../src/application
SRC_DIR2=../../src/common
GTEST_SRC_DIR=$(GTEST_DIR)/src

## All include directories
IFLAGS=-I. \
	-I../../src \
	-I../../../omnetpp-4.6/include \
	-I$(GTEST_DIR) \
	-I../../../inet/src

## compiler flags
CFLAGS += -Wall -g -std=c++0x -fpic -pthread

##preporcessor flags
CPPFLAGS += -isystem $(GTEST_DIR)/include

## contains all library paths in -LPATH format
LFLAGS=-L../../../omnetpp-4.6/lib \
	-L../../../inet/src

## library .a or .so file names are added here
## drop trailing extension and word "lib" from the begining of file name and 
## add -l flag to remaining part
LIBS = -lm -loppcmdenvd -loppcommond -loppenvird \
	-loppeventlogd -lopplayoutd \
	-loppnedxmld -loppscaved -loppsimd \
	-lopptkenvd \
	-lINET


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
	@echo "====================="
	@echo "Linking the target $@"
	@echo "====================="
	@mkdir -p $(dir $(TARGET))
	$(CC) $(CFLAGS) $(CPPFLAGS) $(IFLAGS) $(LFLAGS) $(LIBS) -shared -o $@ $^
	@echo "-----Finished Linking-----"

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
	@echo "\"$(OBJS)\""
	@echo "\"$(SRC_DIR)/obj/\""
	$(CC) $(CFLAGS) $(CPPFLAGS) $(IFLAGS) -c $< -o $@

## Rule for object in other source directory. object files are compiled into
## obj directory under the other source directory.
## you will need one rule like this per "other" directories
$(SRC_DIR)/obj/%.o: $(SRC_DIR)/%.cc
	@mkdir -p $(dir $@)
	@echo "============"
	@echo "compiling $@"
	$(CC) $(IFLAGS) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

$(SRC_DIR1)/obj/%.o: $(SRC_DIR1)/%.cc
	@mkdir -p $(dir $@)
	@echo "============"
	@echo "compiling $@"
	$(CC) $(IFLAGS) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

$(SRC_DIR2)/obj/%.o: $(SRC_DIR2)/%.cc
	@mkdir -p $(dir $@)
	@echo "============"
	@echo "compiling $@"
	$(CC) $(IFLAGS) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

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

## Dependency rule for "other" directory
## dependency .d files are stored under .dep directory in 
## the "other" source directory
$(SRC_DIR)/.dep/%.d: $(SRC_DIR)/%.cc
	@mkdir -p $(dir $@)
	@echo "============"
	@echo "Building dependencies file for $*.o"
	@$(SHELL) -ec '$(CC) -M $(IFLAGS) $(CFLAGS) $(CPPFLAGS) $< | sed "s^$*.o^$(SRC_DIR)/obj/$*.o^" > $@'

$(SRC_DIR1)/.dep/%.d: $(SRC_DIR1)/%.cc
	@mkdir -p $(dir $@)
	@echo "============"
	@echo "Building dependencies file for $*.o"
	@$(SHELL) -ec '$(CC) -M $(IFLAGS) $(CFLAGS) $(CPPFLAGS) $< | sed "s^$*.o^$(SRC_DIR1)/obj/$*.o^" > $@'

$(SRC_DIR2)/.dep/%.d: $(SRC_DIR2)/%.cc
	@mkdir -p $(dir $@)
	@echo "============"
	@echo "Building dependencies file for $*.o"
	@$(SHELL) -ec '$(CC) -M $(IFLAGS) $(CFLAGS) $(CPPFLAGS) $< | sed "s^$*.o^$(SRC_DIR2)/obj/$*.o^" > $@'

$(GTEST_SRC_DIR)/.dep/%.d: $(GTEST_SRC_DIR)/%.cc
	@mkdir -p $(dir $@)
	@echo "============"
	@echo "Building dependencies file for $*.o"
	@$(SHELL) -ec '$(CC) -M $(IFLAGS) $(CFLAGS) $(CPPFLAGS) $< | sed "s^$*.o^$(GTEST_SRC_DIR)/obj/$*.o^" > $@'


## Include the dependency files
-include $(DEPENDS)

.PHONY: clean

clean:
	@-rm -f $(TARGET) $(OBJS) $(DEPENDS)

