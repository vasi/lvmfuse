#ifndef LVM_HPP
#define LVM_HPP

#include "dm.hpp"

#include <fstream>
#include <stdexcept>
#include <string>

namespace lvm {

struct pvdevice {
	struct exception : public std::runtime_error {
		exception(const std::string& msg) : std::runtime_error(msg) { }
	};
	
	pvdevice();
	pvdevice(const char *name);
	void open(const char *name);
	
	std::string uuid() { return m_uuid; }
	std::string vg_config();
	devmapper::target::ptr target();
	
private:
	bool read_label(size_t sector, uint8_t *buf);
	void read_header(uint8_t *buf, size_t size);
	void read_md_area(off_t off, size_t size);
	
	devmapper::filedesc fd;
	std::string m_uuid;
	
	off_t text_offset;
	size_t text_size;
	uint32_t text_crc32;
};

} // namespace lvm

#endif // LVM_HPP
