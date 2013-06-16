#include "main-fuse.h"
#include <cstring>
#include <iostream>
#include <cstdio>
#include <endian.h>
#include <errno.h>
#include <stdexcept>
#include <limits>
#include "HFSVolume.h"
#include "HFSCatalogBTree.h"
#include "AppleDisk.h"
#include "DMGDisk.h"
#include "FileReader.h"

static const char* RESOURCE_FORK_SUFFIX = "#..namedfork#rsrc";
static const char* XATTR_RESOURCE_FORK = "com.apple.ResourceFork";

Reader* g_fileReader;
PartitionedDisk* g_partitions;
HFSVolume* g_volume;
HFSCatalogBTree* g_tree;

int main(int argc, char** argv)
{
	char** args = nullptr;
	int argi, rv;
	
	try
	{
		struct fuse_operations ops;
	
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
	
		args = new char*[argc+1];
		args[0] = argv[0];
		args[1] = argv[2];
		args[2] = (char*) "-oro";
	
		for (argi = 3; argi < argc; argi++)
			args[argi] = argv[argi];
	
		args[argi] = nullptr;
	
		rv = fuse_main(argc, args, &ops, 0);
	}
	catch (const std::exception& e)
	{
		std::cerr << "Error: " << e.what() << std::endl;
		rv = 1;
	}
	
	delete g_tree;
	delete g_volume;
	delete g_partitions;
	delete g_fileReader;
	delete [] args;
	
	return rv;
}

void openDisk(const char* path)
{
	int partIndex = -1;

	g_fileReader = new FileReader(path);

	if (DMGDisk::isDMG(g_fileReader))
		g_partitions = new DMGDisk(g_fileReader);
	else if (AppleDisk::isAppleDisk(g_fileReader))
		g_partitions = new AppleDisk(g_fileReader);
	else if (HFSVolume::isHFSPlus(g_fileReader))
		g_volume = new HFSVolume(g_fileReader);
	else
		throw std::runtime_error("Unsupported file format");

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
		}

		if (partIndex == -1)
			throw std::runtime_error("No suitable partition found in file");

		g_volume = new HFSVolume(g_partitions->readerForPartition(partIndex));
	}
	
	g_tree = g_volume->rootCatalogTree();
}

void showHelp(const char* argv0)
{
	std::cerr << "Usage: " << argv0 << " <file> <mount-point> [fuse args]\n\n";
	std::cerr << ".DMG files and raw disk images can be mounted.\n";
	std::cerr << argv0 << " auttomatically selects the first HFS+/HFSX partition.\n";
}

static void hfs_nativeToStat(const HFSPlusCatalogFileOrFolder& ff, struct stat* stat, bool resourceFork = false)
{
	memset(stat, 0, sizeof(*stat));

	stat->st_atime = HFSCatalogBTree::appleToUnixTime(ff.file.accessDate);
	stat->st_mtime = HFSCatalogBTree::appleToUnixTime(ff.file.contentModDate);
	stat->st_ctime = HFSCatalogBTree::appleToUnixTime(ff.file.attributeModDate);
	stat->st_mode = ff.file.permissions.fileMode;
	stat->st_uid = ff.file.permissions.ownerID;
	stat->st_gid = ff.file.permissions.groupID;
	stat->st_ino = ff.file.fileID;
	stat->st_blksize = 512;
	stat->st_nlink = 1;

	if (ff.file.recordType == RecordType::kHFSPlusFileRecord)
	{
		if (!resourceFork)
		{
			stat->st_size = ff.file.dataFork.logicalSize;
			stat->st_blocks = ff.file.dataFork.totalBlocks;
		}
		else
		{
			stat->st_size = ff.file.resourceFork.logicalSize;
			stat->st_blocks = ff.file.resourceFork.totalBlocks;
		}
		
		if (S_ISCHR(stat->st_mode) || S_ISBLK(stat->st_mode))
			stat->st_rdev = ff.file.permissions.special.rawDevice;
	}
	
	if (!stat->st_mode)
	{
		if (ff.file.recordType == RecordType::kHFSPlusFileRecord)
		{
			stat->st_mode = S_IFREG;
			stat->st_mode |= 0444;
		}
		else
		{
			stat->st_mode = S_IFDIR;
			stat->st_mode |= 0555;
		}
	}
}

