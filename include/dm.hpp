#ifndef DM_HPP
#define DM_HPP

#include "common.hpp"

#include <vector>

namespace devmapper {

struct target {
	typedef SHARED_PTR<target> ptr;
	
	// FIXME: Error checking??
	
	virtual ~target() { }
	virtual int read(off_t block, uint8_t *buf, size_t offset, size_t size) = 0;
};

void fuse_serve(const char *path, target& tgt, size_t size);


namespace targets {

struct file : public target {
	file(const char *name);
	file(int fd, bool cleanup = true);
	virtual ~file();
	virtual int read(off_t block, uint8_t *buf, size_t offset, size_t size);
	
private:
	int fd;
	bool cleanup;
};


struct linear : public target {
	linear(target::ptr src, off_t off);	
	virtual int read(off_t block, uint8_t *buf, size_t offset, size_t size);

private:
	target::ptr source;
	off_t src_offset;
};

} } // namespace devmapper::targets

#endif // DM_HPP
