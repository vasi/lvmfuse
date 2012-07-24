#include "dm.hpp"

#include <string>
using std::string;

#include <errno.h>

#define FUSE_USE_VERSION 26
#include <fuse.h>


namespace devmapper {

struct fuse_params {
	target& tgt;
	size_t size;
};

} // namespace devmapper
using devmapper::fuse_params;
using devmapper::BlockSize;


static const char *target_name = "device";

static fuse_params *par() {
	return reinterpret_cast<fuse_params*>(fuse_get_context()->private_data);
}

static bool is_target(const char *path) {
	return path[0] == '/' && string(target_name) == (path + 1);
}


extern "C" int dm_getattr(const char *path, struct stat *stbuf) {
	memset(stbuf, 0, sizeof(struct stat));

	if (string("/") == path) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 3;
	} else if (is_target(path)) {
		stbuf->st_mode = S_IFREG | 0444;
		stbuf->st_nlink = 1;
		stbuf->st_size = par()->size;
	} else
		return -ENOENT;

	return 0;
}

extern "C" int dm_open(const char *path,
		struct fuse_file_info *fi) {
	if (!is_target(path))
		return -ENOENT;

	if ((fi->flags & O_ACCMODE) != O_RDONLY)
		return -EACCES;

	return 0;
}

extern "C" int dm_readdir(const char *path, void *buf,
		fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
	if (string("/") != path)
		return -ENOENT;
	filler(buf, target_name, NULL, 0);
	return 0;
}

extern "C" int dm_read(const char *path, char *buf, size_t size,
		off_t offset, struct fuse_file_info *fi) {
	if (!is_target(path))
		return -ENOENT;
	
	off_t end = offset + size, block = offset / BlockSize;
	while (true) {
		off_t bstart = block * BlockSize;
		if (bstart > end)
			return size;
		
		off_t boffset = offset - bstart;
		if (boffset < 0)
			boffset = 0;
		off_t bend = end - bstart;
		if (bend > BlockSize)
			bend = BlockSize;
		size_t want = bend - boffset;
		
		int err = par()->tgt.read(block, reinterpret_cast<uint8_t*>(buf),
			boffset, want);
		if (err < 0)
			return err;
		if (err != want)
			return err + boffset - offset;
		
		++block;
		buf += want;
	}
}


namespace devmapper {

static struct fuse_operations fuse_ops = {
	.getattr	= dm_getattr,
	.open		= dm_open,
	.read		= dm_read,
	.readdir	= dm_readdir,
};

void fuse_serve(const char *path, target& tgt, size_t size) {
	fuse_params params = { .tgt = tgt, .size = size };
	struct fuse_args args = FUSE_ARGS_INIT(0, NULL);
	fuse_opt_add_arg(&args, "devmapper::fuse_serve"); // progname
	fuse_opt_add_arg(&args, "-f"); // foreground
	fuse_opt_add_arg(&args, path);
	fuse_main(args.argc, args.argv, &fuse_ops, &params);
}

} // namespace devmapper
