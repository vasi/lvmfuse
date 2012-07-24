#include "dm.hpp"

namespace devmapper {

namespace targets {

linear::linear(target::ptr src, off_t off)
	: source(src), src_offset(off) { }

int linear::read(off_t block, uint8_t *buf, size_t offset, size_t size) {
	return source->read(block + src_offset, buf, offset, size);
}

} } // namespace devmapper::targets
