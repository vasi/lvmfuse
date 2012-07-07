#include <boost/spirit/include/qi.hpp>
#include <boost/fusion/include/adapt_struct.hpp>

#include <iostream>

namespace qi = boost::spirit::qi;

struct my_type {
	int inner;
};
BOOST_FUSION_ADAPT_STRUCT(
	my_type,
	(int, inner)
)

template <typename Iterator>
struct my_grammar : qi::grammar<Iterator, my_type()> {
	my_grammar() : my_grammar::base_type(outer_rule) {
		inner_rule = qi::int_;
		outer_rule = '{' >> inner_rule >> '}';	// Doesn't compile...
//		outer_rule = '{' >> qi::int_ >> '}';	// ...but this does!
	}
	qi::rule<Iterator, my_type()> inner_rule, outer_rule;
};

int main() {
	std::string input("{123}");
	my_type out;
	my_grammar<std::string::iterator> g;
	if (qi::parse(input.begin(), input.end(), g, out)) {
		std::cout << out.inner << "\n";
	} else {
		std::cout << "failed\n";
	}
	return 0;
}

