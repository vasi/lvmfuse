#include "lvm.hpp"
#include "lvm-text.hpp"

#include <iostream>

int main(int argc, char *argv[]) {
	lvm::pvdevice pv;
	std::string conftext;
	try {
		pv.open(argv[1]);
		conftext = pv.vg_config();
	} catch (std::exception& e) {
		std::cerr << e.what() << "\n";
		return -1;
	}
	
	lvm::text::section_p config;
	lvm::text::parser parser(conftext);
	if (!parser.vg_config(config)) {
		std::cerr << parser.error();
		return -2;
	}
	
	lvm::text::dumper dumper(std::cout);
	dumper.dump(*config);
}
