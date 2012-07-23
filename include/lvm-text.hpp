#ifndef LVM_TEXT_HPP
#define LVM_TEXT_HPP

#include "lvm-text-value.hpp"

#include <stdexcept>
#include <iostream>

namespace lvm {
namespace text {

struct parser {
	struct exception : public std::runtime_error {
		exception(const std::string& msg) : std::runtime_error(msg) { }
	};
	
	parser(const std::string& s) : str(s), pos(s.c_str()) { }
	
	enum advance { Advance, Remain };
	
	void skip();
	bool eof();
	bool literal(char c, advance a = Advance);
	void identifier(std::string& s);
	
	bool integer(int& i);
	bool string(std::string& s);
	bool array(array_p& a);
	void section(section_p& m);
	
	void value(struct value& v);
	
	section_p vg_config();
	std::string vg_name();
	
private:
	const std::string& str;
	const char *pos;
};

struct dumper {
	dumper(std::ostream& o) : os(o), indent(0) { }
	
	void tab();
	void dump(const std::string& s);
	void dump(const array_t& a);
	void dump(const section_t& m);
	void dump(const value& v);
	
private:
	std::ostream& os;
	int indent;
};

} } // namespace lvm::text

#endif // LVM_TEXT_HPP
