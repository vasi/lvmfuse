CXX = clang++
CXXFLAGS = -Wall -O0 -g
COMPILE = $(CXX) $(CXXFLAGS) -o

MACPORTS = /opt/local
BOOSTINC = -I$(MACPORTS)/include
TOMCRYPT = -I$(MACPORTS)/include -L$(MACPORTS)/lib -ltomcrypt
ZLIB = -lz
IOKIT = -framework IOKit -framework CoreFoundation
FUSE = -D_FILE_OFFSET_BITS=64 -D__FreeBSD__=10 \
	-I$(MACPORTS)/include -L$(MACPORTS)/lib -lfuse
OPENSSL = -lcrypto -Wno-deprecated-declarations

PROGRAMS = spirit devices lvmscan crc-bug dm crypt luks parse openssl

all: $(PROGRAMS)

spirit: spirit.cpp
	$(COMPILE) $@ $^ $(BOOSTINC)

devices: devices.cpp
	$(COMPILE) $@ $^ $(IOKIT)

lvmscan: lvmscan.cpp
	$(COMPILE) $@ $^ $(ZLIB)

luks: luks.cpp
	$(COMPILE) $@ $^ $(TOMCRYPT)

dm: dm.cpp
	$(COMPILE) $@ $^ $(FUSE)

crc-bug: crc-bug.cpp
	$(COMPILE) $@ $^ $(BOOSTINC) $(ZLIB)

qi-adapt-bug: qi-adapt-bug.cpp
	$(COMPILE) $@ $^ $(BOOSTINC)

crypt: crypt.cpp
	$(COMPILE) $@ $^ $(TOMCRYPT)

parse: parse.cpp
	$(COMPILE) $@ $^

openssl: openssl.cpp
	$(COMPILE) $@ $^ $(BOOSTINC) $(OPENSSL)

.PHONY: all clean

clean:
	rm -rf $(PROGRAMS) *.dSYM
