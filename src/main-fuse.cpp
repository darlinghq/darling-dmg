#include "main-fuse.h"
#include <cstring>
#include <iostream>
#include <cstdio>
#include "be.h"
#include <errno.h>
#include <stdexcept>
#include <limits>
#include <functional>
#include "HFSVolume.h"
#include "AppleDisk.h"
#include "GPTDisk.h"
#include "DMGDisk.h"
#include "FileReader.h"
#include "CachedReader.h"
#include "exceptions.h"
#include "HFSHighLevelVolume.h"
#include "DiskImage.h"
#ifdef DARLING
#	include "stat_xlate.h"
#endif

std::unique_ptr<HFSHighLevelVolume> g_volume;

// The volume's name, given to Darling's kernel layer (see volumeAttribute).
static std::string g_volumeName;

int main(int argc, const char** argv)
{
	try
	{
		struct fuse_operations ops;
		struct fuse_args args = FUSE_ARGS_INIT(0, NULL);
	
		if (argc < 3)
		{
			showHelp(argv[0]);
			return 1;
		}
	
		openDisk(argv[1]);
	
		memset(&ops, 0, sizeof(ops));
	
		ops.getattr = hfs_getattr;
		ops.open = hfs_open;
		ops.read = hfs_read;
		ops.release = hfs_release;
		//ops.opendir = hfs_opendir;
		ops.readdir = hfs_readdir;
		ops.readlink = hfs_readlink;
		//ops.releasedir = hfs_releasedir;
		ops.getxattr = hfs_getxattr;
		ops.listxattr = hfs_listxattr;
	
		for (int i = 0; i < argc; i++)
		{
			if (i == 1)
				;
			else
				fuse_opt_add_arg(&args, argv[i]);
		}
		fuse_opt_add_arg(&args, "-oro");
		fuse_opt_add_arg(&args, "-s");
	
#ifndef BEFORE_MOUNT_EXTRA // the mount helper reports through its stderr, which must only carry errors
		std::cerr << "Everything looks OK, disk mounted\n";
#endif

#ifdef BEFORE_MOUNT_EXTRA // Darling only
		BEFORE_MOUNT_EXTRA;
#endif

		return fuse_main(args.argc, args.argv, &ops, 0);
	}
	catch (const std::exception& e)
	{
		std::cerr << "Error: " << e.what() << std::endl;
		std::cerr << std::endl;

		std::cerr << "Possible reasons:\n"
			"1) The file is corrupt.\n"
			"2) The file is not really a DMG file, although it resembles one.\n"
			"3) There is a bug in darling-dmg.\n";

		return 1;
	}
}

void showHelp(const char* argv0)
{
	std::cerr << "Usage: " << argv0 << " <file> <mount-point> [fuse args]\n\n";
	std::cerr << ".DMG files and raw disk images can be mounted.\n";
	std::cerr << argv0 << " automatically selects the first HFS+/HFSX partition.\n";
}


void openDisk(const char* path)
{
	static DiskImage image;

	openDiskImage(path, image);
	g_volume.reset(new HFSHighLevelVolume(image.volume));
	g_volumeName = image.volume->volumeName();
}

int handle_exceptions(std::function<int()> func)
{
	try
	{
		return func();
	}
	catch (const file_not_found_error& e)
	{
		std::cerr << "File not found: " << e.what() << std::endl;
		return -ENOENT;
	}
	catch (const function_not_implemented_error& e)
	{
		std::cerr << "Error: " << e.what() << std::endl;
		return -ENOSYS;
	}
	catch (const io_error& e)
	{
		std::cerr << "I/O error: " << e.what() << std::endl;
		return -EIO;
	}
	catch (const no_data_error& e)
	{
		std::cerr << "Non-existent data requested" << std::endl;
		return -ENODATA;
	}
	catch (const attribute_not_found_error& e)
	{
		std::cerr << e.what() << std::endl;
		return -ENODATA;
	}
	catch (const operation_not_permitted_error& e)
	{
		std::cerr << e.what() << std::endl;
		return -EPERM;
	}
	catch (const std::logic_error& e)
	{
		std::cerr << "Fatal error: " << e.what() << std::endl;
		abort();
	}
	catch (const std::exception& e)
	{
		std::cerr << "Unknown error: " << e.what() << std::endl;
		return -EIO;
	}
}

