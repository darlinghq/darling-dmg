#ifndef _STAT_XLATE_H
#define _STAT_XLATE_H

// THIS FILE IS ONLY USED UNDER DARLING.
//
// Explanation:
// Under Darling, this FUSE module is built as a Mach-O
// application against macOS header files. However,
// it still links against host system's libfuse.so.
// This causes problems with struct stat, the layout
// of which differs between the two systems.

struct linux_stat
{
	unsigned long st_dev;
	unsigned long st_ino;
	unsigned long st_nlink;

	unsigned int st_mode;
	unsigned int st_uid;
	unsigned int st_gid;
	unsigned int __pad0;
	unsigned long st_rdev;
	unsigned long st_size;
	unsigned long st_blksize;
	unsigned long st_blocks;

	unsigned long st_atime_sec;
	unsigned long st_atime_nsec;
	unsigned long st_mtime_sec;
	unsigned long st_mtime_nsec;
	unsigned long st_ctime_sec;
	unsigned long st_ctime_nsec;
	long unused[3];
};

static inline void bsd_stat_to_linux_stat(const struct stat* in, struct linux_stat* out)
{
	out->st_dev = in->st_dev;
	out->st_ino = in->st_ino;
	out->st_nlink = in->st_nlink;

	out->st_mode = in->st_mode;
	out->st_uid = in->st_uid;
	out->st_gid = in->st_gid;
	out->st_rdev = in->st_rdev;
	out->st_size = in->st_size;
	out->st_blksize = in->st_blksize;
	out->st_blocks = in->st_blocks;

	out->st_atime_sec = in->st_atime;
	out->st_mtime_sec = in->st_mtime;
	out->st_ctime_sec = in->st_ctime;
	out->st_atime_nsec = in->st_atimespec.tv_nsec;
	out->st_mtime_nsec = in->st_mtimespec.tv_nsec;
	out->st_ctime_nsec = in->st_ctimespec.tv_nsec;
}

#endif

