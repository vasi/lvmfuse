#ifndef COMMON_HPP
#define COMMON_HPP

#include <vector>

#include <tr1/memory>
#define SHARED_PTR std::tr1::shared_ptr
#define DYPTR_CAST std::tr1::dynamic_pointer_cast

namespace devmapper {

const size_t BlockSize = 512;

template <typename T>
void swap_le(T& val) {
	T res;
	uint8_t *byte = reinterpret_cast<uint8_t*>(&val);
	for (int i = sizeof(T) - 1; i >= 0; --i) {
		res <<= 8;
		res += byte[i];
	}
	val = res;
}

} // namespace devmapper

#endif // COMMON_HPP
