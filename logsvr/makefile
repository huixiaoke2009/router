CXX = g++
RM = rm -f
CP = cp -f
CXXFLAGS += -Wall -D_GNU_SOURCE  -g -DLINUX -O2 -fPIC
INCS = -I. -I../mmlib
LIBS = ../mmlib/mmlib.a

TARGETS = logsvr

all:$(TARGETS)

logsvr:logsvr_main.o logsvr_proc.o
	$(CXX) $^ $(LIBS) -o $@

%.o: %.cpp
	$(CXX) -c $(CXXFLAGS) $(INCS) $< -o $@

clean:
	$(RM) $(TARGETS) *.o *.exe *.pb.h *.pb.cc

install:
	$(CP) $(TARGETS) /data/knight/logsvr/bin/$(TARGETS)
