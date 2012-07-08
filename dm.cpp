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

extern "C" int dm_read(const char *path, char *buf, size_t size, off_t offset,
           struct fuse_file_info *fi) {
	if (!is_target(path))
		return -ENOENT;
	
	int bytes = 0;
	seg_i iter = segments.begin();
	for (; size && iter != segments.end(); ++iter) {
		off_t start = (*iter)->start * Sector,
			end = start + (*iter)->length * Sector;
		if (end < offset)
			continue;
		
		off_t pos = offset - start;
		size_t want = end - pos;
		if (want > size)
			want = size;
		
		int ret = (*iter)->read(buf, want, pos);
		if (ret <= 0)
			return ret;
		
		bytes += ret;
		buf += want;
		offset += want;
		size -= want;
	}
	return bytes;
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
