#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/phoenix_object.hpp>
#include <boost/spirit/include/phoenix_operator.hpp>
#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/fusion/adapted/std_pair.hpp>
#include <boost/variant.hpp>

#include <iostream>
#include <sstream>
#include <fstream>
#include <vector>
#include <map>

namespace lvmfuse {

std::string slurp(char *file) {
	std::ifstream input(file);
	std::stringstream ss;
	ss << input.rdbuf();
	return ss.str();
}


typedef std::string lvmtext_string;
typedef unsigned long long lvmtext_int;
struct lvmtext_array;
struct lvmtext_section;
typedef boost::variant<
	lvmtext_int,
	lvmtext_string,
	boost::recursive_wrapper<lvmtext_array>,
	boost::recursive_wrapper<lvmtext_section>
> lvmtext_t;
struct lvmtext_array {
	std::vector<lvmtext_t> items;
};
struct lvmtext_section {
	typedef std::map<lvmtext_string,lvmtext_t> items_type;
	items_type items;
};
typedef std::pair<lvmtext_string,lvmtext_t> lvmtext_keyval;

} // namespace

BOOST_FUSION_ADAPT_STRUCT(
	lvmfuse::lvmtext_array,
	(std::vector<lvmfuse::lvmtext_t>, items)
)
BOOST_FUSION_ADAPT_STRUCT(
	lvmfuse::lvmtext_section,
	(lvmfuse::lvmtext_section::items_type, items)
)

namespace lvmfuse {

namespace qi = boost::spirit::qi;
namespace phoenix = boost::phoenix;

template <typename Iter>
struct lvmtext_skipper : qi::grammar<Iter> {
	lvmtext_skipper() : lvmtext_skipper::base_type(skip, "space") {
		using qi::lit;
		using qi::char_;
		using qi::space;
		using qi::eol;
		comment = lit('#') > *(char_ - eol) > eol;
		skip = space | comment;
	}
	
	qi::rule<Iter> comment;
	qi::rule<Iter> skip;
};

typedef lvmtext_section lvmtext_out;

template <typename Iter>
struct lvmtext_parser : qi::grammar<Iter, lvmtext_out(),
		 lvmtext_skipper<Iter> > {
	lvmtext_parser() : lvmtext_parser::base_type(section, "lvm2 text") {
		using qi::hold;
		using qi::lexeme;
		using qi::eps;
		using qi::alnum;
		using qi::lit;
		using qi::space;
		using qi::char_;
		using namespace qi::labels;
		using phoenix::val;
		using phoenix::construct;
		
		identifier %= lexeme[+(alnum | char_("-_.+"))];
		integer_r %= qi::ulong_long;
		escape_seq %= lit('\\') > (char_('\\') | char_('"'));
		string = lexeme[*(escape_seq | (char_ - '"'))];
		quoted_string %= lit('"') > string > lit('"');
		
		array %= lit('[') > -(expr % ',') > lit(']');
		keyval %= identifier >> lit('=') >> expr;
		named_section %= identifier >> lit('{') > section > lit('}');
		section %= eps >> *(hold[keyval] | named_section);
		expr %= integer_r | quoted_string | array;
		
		identifier.name("identifier");
		integer_r.name("integer");
		escape_seq.name("escape sequence");
		quoted_string.name("quoted string");
		array.name("array");
		keyval.name("key-value pair");
		named_section.name("named section");
		section.name("section");
		expr.name("expression");
	}
	
	qi::rule<Iter, lvmtext_t(), lvmtext_skipper<Iter> > expr;
	qi::rule<Iter, lvmtext_array(), lvmtext_skipper<Iter> > array;
	qi::rule<Iter, lvmtext_keyval(), lvmtext_skipper<Iter> > keyval;
	qi::rule<Iter, lvmtext_keyval(), lvmtext_skipper<Iter> > named_section;
	qi::rule<Iter, lvmtext_section(), lvmtext_skipper<Iter> > section;
	
	qi::rule<Iter, lvmtext_string()> identifier;
	qi::rule<Iter, lvmtext_int()> integer_r;
	
	qi::rule<Iter, lvmtext_string()> quoted_string;
	qi::rule<Iter, lvmtext_string()> string;
	qi::rule<Iter, char()> escape_seq;
};


void tab(size_t indent) {
	for (size_t i = 0; i < indent; ++i)
		std::cout << "  ";
}

template <typename Printer>
struct lvmtext_keyval_printer : boost::static_visitor<> {
	const Printer& p;
	const std::string& k;
	const lvmtext_t& v;
	size_t indent;
	lvmtext_keyval_printer(const Printer& p, const std::string& k,
		const lvmtext_t& v, size_t indent = 0)
		: p(p), k(k), v(v), indent(indent) { }
	
