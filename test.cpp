#include "lvm.hpp"
#include "lvm-text.hpp"

#include <iostream>

int main(int argc, char *argv[]) {
	try {
		lvm::pvdevice pv(argv[1]);
		std::string conftext(pv.vg_config());
		
		lvm::text::parser parser(conftext);
		lvm::text::section_p config(parser.vg_config());
		
		lvm::text::dumper dumper(std::cout);
		dumper.dump(*config);
	} catch (std::exception& e) {
		std::cerr << e.what() << "\n";
		return -1;
	}	
}
