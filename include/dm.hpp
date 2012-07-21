#include <vector>

namespace devmapper {

const size_t BlockSize = 512;

namespace target {
struct file {
	file(const char *name);
	~file();
	int read(off_t block, uint8_t *buf);
	
private:
	int fd;
};
}

} // devmapper
