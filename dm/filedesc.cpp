#include "common.hpp"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

namespace devmapper {

static void error(const char *msg) {
	char buf[256];
	buf[0] = '\0';
	strerror_r(errno, buf, sizeof(buf));
	throw filedesc::exception(std::string(msg) + ": " + buf);
}

static void read_error(size_t want, ssize_t ret) {
	if (ret == -1)
		error("Can't read file");
	else if (ret != want)
		throw filedesc::exception("End of file");
}

filedesc::filedesc() : m_fd(-1) { }

filedesc::filedesc(const char *path) : m_fd(-1) { open(path); }

void filedesc::open(const char *path) {
	if ((m_fd = ::open(path, O_RDONLY)) == -1)
		error("Can't open file");
}

filedesc::~filedesc() { close(); }

void filedesc::close() { if (is_open()) ::close(m_fd); }

bool filedesc::is_open() const { return m_fd != -1; }

void filedesc::read(uint8_t *buf, size_t size) {
	read_error(size, ::read(m_fd, buf, size));
}

void filedesc::pread(uint8_t *buf, size_t size, off_t offset) {
	read_error(size, ::pread(m_fd, buf, size, offset));
}

int filedesc::dup() {
	return ::dup(m_fd);
}

} // namespace devmapper
