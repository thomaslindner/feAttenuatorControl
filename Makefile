# Makefile for PMT test facility
#
# $Id: Makefile 1131 2008-05-22 21:35:42Z olchansk $
# 
# $Log$
#

CFLAGS   = -DOS_LINUX -Dextname -g -O2 -Wall -Wuninitialized -I/home1/midptf/boost_1_47_0/ 
CXXFLAGS = $(CFLAGS)

# MIDAS location

MIDASSYS=$(HOME)/packages/midas

DRV_DIR = $(MIDASSYS)/drivers/bus

MFE       = $(MIDASSYS)/linux/lib/mfe.o
MIDASLIBS = $(MIDASSYS)/linux/lib/libmidas.a
CFLAGS   += -I$(MIDASSYS)/include
CFLAGS   += -I$(MIDASSYS)/drivers/vme
# CFLAGS   += -I$(HOME)/packages/root
CFLAGS   += -I$(DRV_DIR)

# ROOT library

ROOTLIBS  = $(shell $(ROOTSYS)/bin/root-config --libs) -lThread -Wl,-rpath,$(ROOTSYS)/lib
ROOTGLIBS = $(shell $(ROOTSYS)/bin/root-config --glibs) -lThread -Wl,-rpath,$(ROOTSYS)/lib
#CXXFLAGS += -DHAVE_ROOT -DUSE_ROOT -I$(ROOTSYS)/include

# VME interface library

VMICHOME     = $(HOME)/packages/vmisft-7433-3.5-KO2/vme_universe
VMICLIBS     = $(VMICHOME)/lib/libvme.so.3.4 -Wl,-rpath,$(VMICHOME)/lib
#VMICLIBS     = -lvme
CFLAGS      += -I$(VMICHOME)/include

#USE_VMICVME=1
USE_GEFVME=1

ifdef USE_VMICVME
VMICHOME     = $(HOME)/packages/vmisft-7433-3.5-KO2/vme_universe
VMICLIBS     = $(VMICHOME)/lib/libvme.so.3.4 -Wl,-rpath,$(VMICHOME)/lib
#VMICLIBS     = -lvme
CFLAGS      += -I$(VMICHOME)/include
VMEDRV      = vmicvme.o
VMELIBS     = -lvme
endif

ifdef USE_GEFVME
VMEDRV      = gefvme.o
VMELIBS     =
endif


# support libraries

LIBS = -lm -lz -lutil -lnsl -lpthread -lrt

# all: default target for "make"

all:feAttenuator



feAttenuator: $(MIDASLIBS) $(MFE) feAttenuator.o rs232.o
	$(CXX) -o $@ $(CFLAGS)  $^  $(MIDASLIBS) $(LIBS) $(VMELIBS) 


%.o: $(MIDASSYS)/drivers/vme/%.c
	$(CC) -o $@ -c $< $(CFLAGS)

%.o: $(MIDASSYS)/src/%.c
	$(CC) -o $@ -c $< $(CFLAGS)

%.o: %.c
	$(CC) -o $@ -c $< $(CFLAGS)

%.o: %.cxx
	$(CXX) -o $@ -c $< $(CXXFLAGS)

%.o: $(MIDASSYS)/drivers/vme/%.c
	$(CC) -o $@ -c $< $(CFLAGS)


clean::
	-rm -f *.o 

# end
