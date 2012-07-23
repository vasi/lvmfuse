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

void parser::identifier(std::string& s) {
	skip();
	s.clear();
	while (*pos && (isalnum(*pos) || strchr("-_.+", *pos)))
		s.push_back(*pos++);
	
	if (s.empty())
		throw exception("expected identifier");
}
	
bool parser::integer(int& i) {
	skip();
	if (!isdigit(*pos))
		return false;
	
	i = 0;
	while (*pos && isdigit(*pos))
		i = 10 * i + *pos++ - '0';
	return true;
}

bool parser::string(std::string& s) {
	if (!literal('"'))
		return false;
	
	s.clear();
	for (; *pos && *pos != '"'; ++pos) {
		char c = *pos;
		if (c == '\\') {
			c = *(pos + 1);
			if (c != '\\' && c != '"')
				throw exception("unknown escape");
			++pos;
		}
		s.push_back(c);
	}
	
	if (*pos != '"')
		throw exception("expected terminating quote");
	++pos;
	
	return true;
}

bool parser::array(array_p& a) {
	if (!literal('['))
		return false;
	
	struct value v;
	a.reset(new array_t());
	while (true) {
		if (literal(']'))
			return true;
		
		if (!a->empty() && !literal(','))
			throw exception("expected array separator");
		
		value(v);
		a->push_back(v);
	}
}

void parser::section(section_p& m) {
	std::string id;
	struct value v;
	m.reset(new section_t());
	while (!literal('}', Remain) && !eof()) {
		identifier(id);
		if (literal('=')) {
			value(v);
		} else if (literal('{')) {
			section_p sub;
			section(sub);
			if (!literal('}'))
				throw exception("expected closing brace");
			v.set(value::Section, new section_core(sub));
		} else {
			throw exception("expected equals sign or opening brace");
		}
		(*m)[id] = v;
	}
}

void parser::value(struct value& v) {
	int i;
	if (integer(i)) {
		v.set(value::Integer, new integer_core(i));
		return;
	}
	
	std::string s;
	if (string(s)) {
		v.set(value::String, new string_core(s));
		return;
	}
	
	array_p a;
	if (array(a)) {
		v.set(value::Array, new array_core(a));
		return;
	}
	
	throw exception("expected a value");
}
	
section_p parser::vg_config() {
	section_p m;
	section(m);
	if (!eof())
		throw exception("expected end of file");
	return m;
}

std::string parser::vg_name() {
	std::string s;
	identifier(s);
	if (!literal('{'))
		throw exception("expected opening brace");
	return s;
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
