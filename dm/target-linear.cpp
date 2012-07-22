#include "dm.hpp"

namespace devmapper {

namespace targets {

linear::linear(target::ptr src, off_t off)
	: source(src), offset(off) { }

int linear::read(off_t block, uint8_t *buf) {
	return source->read(block + offset, buf);
}

} } // namespace devmapper::targets
