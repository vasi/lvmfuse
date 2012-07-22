#ifndef LVM_TEXT_HPP
#define LVM_TEXT_HPP

#include "lvm-text-value.hpp"

#include <iostream>

namespace lvm {
namespace text {

struct parser {
	parser(const std::string& s) : str(s), pos(s.c_str()) { }
	
	enum status { Ok, Continue, Error };
	enum advance { Advance, Remain };
	
	void skip();
	bool eof();
	bool literal(char c, advance a = Advance);
	bool identifier(std::string& s);
	
	status integer(int& i);
	status string(std::string& s);
	status array(array_p& a);
	bool section(section_p& m);
	
	bool value(struct value& v);
	
	bool vg_config(section_p& m);
	bool vg_name(std::string& s);
	
	const std::string& error() const { return err; }
	
private:
	const std::string& str;
	const char *pos;
	std::string err;
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
