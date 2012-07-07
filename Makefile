CXX = clang++
CXXFLAGS = -Wall -O0 -g
COMPILE = $(CXX) $(CXXFLAGS) -o

BOOSTINC = -I/opt/local/include
ZLIB = -lz
IOKIT = -framework IOKit -framework CoreFoundation

PROGRAMS = spirit devices lvmscan

all: $(PROGRAMS)

spirit: spirit.cpp
	$(COMPILE) $@ $^ $(BOOSTINC)

devices: devices.cpp
	$(COMPILE) $@ $^ $(IOKIT)

lvmscan: lvmscan.cpp
	$(COMPILE) $@ $^ $(ZLIB)

.PHONY: all clean

clean:
	rm -rf $(PROGRAMS) *.dSYM
