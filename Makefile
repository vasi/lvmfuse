CXX = clang++
CXXFLAGS = -Wall -I include
LDFLAGS = -lz
OPT = -O0 -g

PROGRAMS = test

OBJECTS = test.o dm/target-file.o dm/target-linear.o lvm/pvdevice.o \
	lvm/text.o

all: $(PROGRAMS)

test: $(OBJECTS)
	$(CXX) $(LDFLAGS) -o $@ $^

%.o: %.cpp include/*.hpp
	$(CXX) $(CXXFLAGS) $(OPT) -o $@ -c $<

clean:
	rm -rf *.dSYM *.o */*.o $(PROGRAMS)

.PHONY: all clean
