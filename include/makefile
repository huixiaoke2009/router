PROJECT_ROOT = ..
CXX = g++
RM = rm -f
AR = ar -csq
PROTOC = protoc --cpp_out=.
CXXFLAGS += -Wall -D_GNU_SOURCE  -g -DLINUX -O0 -fPIC  
INCS = -I. -I$(PROJECT_ROOT)/mmlib -I/usr/local/include/google/protobuf

SOURCES = $(wildcard *.c) $(wildcard *.cc) $(wildcard *.cpp) $(wildcard *.proto)  
OBJS = $(patsubst %.proto,%.pb.o,$(SOURCES)) 
OBJS += $(patsubst %.cpp,%.o,$(patsubst %.cc,%.o,$(patsubst %.c,%.o,$(SOURCES))))

TARGETS = router.a

all:$(TARGETS)

$(TARGETS):$(OBJS)
	$(AR) $@ $(OBJS)

%.o: %.cpp
	$(CXX) -c $(CXXFLAGS) $(INCS) $< -o $@

%.pb.o: %.pb.cc
	$(CXX) -c $(CXXFLAGS) $(INCS) $< -o $@

%.pb.cc: %.proto
	$(PROTOC) $<

clean:
	$(RM) $(TARGETS) *.o *.exe *.pb.h *.pb.cc