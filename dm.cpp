#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <exception>
#include <iostream>
#include <string>
#include <vector>

#include <boost/lexical_cast.hpp>
#include <boost/shared_ptr.hpp>

#include <fcntl.h>
#include <unistd.h>

#define FUSE_USE_VERSION 26
#include <fuse.h>

using std::string;
using std::vector;
using boost::shared_ptr;

const size_t Sector = 512;


template <typename T, typename Iter>
void shift(Iter& i, Iter& end, T& t);

void die(const char *msg);

typedef std::deque<string> args_t;
typedef args_t::const_iterator args_i;
args_t args;
string target("device");


template <typename Iter>
int readparts(Iter iter, char *buf, size_t size, off_t offset) {
	int bytes = 0;
	for (; size > 0 && !iter.end(); ++iter) {
		off_t pos = offset - iter.offset();
		size_t isize = iter.size();
		if (pos > isize)
			continue;
		
		size_t want = isize - pos;
		if (want > size)
			want = size;
		
		int ret = iter.read(buf, want, pos);
		if (ret <= 0)
			return ret;
			
		bytes += ret;
		buf += want;
		offset += want;
		size -= want;
	}
	return bytes;
}

struct segment {
	off_t start, length; // in sectors
	virtual ~segment() { }
	
	// offset and size in bytes, offset relative to 'start'
	virtual int read(char *buf, size_t size, off_t offset) = 0;
};

struct linear : segment {
	int fd;
	off_t fdoff;
	
	linear(args_i& iter, args_i& end) {
		string file;
		shift(iter, end, file);
		fd = open(file.c_str(), O_RDONLY);
		if (fd == -1)
			die("can't open file");
		shift(iter, end, fdoff);
	}
	virtual ~linear() { close(fd); }
	
	virtual int read(char *buf, size_t size, off_t offset) {
		ssize_t bytes = pread(fd, buf, size, fdoff * Sector + offset);
		return (bytes == -1) ? -errno : bytes;
	}
};

struct zero : segment {
	zero(args_i& iter, args_i& end) { }
	
	virtual int read(char *buf, size_t size, off_t offset) {
		memset(buf, 0, size);
		return size;
	}
};

struct striped : segment {
	struct source { int fd; off_t off; };
	vector<source> sources;
	off_t chunk_size;
	
	striped(args_i& iter, args_i& end) {
		size_t stripes;
		shift(iter, end, stripes);
		if (stripes <= 0)
			die("need a positive number of stripes");
		sources.resize(stripes);
		
		shift(iter, end, chunk_size);
		
		for (int i = 0; i < stripes; ++i) {
			string file;
			shift(iter, end, file);
			int fd = sources[i].fd = open(file.c_str(), O_RDONLY);
			if (fd == -1)
				die("can't open file in stripe");
			shift(iter, end, sources[i].off);
		}
	}
	virtual ~striped() {
		vector<source>::iterator i = sources.begin();
		for (; i != sources.end(); ++i)
			close(i->fd);
	}
	
	struct read_iter {
		striped& st;
		off_t ch;
		read_iter(striped& s, off_t c) : st(s), ch(c) { }
		bool end() { return ch * st.chunk_size >= st.length; }
		off_t offset() { return ch * size(); }
		size_t size() { return st.chunk_size * Sector; }
		int read(char *bf, size_t sz, off_t of) {
			size_t n = st.sources.size();
			source& sr = st.sources[ch % n];
			return pread(sr.fd, bf, sz,
				Sector * (ch / n * st.chunk_size + sr.off) + of);
		}
		read_iter& operator++() { ++ch; return *this; }
	};
	
	virtual int read(char *buf, size_t size, off_t offset) {
		return readparts(read_iter(*this, offset / Sector / chunk_size),
			buf, size, offset);
	}
};


typedef shared_ptr<segment> seg_p;
typedef std::vector<seg_p> seg_c;
typedef seg_c::const_iterator seg_i;
seg_c segments;


template <typename T, typename Iter>
void shift(Iter& i, Iter& end, T& t) {
	if (i == end)
		die("no args left");
	t = boost::lexical_cast<T>(*i++);
}

