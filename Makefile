CXX = clang++
CXXFLAGS = -Wall
OPT = -O0 -g

PROGRAMS = test

all: $(PROGRAMS)

test: test.o
	$(CXX) $(LDFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(OPT) -o $@ -c $<

clean:
	rm -rf *.dSYM $(PROGRAMS)

.PHONY: all clean
