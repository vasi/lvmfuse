#ifndef LVM_CONFIG
#define LVM_CONFIG

#include "lvm-text.hpp"

#include <vector>

namespace lvm {

struct lv {
};

struct pv {
	pv(const text::section_p& txt);
	
private:
	std::string uuid;
};

struct config {
	config(const text::section_p& txt);
	
	const std::vector<pv>& pvs() const;
	const std::vector<lv>& lvs() const;
	const std::string& uuid() const;
	
private:
	std::string uuid;
	std::vector<pv> m_pvs, m_lvs;
	size_t extent_size;
};

} // namespace lvm

#endif // LVM_CONFIG