void die(const char *msg) {
	fprintf(stderr, "%s\n", msg);
	exit(-1);
}

bool is_target(const char *path) {
	return path[0] == '/' && target == (path + 1);
}

off_t total_size() {
	seg_p s = segments.back();
	return (s->start + s->length) * Sector;
}

void validate_segments() {
	if (segments.empty())
		die("no segments");
	seg_i i1 = segments.begin();
	if ((*i1)->start != 0)
		die("First segment must have offset zero");
	
	for (seg_i i2 = i1 + 1; i2 != segments.end(); ++i1, ++i2) {
		if ((*i1)->start + (*i1)->length != (*i2)->start)
			die("Segments must be consecutive");
	}
}


seg_p parse_segment(args_i& iter, args_i& end) {
	if (iter == end)
		return seg_p();
	
	off_t start, length;
	string name;
	
	shift(iter, end, start);
	shift(iter, end, length);
	shift(iter, end, name);
	
	seg_p p;
	if (name == "linear") {
		p.reset(new linear(iter, end));
	} else if (name == "zero") {
		p.reset(new zero(iter, end));
	} else if (name == "error") {
		die("error mapping unsupported due to FUSE minimum I/O size");
	} else if (name == "striped") {
		p.reset(new striped(iter, end));
	} else {
		die("no such segment type");
	}
	p->start = start;
	p->length = length;
	
	if (iter != end) {
		if (*iter++ != ".")
			die("unterminated segment");
	}
	
	return p;
}


extern "C" int dm_getattr(const char *path, struct stat *stbuf) {
	memset(stbuf, 0, sizeof(struct stat));

	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 3;
	} else if (is_target(path)) {
		stbuf->st_mode = S_IFREG | 0444;
		stbuf->st_nlink = 1;
		stbuf->st_size = total_size();
	} else
		return -ENOENT;

	return 0;
}

extern "C" int dm_open(const char *path, struct fuse_file_info *fi) {
	if (!is_target(path))
		return -ENOENT;

	if ((fi->flags & O_ACCMODE) != O_RDONLY)
		return -EACCES;

	return 0;
}

extern "C" int dm_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
              off_t offset, struct fuse_file_info *fi) {
	if (strcmp(path, "/") != 0)
		return -ENOENT;
	filler(buf, target.c_str(), NULL, 0);
	return 0;
}

struct seg_read_i {
	seg_i seg, endi;
	seg_read_i(seg_i start, seg_i end) : seg(start), endi(end) { }
	bool end() { return seg == endi; }
	off_t offset() { return (*seg)->start * Sector; }
	size_t size() { return (*seg)->length * Sector; }
	int read(char *b, size_t s, off_t o) { return (*seg)->read(b, s, o); }
	seg_read_i& operator++() { ++seg; return *this; }
};
extern "C" int dm_read(const char *path, char *buf, size_t size, off_t offset,
           struct fuse_file_info *fi) {
	if (!is_target(path))
		return -ENOENT;
	
	return readparts(seg_read_i(segments.begin(), segments.end()),
		buf, size, offset);
}

struct fuse_operations dm_ops = {
	.getattr = dm_getattr,
	.open    = dm_open,
	.read    = dm_read,
	.readdir = dm_readdir,
};

extern "C" int dm_opt(void *data, const char *arg, int key,
		struct fuse_args *outargs) {
	if (key != FUSE_OPT_KEY_NONOPT)
		return 1;
	args.push_back(arg);
	return 0;
}

int main(int argc, char **argv) {
	struct fuse_args fargs = FUSE_ARGS_INIT(argc, argv);
	fuse_opt_parse(&fargs, NULL, NULL, dm_opt);

	args_i iter = args.begin(), end = args.end();	
	string mountpoint;
	shift(iter, end, mountpoint);
	fuse_opt_add_arg(&fargs, mountpoint.c_str());
	
	seg_p seg;
	while ((seg = parse_segment(iter, end))) {
		segments.push_back(seg);
	}
	validate_segments();
	
	return fuse_main(fargs.argc, fargs.argv, &dm_ops, NULL);
}
