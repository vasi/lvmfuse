#include "dm.hpp"

#include <errno.h>
#include <fcntl.h>

namespace devmapper {

namespace targets {

file::file(const char *name)
	: fd(open(name, O_RDONLY)), cleanup(true) { }

file::file(int fd, bool cleanup)
	: fd(fd), cleanup(cleanup) { }

file::~file() {
	if (cleanup)
		close(fd);
}

int file::read(off_t block, uint8_t *buf) {
	ssize_t bytes = pread(fd, &buf[0], BlockSize, BlockSize * block);
	if (bytes == -1)
		return errno;
	else if (bytes != BlockSize)
		return EIO;
	else
		return 0;
}

} } // devmapper::targets