	void pref() const {
		tab(indent);
		std::cout << k << " ";
	}
	
	void operator()(const lvmtext_section& s) const {
		pref();
		boost::apply_visitor(p, v);
	}
	
	template <typename T> void operator()(const T& t) const {
		pref();
		std::cout << "= ";
		boost::apply_visitor(p, v);
	}
};

struct lvmtext_printer : boost::static_visitor<> {
	size_t indent;
	lvmtext_printer(size_t indent = 0) : indent(indent) { }
	
	void operator()(const lvmtext_string& s) const {
		std::cout << '"';
		lvmtext_string::const_iterator it = s.begin();
		for (; it != s.end(); ++it) {
			if (*it == '\\' || *it == '"')
				std::cout << '\\';
			std::cout << *it;
		}
		std::cout << '"';
	}
	void operator()(const lvmtext_int& i) const {
		std::cout << i;
	}
	void operator()(const lvmtext_array& a) const {
		std::cout << '[';
		std::vector<lvmtext_t>::const_iterator it = a.items.begin();
		bool first = true;
		for (; it != a.items.end(); ++it) {
			if (!first)
				std::cout << ", ";
			first = false;
			boost::apply_visitor(*this, *it);
		}
		std::cout << ']';
	}
	void operator()(const lvmtext_section& s) const {
		lvmtext_printer next(indent + 1);
		std::cout << "{\n";
		lvmtext_section::items_type::const_iterator it = s.items.begin();
		for (; it != s.items.end(); ++it) {
			lvmtext_keyval_printer<lvmtext_printer> kvp(next, it->first,
				it->second, indent + 1);
			boost::apply_visitor(kvp, it->second);
			std::cout << "\n";
		}
		tab(indent);
		std::cout << "}";
	}
};

void lvmtext_print_keyval(const lvmtext_keyval& kv, size_t indent = 0) {
	lvmtext_keyval_printer<lvmtext_printer> kvp(lvmtext_printer(), kv.first,
		kv.second);
	boost::apply_visitor(kvp, kv.second);
}

} // namespace


int main(int argc, char *argv[]) {
	if (argc < 2) {
		std::cerr << "Need some input text\n";
		return -1;
	}
	std::string input = lvmfuse::slurp(argv[1]);
	
	typedef std::string::const_iterator iter_t;
	iter_t iter = input.begin(), end = input.end();
	lvmfuse::lvmtext_parser<iter_t> parser;
	lvmfuse::lvmtext_skipper<iter_t> skip;
	
	namespace qi = boost::spirit::qi;
	std::string name;
	lvmfuse::lvmtext_out desc;
	
	bool ok = qi::parse(iter, end,
		parser.identifier >> *qi::space >> qi::lit('{'), name);
	if (!ok) {
		std::cout << "name parse failed\n";
	} else {
		std::cout << "name: " << name << "\n\n";
		iter = input.begin();
		ok = qi::phrase_parse(iter, end, parser, skip, desc);
		if (ok && iter == end) {
			lvmfuse::lvmtext_printer()(desc);
			std::cout << "\n";
		} else {
			std::cout << "parse failed\n";
		}
	}
	
	return 0;
}
