#include <cctype>
#include <cstdio>
#include <fstream>
#include <map>
#include <sstream>
#include <vector>
#include <tr1/memory>
using std::string;
using std::vector;
using std::map;
using std::tr1::shared_ptr;

string slurp(char *file) {
	std::ifstream input(file);
	std::stringstream ss;
	ss << input.rdbuf();
	return ss.str();
}


struct value;
typedef map<string,value> section;

// probably should be polymorphic
struct value {
	enum type_t {
		INT,
		STRING,
		ARRAY,
		SECTION,
	};
	
	type_t type;
	int intr;
	string str;
	vector<value> arr;
	section sec;
};

void fatal(string err, const char *p) {
	fprintf(stderr, "%s at '%-20s'\n", err.c_str(), p);
	exit(-1);
}

void skip(const char *&p) {
	while (true) {
		if (*p == '\0')
			return;
		else if (*p == '#')
			while (*p && *p++ != '\n')
				; // pass
		else if (isspace(*p))
			++p;
		else
			break;
	}
}

void parse_ident(const char *&p, string& s) {
	skip(p);
	while(*p && (isalnum(*p) || strchr("-_.+", *p)))
		s.push_back(*p++);
	if (s.empty())
		fatal("expected identifier", p);
}

bool parse_quoted(const char *&p, string& s) {
	skip(p);
	if (*p != '"')
		return false;
	++p;
	while (*p && *p != '"') {
		char c = *p++;
		if (c == '\\') {
			if (strchr("\\\"", *p))
				c = *p++;
			else
				fatal("unknown escape", p - 1);
		}
		s.push_back(c);
	}
	if (*p++ != '"')
		fatal("no terminating quote", p - 1);
	return true;
}

bool parse_int(const char *&p, int& i) {
	skip(p);
	if (!isdigit(*p))
		return false;
	while (isdigit(*p))
		i = i * 10 + *p++ - '0';
	return true;
}

void parse_value(const char *&p, value& v);

bool parse_array(const char *&p, vector<value>& v) {
	skip(p);
	if (*p != '[')
		return false;
	++p;
	
	value val;
	while (true) {
		skip(p);
		if (*p == ']') {
			++p;
			return true;
		} else if (!v.empty() && *p++ != ',') {
			fatal("expected comma", p - 1);
		}
		parse_value(p, val);
		v.push_back(val);
		val = value();
	}
}

bool parse_sec(const char *&p, section& m, bool brackets = true) {
	skip(p);
	if (brackets) {
		if (*p != '{')
			return false;
		++p;
	}
	
	string id;
	value val;
	while (true) {
		skip(p);
		if (brackets && *p == '}') {
			++p;
			return true;
		}
		if (!brackets && !*p)
			return true;
		parse_ident(p, id);
		skip(p);
		if (*p == '=')
			++p;		
		parse_value(p, val);
		m[id] = val;
		id = string();
		val = value();
	}
}

void parse_value(const char *&p, value& v) {
	if (parse_quoted(p, v.str)) {
		v.type = value::STRING;
	} else if (parse_int(p, v.intr)) {
		v.type = value::INT;
	} else if (parse_array(p, v.arr)) {
		v.type = value::ARRAY;
	} else if (parse_sec(p, v.sec)) {
		v.type = value::SECTION;
	} else {
		fatal("expected value", p);
	}
}

void tab(int indent) {
	for (int i = 0; i < indent; ++i)
		printf("  ");
}

void print_value(const value& v, int indent = 0);

void print_sec(const section& m, int indent = 0) {
	printf("{\n");
	++indent;
	section::const_iterator it = m.begin();
	for (; it != m.end(); ++it) {
		tab(indent);
		printf("%s ", it->first.c_str());
		if (it->second.type != value::SECTION)
			printf("= ");
		print_value(it->second, indent);
		printf("\n");
	}
	--indent;
	tab(indent);
	printf("}");
}

void print_value(const value& v, int indent) {
	if (v.type == value::STRING) {
		const char *c = v.str.c_str();
		printf("\"");
		for (; *c; ++c) {
			if (strchr("\\\"", *c))
				printf("\\");
			printf("%c", *c);
		}
		printf("\"");
	} else if (v.type == value::INT) {
		printf("%d", v.intr);
	} else if (v.type == value::ARRAY) {
		printf("[");
		for (int i = 0; i < v.arr.size(); ++i) {
			if (i != 0)
				printf(", ");
			print_value(v.arr[i], indent);
		}
		printf("]");		
	} else if (v.type == value::SECTION) {
		print_sec(v.sec, indent);
	}
}


int main(int argc, char *argv[]) {
	string s = slurp(argv[1]);
	const char *c = s.c_str();
	
	// value v;
	// parse_value(c, v);
	// print_value(v);
	
	section sec;
	parse_sec(c, sec, false);
	print_sec(sec);
	
	printf("\n");
	return 0;
}
