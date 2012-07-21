CXX = clang++
CXXFLAGS = -Wall -I include
OPT = -O0 -g

PROGRAMS = test

all: $(PROGRAMS)

test: test.o dm/target-file.o
	$(CXX) $(LDFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(OPT) -o $@ -c $<

clean:
	rm -rf *.dSYM *.o $(PROGRAMS)

.PHONY: all clean
