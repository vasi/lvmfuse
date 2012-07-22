#ifndef DM_HPP
#define DM_HPP

#include "common.hpp"

#include <vector>

namespace devmapper {

struct target {
	typedef SHARED_PTR<target> ptr;
	
	// FIXME: init() method to check for errs?
	
	virtual ~target() { }
	virtual int read(off_t block, uint8_t *buf) = 0;
};


namespace targets {

struct file : public target {
	file(const char *name);
	file(int fd, bool cleanup = true);
	virtual ~file();
	virtual int read(off_t block, uint8_t *buf);
	
private:
	int fd;
	bool cleanup;
};


struct linear : public target {
	linear(target::ptr src, off_t off);	
	virtual int read(off_t block, uint8_t *buf);

private:
	target::ptr source;
	off_t offset;
};

} // target

} // devmapper

#endif // DM_HPP