int hfs_getattr(const char* path, struct stat* stat)
{
	std::cerr << "hfs_getattr(" << path << ")\n";

	return handle_exceptions([&]() {
#ifndef DARLING
		*stat = g_volume->stat(path);
#else
		struct stat st = g_volume->stat(path);
		bsd_stat_to_linux_stat(&st, reinterpret_cast<linux_stat*>(stat));
#endif
		return 0;
	});
}

int hfs_readlink(const char* path, char* buf, size_t size)
{
	std::cerr << "hfs_readlink(" << path << ")\n";

	return handle_exceptions([&]() {

		std::shared_ptr<Reader> file;
		size_t rd;

		file = g_volume->openFile(path);
		rd = file->read(buf, size-1, 0);
		
		buf[rd] = '\0';
		return 0;
	});
}

int hfs_open(const char* path, struct fuse_file_info* info)
{
	std::cerr << "hfs_open(" << path << ")\n";

	return handle_exceptions([&]() {

		std::shared_ptr<Reader> file;
		std::shared_ptr<Reader>* fh;

		file = g_volume->openFile(path);
		fh = new std::shared_ptr<Reader>(file);

		info->fh = uint64_t(fh);
		return 0;
	});
}

int hfs_read(const char* path, char* buf, size_t bytes, off_t offset, struct fuse_file_info* info)
{
	return handle_exceptions([&]() {
		if (!info->fh)
			return -EIO;

		std::shared_ptr<Reader>& file = *(std::shared_ptr<Reader>*) info->fh;
		return file->read(buf, bytes, offset);
	});
}

int hfs_release(const char* path, struct fuse_file_info* info)
{
	// std::cout << "File cache zone: hit rate: " << g_volume->getFileZone()->hitRate() << ", size: " << g_volume->getFileZone()->size() << " blocks\n";

	return handle_exceptions([&]() {

		std::shared_ptr<Reader>* file = (std::shared_ptr<Reader>*) info->fh;
		delete file;
		info->fh = 0;
		
		return 0;
	});
}

int hfs_readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* info)
{
	std::cerr << "hfs_readdir(" << path << ")\n";

	return handle_exceptions([&]() {
		std::map<std::string, struct stat> contents;

		contents = g_volume->listDirectory(path);

		for (auto it = contents.begin(); it != contents.end(); it++)
		{
			if (filler(buf, it->first.c_str(), &it->second, 0) != 0)
				return -ENOMEM;
		}

		return 0;
	});

}

// Darling's kernel layer asks the root of a FUSE volume for these two attributes to answer getattrlist(ATTR_VOL_NAME)
// and statfs().f_fstypename the way macOS does: "darling.volume.name" is the volume name (UTF-8, unterminated) and
// "darling.volume.kind" the file system kind. They are not listed; guests cannot see them, because guest xattrs are
// all "user." ones on the host.
static bool volumeAttribute(const char* path, const char* name, std::vector<uint8_t>& data)
{
	if (strcmp(path, "/") != 0)
		return false;

	const char* value;
	if (strcmp(name, "darling.volume.name") == 0)
		value = g_volumeName.c_str();
	else if (strcmp(name, "darling.volume.kind") == 0)
		value = "hfs";
	else
		return false;

	data.assign(value, value + strlen(value));
	return true;
}

#if defined(__APPLE__) && !defined(DARLING)
int hfs_getxattr(const char* path, const char* name, char* value, size_t vlen, uint32_t position)
#else
int hfs_getxattr(const char* path, const char* name, char* value, size_t vlen)
#endif
{
	std::cerr << "hfs_getxattr(" << path << ", " << name << ")\n";
#if defined(__APPLE__) && !defined(DARLING)
	if (position > 0) return -ENOSYS; // it's not supported... yet. I think it doesn't happen anymore since osx use less ressource fork
#endif

	return handle_exceptions([&]() -> int {
		std::vector<uint8_t> data;

		if (!volumeAttribute(path, name, data))
			data = g_volume->getXattr(path, name);

		if (value == nullptr)
			return data.size();

		if (vlen < data.size())
			return -ERANGE;

		memcpy(value, &data[0], data.size());
		return data.size();
	});
}

int hfs_listxattr(const char* path, char* buffer, size_t size)
{
	return handle_exceptions([&]() -> int {
		std::vector<std::string> attrs;
		std::vector<char> output;

		attrs = g_volume->listXattr(path);

		for (const std::string& str : attrs)
			output.insert(output.end(), str.c_str(), str.c_str() + str.length() + 1);

		if (buffer == nullptr)
			return output.size();

		if (size < output.size())
			return -ERANGE;

		memcpy(buffer, &output[0], output.size());
		return output.size();
	});
}