static bool string_endsWith(const std::string& str, const std::string& what)
{
	if (str.size() < what.size())
		return false;
	else
		return str.compare(str.size()-what.size(), what.size(), what) == 0;
}

int hfs_getattr(const char* path, struct stat* stat)
{
	HFSPlusCatalogFileOrFolder ff;
	std::string spath = path;
	int rv;
	bool resourceFork = false;
	
	if (string_endsWith(path, RESOURCE_FORK_SUFFIX))
	{
		spath.resize(spath.length() - strlen(RESOURCE_FORK_SUFFIX));
		resourceFork = true;
	}
	
	rv = g_tree->stat(spath.c_str(), &ff);

	if (rv == 0)
		hfs_nativeToStat(ff, stat, resourceFork);

	return rv;
}

int hfs_readlink(const char* path, char* buf, size_t size)
{
	Reader* file;
	int rv = g_tree->openFile(path, &file);
	if (rv == 0)
	{
		int rd = file->read(buf, size, 0);
		buf[rd] = 0;
	}
	
	delete file;
	return rv;
}

int hfs_open(const char* path, struct fuse_file_info* info)
{
	Reader* file;
	std::string spath = path;
	int rv;
	bool resourceFork = false;
	
	if (string_endsWith(path, RESOURCE_FORK_SUFFIX))
	{
		spath.resize(spath.length() - strlen(RESOURCE_FORK_SUFFIX));
		resourceFork = true;
	}
	
	rv = g_tree->openFile(spath.c_str(), &file, resourceFork);

	if (rv == 0)
		info->fh = uint64_t(file);
	
	return rv;
}

int hfs_read(const char* path, char* buf, size_t bytes, off_t offset, struct fuse_file_info* info)
{
	Reader* file = (Reader*) info->fh;
	return file->read(buf, bytes, offset);
}

int hfs_release(const char* path, struct fuse_file_info* info)
{
	Reader* file = (Reader*) info->fh;
	delete file;
	info->fh = 0;
	return 0;
}

void processResourceForks(std::map<std::string, HFSPlusCatalogFileOrFolder>& contents)
{
	for (auto it = contents.begin(); it != contents.end(); it++)
	{
		if (it->second.file.recordType == RecordType::kHFSPlusFileRecord
			&& !string_endsWith(it->first, RESOURCE_FORK_SUFFIX))
		{
			if (it->second.file.resourceFork.logicalSize > 0)
			{
				std::string rsrcForkName = it->first + RESOURCE_FORK_SUFFIX;
				contents[rsrcForkName] = it->second;
			}
		}
	}
}

int hfs_readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* info)
{
	std::map<std::string, HFSPlusCatalogFileOrFolder> contents;
	int rv = g_tree->listDirectory(path, contents);
	
	//processResourceForks(contents);

	if (rv == 0)
	{
		for (auto it = contents.begin(); it != contents.end(); it++)
		{
			struct stat st;
			hfs_nativeToStat(it->second, &st, string_endsWith(it->first, RESOURCE_FORK_SUFFIX));

			if (filler(buf, it->first.c_str(), &st, 0) != 0)
				return -ENOMEM;
		}
	}

	return rv;
}

int hfs_getxattr(const char* path, const char* name, char* value, size_t vlen)
{
	Reader* file;
	int rv;
	
	if (strcmp(name, XATTR_RESOURCE_FORK) != 0)
		return -ENODATA;
	
	rv = g_tree->openFile(path, &file, true);
	if (rv < 0)
		return rv;
	
	rv = std::min<int>(std::numeric_limits<int>::max(), file->length());
	
	if (vlen >= rv)
		rv = file->read(value, rv, 0);
	
	delete file;
	
	return rv;
}

int hfs_listxattr(const char* path, char* buffer, size_t size)
{
	int rv = strlen(XATTR_RESOURCE_FORK)+1;
	if (size >= rv)
	{
		strcpy(buffer, XATTR_RESOURCE_FORK);
		buffer[rv] = 0;
	}
	
	return rv;
}
