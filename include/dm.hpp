#include <vector>

#include <tr1/memory>
#define SHARED_PTR std::tr1::shared_ptr

namespace devmapper {

const size_t BlockSize = 512;

struct target {
	typedef SHARED_PTR<target> ptr;
	
	virtual ~target() { }
	virtual int read(off_t block, uint8_t *buf) = 0;
};


namespace targets {

struct file : public target {
	file(const char *name);
	virtual ~file();
	virtual int read(off_t block, uint8_t *buf);
	
private:
	int fd;
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
