#ifndef _STAT_XLATE_H
#define _STAT_XLATE_H

#include <errno.h>

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

// For the same reason, the error numbers this module returns to libfuse (for the
// Linux kernel) are Darwin ones and must be translated: most are the same, but
// not ENODATA (96 on Darwin, 61 on Linux) or ENOSYS (78, 38).
static inline int bsd_errno_to_linux(int error)
{
	switch (error)
	{
		case ENODATA: return 61;
		case ENOSYS: return 38;
		case ENOTSUP: return 95;
		default: return error; // EPERM ENOENT EIO ENOMEM EACCES ENOTDIR EISDIR EINVAL ERANGE
	}
}

// Extended attribute names: Darling's kernel layer keeps every Darwin attribute
// "<name>" as "user.<name>" on the host, and shows guests only "user." ones. This
// volume's attributes are Darwin ones, so they are listed and looked up that way.
#define LINUX_XATTR_GUEST_PREFIX "user."

#endif
