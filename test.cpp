#include <dm.hpp>
using namespace devmapper;

#include <unistd.h>

int main(int argc, char *argv[]) {
	uint8_t buf[BlockSize];
	target::file ft(argv[1]);
	ft.read(0, buf);
	write(STDOUT_FILENO, buf, BlockSize);
}
