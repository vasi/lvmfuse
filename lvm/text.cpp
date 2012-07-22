#include "lvm-text.hpp"

#include <cctype>

namespace lvm {
namespace text {

int value::integer() const {
	return DYPTR_CAST<integer_core>(inner)->i;
}

std::string& value::string() const {
	return DYPTR_CAST<string_core>(inner)->s;
}

array_t& value::array() const {
	return *DYPTR_CAST<array_core>(inner)->a;
}

section_t& value::section() const {
	return *DYPTR_CAST<section_core>(inner)->m;
}

void value::set(vtype t, value_core *i) {
	m_type = t;
	inner.reset(i);
}

void parser::skip() {
	while (*pos) {
		if (*pos == '#') {
			while (*pos && *pos++ != '\n')
				; // pass
		} else if (isspace(*pos)) {
			++pos;
		} else {
			break;
		}
	}
}

bool parser::eof() {
	return literal('\0', Remain);
}

bool parser::literal(char c, advance a) {
	skip();
	if (*pos != c)
		return false;
	if (a == Advance)
		++pos;
	return true;
}

bool parser::identifier(std::string& s) {
	skip();
	s.clear();
	while (*pos && (isalnum(*pos) || strchr("-_.+", *pos)))
		s.push_back(*pos++);
	
	if (s.empty())
		err = "expected identifier";
	return !s.empty();
}
	
parser::status parser::integer(int& i) {
	skip();
	if (!isdigit(*pos))
		return Continue;
	
	i = 0;
	while (*pos && isdigit(*pos))
		i = 10 * i + *pos++ - '0';
	return Ok;
}

parser::status parser::string(std::string& s) {
	if (!literal('"'))
		return Continue;
	
	s.clear();
	for (; *pos && *pos != '"'; ++pos) {
		char c = *pos;
		if (c == '\\') {
			c = *(pos + 1);
			if (c != '\\' && c != '"') {
				err = "unknown escape";
				return Error;
			}
			++pos;
		}
		s.push_back(c);
	}
	
	if (*pos != '"') {
		err = "expected terminating quote";
		return Error;
	}
	++pos;
	
	return Ok;
}

parser::status parser::array(array_p& a) {
	if (!literal('['))
		return Continue;
	
	struct value v;
	a.reset(new array_t());
	while (true) {
		if (literal(']'))
			return Ok;
		
		if (!a->empty() && !literal(',')) {
			err = "expected array separator";
			return Error;
		}
		
		if (!value(v))
			return Error;
		a->push_back(v);
	}
}

bool parser::section(section_p& m) {
	std::string id;
	struct value v;
	m.reset(new section_t());
	while (true) {
		if (literal('}', Remain) || eof())
			return true;
		
		if (!identifier(id))
			return false;
		if (literal('=')) {
			if (!value(v))
				return false;
		} else if (literal('{')) {
			section_p sub;
			if (!section(sub))
				return false;
			if (!literal('}')) {
				err = "expected closing brace";
				return false;
			}
			v.set(value::Section, new section_core(sub));
		} else {
			err = "expected equals sign or opening brace";
			return false;
		}
		(*m)[id] = v;
	}
}

bool parser::value(struct value& v) {
	int i;
	status st = integer(i); // Can't be Error
	if (st == Ok) {
		v.set(value::Integer, new integer_core(i));
		return true;
	}
	
	std::string s;
	st = string(s);
	if (st == Error)
		return false;
	if (st == Ok) {
		v.set(value::String, new string_core(s));
		return true;
	}
	
	array_p a;
	st = array(a);
	if (st == Error)
		return false;
	if (st == Ok) {
		v.set(value::Array, new array_core(a));
		return true;
	}
	
	err = "expected a value";
	return false;
}
	
bool parser::vg_config(section_p& m) {
	if (!section(m))
		return false;
	if (eof())
		return true;
	err = "expected end of file";
	return false;
}

bool parser::vg_name(std::string& s) {
	if (!identifier(s))
		return false;
	if (literal('{'))
		return true;
	err = "expected opening brace";
	return false;
}

void dumper::tab() {
	for (int i = 0; i < indent; ++i)
		os << "    ";
}

void dumper::dump(const std::string& s) {
	os << "\"";
	for (std::string::const_iterator it = s.begin(); it != s.end(); ++it) {
		if (*it == '"' || *it == '\\')
			os << "\\";
		os << *it;
	}
	os << "\"";
}

void dumper::dump(const array_t& a) {
	bool first = true;
	os << "[";
	for (array_t::const_iterator it = a.begin(); it != a.end(); ++it) {
		if (first)
			first = false;
		else
			os << ", ";
		dump(*it);
	}
	os << "]";
}

void dumper::dump(const value& v) {
	switch (v.type()) {
		case value::Integer: os << v.integer(); break;
		case value::String: dump(v.string()); break;
		case value::Array: dump(v.array()); break;
		case value::Section: dump(v.section()); break;
	}
}

void dumper::dump(const section_t& m) {
	for (section_t::const_iterator it = m.begin(); it != m.end(); ++it) {
		tab();
		os << it->first;
		if (it->second.type() == value::Section) {
			os << " {\n";
			++indent;
			dump(it->second);
			--indent;
			tab();
			os << "}";
		} else {
			os << " = ";
			dump(it->second);
		}
		os << "\n";
	}
}

} } // namespace lvm::text
