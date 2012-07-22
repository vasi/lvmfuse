#ifndef LVM_HPP
#define LVM_HPP

#include "dm.hpp"

#include <string>

namespace lvm {

struct pvdevice {
	enum error {
		NoError = 0,
		IOError,
		MagicError,
		CRCError,
		FormatError,
	};
	
	error init(const char *name);
	~pvdevice();
	
	std::string uuid() { return m_uuid; }
	error vg_config(std::string& s);
	devmapper::target::ptr target();
	
private:
	error scan_label();
	error read_label(size_t sector, uint8_t *buf);
	error read_header(uint8_t *buf, size_t size);
	error read_md_area(off_t off, size_t size);
	
	int fd;
	std::string m_uuid;
	
	off_t text_offset;
	size_t text_size;
	uint32_t text_crc32;
};

} // namespace lvm

#endif // LVM_HPP
