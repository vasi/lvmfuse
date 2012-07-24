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

int file::read(off_t block, uint8_t *buf, size_t offset, size_t size) {
	ssize_t bytes = pread(fd, &buf[0], size, BlockSize * block + offset);
	if (bytes == -1)
		return -errno;
	else if (bytes != size)
		return -EIO;
	else
		return size;
}

} } // namespace devmapper::targets
