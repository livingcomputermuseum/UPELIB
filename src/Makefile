#++
# Makefile - Makefile for the UPELIB project...
#
#DESCRIPTION:
#   This is a fairly simple Makefile for building the UPE library on Debian
# using GNU Make, gcc (or rather, g++), and the PLX library.  Note that this
# Makefile is a bit unusual in that it does not create any executable - it
# just compiles all the library files and then quits!
#
# Bob Armstrong <bob@jfcl.com>   [1-JUN-2017]
#
#TARGETS:
#  make all	- rebuild UPE library
#  make clean	- delete all generated files 
#
# REVISION HISTORY:
# dd-mmm-yy	who     description
#  1-JUN-17	RLA	New file.
#--

# Compiler preprocessor DEFINEs for the entire project ...
DEFINES = _DEBUG


# Define the PLX library path and options ...
PLXDEFS   = PLX_LITTLE_ENDIAN PLX_LINUX PLX_64BIT
PLXINC    = /usr/local/PlxSdk/Include/


# Define the target (library) and source files required ...
TARGET    = libupe.a
CPPSRCS   = BitStream.cpp CheckpointFiles.cpp CommandLine.cpp \
            CommandParser.cpp ImageFile.cpp LogFile.cpp MessageQueue.cpp \
            Mutex.cpp Thread.cpp StandardUI.cpp LinuxConsole.cpp \
            UPE.cpp UPELIB.cpp
CSRCS	  = SafeCRT.c
INCLUDES  = $(PLXINC)
OBJECTS   = $(CSRCS:.c=.o) $(CPPSRCS:.cpp=.o)


# Define the standard tool paths and options.
#   Note that the CPPFLAGS are for C++, CCFLAGS are for C programs, and
# the CFLAGS variable (only one "C"!) are common to both.  Also, I'm not
# really clear what the difference is, if any, between the "g++" command
# and "gcc -x c++".  I'm assuming they're the same!
CC       = /usr/bin/gcc
AR       = /usr/bin/ar
CPP      = $(CC) -x c++
#removed: -fpack-struct
CPPFLAGS = -std=c++0x
CCFLAGS  = -std=c11 
CFLAGS   = -ggdb3 -O3 -pthread -Wall \
            -funsigned-char -funsigned-bitfields -fshort-enums \
	    $(foreach inc,$(INCLUDES),-I$(inc)) \
	    $(foreach def,$(DEFINES) $(PLXDEFS),-D$(def))


# Rule to rebuild the library ...
all:		$(TARGET)

$(TARGET):	$(OBJECTS)
	@echo Building $(TARGET)
	@rm -f $(TARGET)
	@ar -cq $(TARGET) $(OBJECTS)


# Rules to compile C and C++ files ...
.cpp.o:
	@echo Compiling $<
	@$(CPP) -c $(CPPFLAGS) $(CFLAGS) $<

.c.o:
	@echo Compiling $<
	@$(CC) -c $(CCFLAGS) $(CFLAGS) $<


# A rule to clean up ...
clean:
	rm -f $(TARGET) $(OBJECTS) *~ *.core core Makefile.dep


# And a rule to rebuild the dependencies ...
Makefile.dep: $(CSRCS) $(CPPSRCS)
	$(CC)  -M $(CCFLAGS) $(CFLAGS) $(CSRCS) >Makefile.dep
	$(CPP) -M $(CPPFLAGS) $(CFLAGS) $(CPPSRCS) >>Makefile.dep

include Makefile.dep
