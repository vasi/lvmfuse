#include "lvm.hpp"

#include <vector>
#include <fcntl.h>

#include <zlib.h>

using std::string;
using std::vector;
using devmapper::BlockSize;
using devmapper::swap_le;


namespace lvm {

/***** Constants and types for PV on-disk format *****/
namespace pvdisk {
static const size_t LabelSectors = 4; // how many sectors to check? 
static const char* LabelMagic = "LABELONE";
static const char* LabelType = "LVM2 001"; // LVM2 text format
static const size_t UUIDSize = 32;
static const char *MetadataMagic = "\040\114\126\115\062\040\170\133\065\101"
	"\045\162\060\116\052\076";
static const uint32_t MetadataVersion = 1;
static const uint32_t FlagIgnore = 0x1;

struct label {
	char magic[8];
	uint64_t this_sector_idx;
	uint32_t crc32; // from next field to end of sector
	uint32_t contents_offset; // from start of label
	char type[8];
} __attribute__((packed));

struct header {
	char uuid[UUIDSize];
	uint64_t size;	
} __attribute__((packed));
struct region {
	uint64_t offset;
	uint64_t size;
} __attribute__((packed));

struct md_header {
	uint32_t crc32; // to end of sector
	char magic[16];
	uint32_t version;
	uint64_t this_offset;
	uint64_t size;
} __attribute__((packed));
struct md_region {
	uint64_t offset;
	uint64_t size;
	uint32_t crc32;
	uint32_t flags;
} __attribute__((packed));
} // namespace pvdisk
using namespace pvdisk;


/***** Utility functions *****/

// Check if a magic value is matched 
static bool magic(const char *want, const char *have) {
	return strncmp(want, have, strlen(want)) == 0;
}

// Calculate a CRC32 checksum
static uint32_t crc(const uint8_t *p, size_t size) {
	// zlib CRC32 does pre-/post-xor, undo it
	const uint32_t CRCInitialRemainder = 0xf597a6cf;
	return crc32(CRCInitialRemainder ^ 0xFFFFFFFF, (const Bytef*)p, size)
		^ 0xFFFFFFFF;
}

// Turn a raw UUID buffer into its string representation
static string uuid_string(const char *uuid) {
	string s;
	int groups[] = { 6, 4, 4, 4, 4, 4, 6 };
	int *group = groups;
	for (int i = 0; i < UUIDSize; ++i) {
		s.push_back(uuid[i]);
		if (!--(*group) && i + 1 < UUIDSize) {
			s.push_back('-');
			++group;
		}
	}
	return s;
}


/***** Methods *****/

pvdevice::pvdevice() { }
pvdevice::pvdevice(const char *name) { open(name); }

void pvdevice::open(const char *name) {
	fd.open(name);
	uint8_t buf[BlockSize];
	for (size_t sector = 0; sector < LabelSectors; ++sector) {
		fd.read(buf, BlockSize);
		if (read_label(sector, buf))
			return;
	}
	throw exception("Can't find an LVM2 label");
}
		
bool pvdevice::read_label(size_t sector, uint8_t *buf) {
	label *lab(reinterpret_cast<label*>(buf));
	if (!magic(LabelMagic, lab->magic))
		return false;
	if (!magic(LabelType, lab->type))
		return false;
	
	swap_le(lab->this_sector_idx);
	if (lab->this_sector_idx != sector)
		throw exception("LVM2 label on wrong sector");
		
	swap_le(lab->crc32);
	uint8_t *crc_start = reinterpret_cast<uint8_t*>(&lab->contents_offset);
	if (crc(crc_start, BlockSize - (crc_start - buf)) != lab->crc32)
		throw exception("LVM2 label has bad CRC");
	
	swap_le(lab->contents_offset);
	if (lab->contents_offset > BlockSize)
		throw exception("LVM2 label doesn't fit on one sector");
	
	read_header(buf + lab->contents_offset, BlockSize - lab->contents_offset);
	return true;
}

void pvdevice::read_header(uint8_t *buf, size_t size) {
	header *hdr = reinterpret_cast<header*>(buf);
	if (size < sizeof(*hdr))
		throw exception("PV header must be on LVM2 label's sector");
	m_uuid = uuid_string(hdr->uuid);
	
	bool md = false; // Data or metadata list?
	for (region *rgn = reinterpret_cast<region*>(buf + sizeof(*hdr));
			reinterpret_cast<uint8_t*>(rgn) + sizeof(*rgn) <= buf + size;
			++rgn) {
		swap_le(rgn->offset);
		swap_le(rgn->size);
		bool zero = rgn->offset == 0 && rgn->size == 0;
		if (md && zero)
			break;			// Out of metadata items
		else if (zero)
			md = true;		// Switch to metadata list
		else if (md) {
			// Found one! (Can there be multiple valid regions?)
			read_md_area(rgn->offset, rgn->size);
			return;
		}
	}
	throw exception("No valid LVM2 metadata region found");;
}

void pvdevice::read_md_area(off_t off, size_t size) {	
	if (size < BlockSize)
		throw exception("LVM2 metadata header too small");
	uint8_t buf[BlockSize];
	fd.pread(buf, BlockSize, off);
	
	md_header *hdr(reinterpret_cast<md_header*>(buf));
	if (!magic(MetadataMagic, hdr->magic))
		throw exception("LVM2 metadata header has bad magic");
	swap_le(hdr->version);
	if (hdr->version != MetadataVersion)
		throw exception("LVM2 metadata header has bad version");
	swap_le(hdr->size);
	if (hdr->size != size)
		throw exception("LVM2 metadata header has bad size");
	swap_le(hdr->this_offset);
	if (hdr->this_offset != off)
		throw exception("LVM2 metadata header has bad offset");
	swap_le(hdr->crc32);
	size_t crc_size = sizeof(hdr->crc32);
	if (hdr->crc32 != crc(buf + crc_size, BlockSize - crc_size))
		throw exception("LVM2 metadata header has bad CRC");
	
	for (md_region *rgn = reinterpret_cast<md_region*>(buf + sizeof(md_header));
			reinterpret_cast<uint8_t*>(rgn) + sizeof(*rgn) <= buf + size;
			++rgn) {
		swap_le(rgn->offset);
		swap_le(rgn->size);
		if (rgn->offset == 0 && rgn->size == 0)
			break;	// Out of regions
		swap_le(rgn->flags);
		if (rgn->flags & FlagIgnore)
			continue;
		swap_le(rgn->crc32);
		
		text_offset = off + rgn->offset;
		text_size = rgn->size;
		text_crc32 = rgn->crc32;
		return;
		// Possible to have multiple valid regions?
	}
	throw exception("No valid LVM2 metadata regions");
}

std::string pvdevice::vg_config() {
	std::string s;
	s.reserve(text_size);
	
	uint8_t buf[BlockSize];
	off_t off = text_offset, end = off + text_size;
	while (off < end) {
		size_t want = end - off;
		if (want > BlockSize)
			want = BlockSize;
		fd.pread(buf, want, off);
		s.append(reinterpret_cast<char*>(buf), want);
		off += want;
	}
	
	const uint8_t *cstr = reinterpret_cast<const uint8_t*>(s.c_str());
	if (text_crc32 != crc(cstr, s.size()))
		throw exception("LVM2 VG configuration fails CRC check");
	return s;
}

devmapper::target::ptr pvdevice::target() {
	using namespace devmapper;
	return target::ptr(new targets::file(fd.dup()));
}

} // namespace lvm
