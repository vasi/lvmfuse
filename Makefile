CXX = clang++
CXXFLAGS = -Wall -O0 -g
COMPILE = $(CXX) $(CXXFLAGS) -o

BOOSTINC = -I/opt/local/include
ZLIB = -lz
IOKIT = -framework IOKit -framework CoreFoundation
FUSE = -D_FILE_OFFSET_BITS=64 -D__FreeBSD__=10 \
	-I/opt/local/include -L/opt/local/lib -lfuse

PROGRAMS = spirit devices lvmscan crc-bug dm

all: $(PROGRAMS)

spirit: spirit.cpp
	$(COMPILE) $@ $^ $(BOOSTINC)

devices: devices.cpp
	$(COMPILE) $@ $^ $(IOKIT)

lvmscan: lvmscan.cpp
	$(COMPILE) $@ $^ $(ZLIB)

dm: dm.cpp
	$(COMPILE) $@ $^ $(FUSE)

crc-bug: crc-bug.cpp
	$(COMPILE) $@ $^ $(BOOSTINC) $(ZLIB)

qi-adapt-bug: qi-adapt-bug.cpp
	$(COMPILE) $@ $^ $(BOOSTINC)

.PHONY: all clean

clean:
	rm -rf $(PROGRAMS) *.dSYM
