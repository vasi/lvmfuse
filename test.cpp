#include "lvm.hpp"
#include "lvm-text.hpp"

#include <iostream>

using namespace devmapper;
using namespace lvm;
using namespace lvm::text;
using namespace std;

int main(int argc, char *argv[]) {
	target::ptr file(new targets::file("/dev/disk0s8"));
	target::ptr linear(new targets::linear(file, 187 * 65536 + 2048));
	fuse_serve("mnt", *linear, 1242 * 65536);
	return 0;
	
	try {
		pvdevice pv(argv[1]);
		std::string conftext(pv.vg_config());
		
		parser parser(conftext);
		section_p config(parser.vg_config());
		
		dumper dumper(cout);
		dumper.dump(*config);
	} catch (std::exception& e) {
		cerr << e.what() << "\n";
		return -1;
	}
}
