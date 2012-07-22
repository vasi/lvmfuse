#include "lvm.hpp"

#include <iostream>

int main(int argc, char *argv[]) {
	lvm::pvdevice pv;
	pv.init(argv[1]);
	std::cout << pv.uuid() << "\n";
	std::string config;
	pv.vg_config(config);
	std::cout << config << "\n";
}
