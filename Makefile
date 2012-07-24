MACPORTS = /opt/local

CXX = clang++
CXXFLAGS = -Wall -I include -D_FILE_OFFSET_BITS=64 -D__FreeBSD__=10 \
	-I$(MACPORTS)/include
LDFLAGS = -lz -L$(MACPORTS)/lib -lfuse
OPT = -O0 -g

PROGRAMS = test

OBJECTS = test.o dm/target-file.o dm/target-linear.o dm/filedesc.o \
	dm/common.o dm/fuse.o lvm/pvdevice.o lvm/text.o

all: $(PROGRAMS)

test: $(OBJECTS)
	$(CXX) $(LDFLAGS) -o $@ $^

%.o: %.cpp include/*.hpp
	$(CXX) $(CXXFLAGS) $(OPT) -o $@ -c $<

clean:
	rm -rf *.dSYM *.o */*.o $(PROGRAMS)

.PHONY: all clean
