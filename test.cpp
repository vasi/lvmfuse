#include "lvm.hpp"
#include "lvm-text.hpp"

#include <iostream>

int main(int argc, char *argv[]) {
	lvm::pvdevice pv;
	pv.init(argv[1]);
	std::string conftext;
	if (pv.vg_config(conftext) != lvm::pvdevice::NoError)
		return -1;
	
	lvm::text::section_p config;
	lvm::text::parser parser(conftext);
	if (!parser.vg_config(config)) {
		std::cerr << parser.error();
		return -2;
	}
	
	lvm::text::dumper dumper(std::cout);
	dumper.dump(*config);
}
