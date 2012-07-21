#include <dm.hpp>
using namespace devmapper;

#include <vector>
#include <unistd.h>

int main(int argc, char *argv[]) {
	target::ptr ft(new targets::file(argv[1]));
	target::ptr lt(new targets::linear(ft, 65536 * 187 + 2048));
	
	std::vector<uint8_t> buf(BlockSize);
	for (size_t i = 0; i < 10; ++i) {
		lt->read(i, &buf[0]);
		write(STDOUT_FILENO, &buf[0], BlockSize);
	}
}
