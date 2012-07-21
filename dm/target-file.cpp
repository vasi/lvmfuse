#include <dm.hpp>

#include <errno.h>
#include <fcntl.h>

namespace devmapper {

namespace target {

file::file(const char *name) : fd(open(name, O_RDONLY)) { }

file::~file() {
	close(fd);
}

int file::read(off_t block, uint8_t *buf) {
	ssize_t bytes = pread(fd, &buf[0], BlockSize, BlockSize * block);
	if (bytes == -1)
		return errno;
	else if (bytes != BlockSize)
		return EIO;
	else
		return bytes;
}

} } // devmapper::target
