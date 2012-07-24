#include "common.hpp"

#include <cstdlib>
#include <limits>
using std::numeric_limits;

namespace devmapper {

bool parse_int(const std::string& s, int& i) {
	if (s.empty())
		return false;
	
	const char *c = s.c_str();
	char *endp = NULL;
	long l = std::strtol(c, &endp, 0);
	if (*endp != 0 || l > numeric_limits<int>::max() ||
			l < numeric_limits<int>::min())
		return false;
	
	i = l;
	return true;
}


} // namespace devmapper
