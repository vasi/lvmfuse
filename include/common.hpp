#ifndef COMMON_HPP
#define COMMON_HPP

#include <stdexcept>
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

bool parse_int(const std::string& s, int& i);

struct filedesc {
	struct exception : public std::runtime_error {
		exception(const std::string& msg) : std::runtime_error(msg) { }
	};
	
	filedesc();
	filedesc(const char *path);
	~filedesc();
	
	bool is_open() const;
	void open(const char *path);
	void close();
	
	void read(uint8_t *buf, size_t size);
	void pread(uint8_t *buf, size_t size, off_t offset);
	int dup();
	
private:
	int m_fd;
};

} // namespace devmapper

#endif // COMMON_HPP
