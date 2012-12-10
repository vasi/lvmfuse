#ifndef LVM_TEXT_VALUE_HPP
#define LVM_TEXT_VALUE_HPP

#include "common.hpp"

#include <map>
#include <string>
#include <vector>

namespace lvm {
namespace text {

// Ersatz variant
struct value;
typedef std::vector<value> array_t;
typedef SHARED_PTR<array_t> array_p;
typedef std::map<std::string,value> section_t;
typedef SHARED_PTR<section_t> section_p;

struct value_core {
	virtual ~value_core() { };
};
struct integer_core : public value_core {
	integer_core(int i) : i(i) { }
	int i;
};
struct string_core : public value_core {
	string_core(const std::string& s) : s(s) { }
	std::string s;
};
struct array_core : public value_core {
	array_core(array_p a) : a(a) { }
	array_p a;
};
struct section_core : public value_core {
	section_core(section_p m) : m(m) { }
	section_p m;
};

struct value {
	enum vtype { Integer, String, Array, Section };

	vtype type() const { return m_type; }
	int integer() const;
	std::string& string() const;
	array_t& array() const;
	section_t& section() const;
	value& operator[](const std::string& key) const;
	
	void set(vtype t, value_core *i);

private:
	vtype m_type;
	SHARED_PTR<value_core> inner;
};


} } // namespace lvm::text

#endif // LVM_TEXT_VALUE_HPP
