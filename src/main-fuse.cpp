#include "main-fuse.h"
#include <cstring>
#include <iostream>
#include <cstdio>
#ifdef __FreeBSD__
#include <sys/endian.h>
#else
#include <endian.h>
#endif
#include <errno.h>
#include <stdexcept>
#include <limits>
#include "HFSVolume.h"
#include "AppleDisk.h"
#include "GPTDisk.h"
#include "DMGDisk.h"
#include "FileReader.h"
#include "CachedReader.h"
#include "exceptions.h"
#include "HFSHighLevelVolume.h"

std::shared_ptr<Reader> g_fileReader;
std::unique_ptr<HFSHighLevelVolume> g_volume;
std::unique_ptr<PartitionedDisk> g_partitions;

int main(int argc, char** argv)
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
	
		std::cout << "Everything looks OK, disk mounted\n";

		return fuse_main(args.argc, args.argv, &ops, 0);
	}
	catch (const std::exception& e)
	{
		std::cerr << "Error: " << e.what() << std::endl;
		std::cerr << std::endl;

		std::cerr << "Possible reasons:\n"
			"1) The file is corrupt.\n"
			"2) The file is not really a supported disk image, and needs further processing. "
			"The author has seen files with a .dmg extension, which were actually XAR archives "
			"containing a DMG.\n"
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
	int partIndex = -1;
	std::shared_ptr<HFSVolume> volume;

	g_fileReader.reset(new FileReader(path));

	if (DMGDisk::isDMG(g_fileReader))
		g_partitions.reset(new DMGDisk(g_fileReader));
	else if (GPTDisk::isGPTDisk(g_fileReader))
		g_partitions.reset(new GPTDisk(g_fileReader));
	else if (AppleDisk::isAppleDisk(g_fileReader))
		g_partitions.reset(new AppleDisk(g_fileReader));
	else if (HFSVolume::isHFSPlus(g_fileReader))
		volume.reset(new HFSVolume(g_fileReader));
	else
		throw function_not_implemented_error("Unsupported file format");

	if (g_partitions)
	{
		const std::vector<PartitionedDisk::Partition>& parts = g_partitions->partitions();

		for (size_t i = 0; i < parts.size(); i++)
		{
			if (parts[i].type == "Apple_HFS" || parts[i].type == "Apple_HFSX")
			{
				std::cout << "Using partition #" << i << " of type " << parts[i].type << std::endl;
				partIndex = i;
				break;
			}
			else
				std::cerr << "Skipping partition of type " << parts[i].type << std::endl;
		}

		if (partIndex == -1)
			throw function_not_implemented_error("No suitable partition found in file");

		volume.reset(new HFSVolume(g_partitions->readerForPartition(partIndex)));
	}
	
	g_volume.reset(new HFSHighLevelVolume(volume));
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
		*stat = g_volume->stat(path);
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

int hfs_getxattr(const char* path, const char* name, char* value, size_t vlen)
{
	std::cerr << "hfs_getxattr(" << path << ", " << name << ")\n";

	return handle_exceptions([&]() -> int {
		std::vector<uint8_t> data;

		data = g_volume->getXattr(path, name);

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

		if (size < output.size())
			return -ERANGE;

		memcpy(buffer, &output[0], output.size());
		return output.size();
	});
}
