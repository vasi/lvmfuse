#include <zlib.h>

#include <iostream>
#include <fstream>
#include <vector>

const size_t Sector = 512;
const size_t LabelSectors = 4; // how many sectors to check? 
const char* LabelMagic = "LABELONE";
const char* LabelType = "LVM2 001"; // LVM2 text format
const size_t UUIDSize = 32;
const char *MetadataMagic = "\040\114\126\115\062\040\170\133\065\101\045\162"
	"\060\116\052\076";
const uint32_t MetadataVersion = 1;

struct lvm2_label {
	char magic[8];
	uint64_t this_sector_idx;
	uint32_t crc32; // from next field to end of sector
	uint32_t contents_offset; // from start of label
	char type[8];
} __attribute__((packed));

struct pv_header {
	char uuid[UUIDSize];
	uint64_t size;	
} __attribute__((packed));
struct region {
	uint64_t offset;
	uint64_t size;
} __attribute__((packed));

struct metadata_header {
	uint32_t crc32; // to end of sector
	char magic[16];
	uint32_t version;
	uint64_t this_offset;
	uint64_t size;
} __attribute__((packed));
struct metadata_region {
	uint64_t offset;
	uint64_t size;
	uint32_t crc32;
	uint32_t flags;
} __attribute__((packed));


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

// Boost CRC is buggy: https://svn.boost.org/trac/boost/ticket/7098
// Use zlib instead
const uint32_t CRCInitialRemainder = 0xf597a6cf;
uint32_t lvm_crc(const char *p, size_t size) {
	return crc32(CRCInitialRemainder ^ 0xFFFFFFFF, (const Bytef*)p, size)
		^ 0xFFFFFFFF;
}

std::string uuid_string(const char *uuid) {
	std::string s;
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

bool magic(const char *want, const char *have) {
	return std::strncmp(want, have, std::strlen(want)) == 0;
}

void read_metadata_region(std::istream& is, region* meta,
		metadata_region* mdrgn) {
	std::vector<char> vbuf(mdrgn->size);
	char *buf = &vbuf[0];
	is.seekg(meta->offset + mdrgn->offset);
	is.read(buf, mdrgn->size);
	if (is.fail()) {
		std::cerr << "Metadata region read error\n";
		return;
	}
	
	if (lvm_crc(buf, vbuf.size()) != mdrgn->crc32) {
		std::cerr << "Metadata region CRC mismatch\n";
		return;
	}
	
	std::cout << buf << "\n\n\n";
}

void read_metadata(std::istream& is, region* meta) {
	char buf[Sector];
	is.seekg(meta->offset);
	is.read(buf, Sector);
	if (is.fail()) {
		std::cerr << "Metadata read error\n";
		return;
	}
	
	metadata_header *mdh = reinterpret_cast<metadata_header*>(buf);
	swap_le(mdh->version);
	swap_le(mdh->this_offset);
	swap_le(mdh->size);
	if (!magic(MetadataMagic, mdh->magic) || mdh->version != MetadataVersion ||
			mdh->this_offset != meta->offset || mdh->size != meta->size) {
		std::cerr << "Metadata format error\n";
		return;
	}
	
	swap_le(mdh->crc32);
	if (lvm_crc(mdh->magic, Sector - (mdh->magic - buf)) != mdh->crc32) {
		std::cerr << "Metadata CRC mismatch\n";
		return;
	}
	
	metadata_region *mdrgn = reinterpret_cast<metadata_region*>(buf +
		sizeof(*mdh));
	swap_le(mdrgn->offset);
	swap_le(mdrgn->size);
	swap_le(mdrgn->crc32);
	swap_le(mdrgn->flags);
	read_metadata_region(is, meta, mdrgn);
}

void read_pv_header(std::istream& is, const char *dev, char *buf) {
	pv_header *pvh = reinterpret_cast<pv_header*>(buf);
	std::cerr << dev << " UUID: " << uuid_string(pvh->uuid) << "\n";
	swap_le(pvh->size);
		
	// two zero-terminated lists of data/meta areas
	region *data = reinterpret_cast<region*>(
		reinterpret_cast<char*>(pvh) + sizeof(*pvh)),
		*data_end, *meta = 0L, *meta_end; 
	for (region *r = data; true; ++r) {
		swap_le(r->offset);
		swap_le(r->size);
		if (!r->offset && !r->size) {
			if (meta) {
				meta_end = r;
				break;
			} else {
				data_end = r;
				meta = r + 1;
			}
		}
	}
//	std::cerr << dev << " data offset: " << data->offset << "\n";
	read_metadata(is, meta);
}

void scan(const char *dev) {
	std::ifstream input(dev);
	char buf[Sector];
	for (size_t sector = 0; sector < LabelSectors; ++sector) {
		input.read(buf, Sector);
		if (input.fail())
			break;
		
		lvm2_label *label = reinterpret_cast<lvm2_label*>(buf);
		if (!magic(LabelMagic, label->magic) || !magic(LabelType, label->type))
			continue;
		
		swap_le(label->this_sector_idx);
		if (label->this_sector_idx != sector)
			continue;
		
		swap_le(label->crc32);
		char *crc_start = reinterpret_cast<char*>(&label->contents_offset);
		if (lvm_crc(crc_start, Sector - (crc_start - buf)) != label->crc32)
			continue;
		
		swap_le(label->contents_offset);
		read_pv_header(input, dev, buf + label->contents_offset);
		return;
	}
	std::cerr << "No label found on " << dev << "\n";
}

int main(int argc, char *argv[]) {
	for (int i = 1; i < argc; ++i) {
		char *dev = argv[i];
		scan(dev);
	}
	return 0;
}
