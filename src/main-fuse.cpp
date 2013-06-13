#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <cstring>
#include <iostream>
#include <cstdio>
#include <endian.h>
#include <errno.h>
#include <stdexcept>
#include "HFSVolume.h"
#include "HFSCatalogBTree.h"
#include "AppleDisk.h"
#include "DMGDisk.h"
#include "FileReader.h"

static void showHelp(const char* argv0);
static void openDisk(const char* path);

int hfs_getattr(const char* path, struct stat* stat);
int hfs_readlink(const char* path, char* buf, size_t size);
int hfs_open(const char* path, struct fuse_file_info* info);
int hfs_read(const char* path, char* buf, size_t bytes, off_t offset, struct fuse_file_info* info);
int hfs_release(const char* path, struct fuse_file_info* info);
int hfs_opendir(const char* path, struct fuse_file_info* info);
int hfs_readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* info);
int hfs_releasedir(const char* path, struct fuse_file_info* info);

Reader* g_fileReader;
PartitionedDisk* g_partitions;
HFSVolume* g_volume;
HFSCatalogBTree* g_tree;

int main(int argc, char** argv)
{
	struct fuse_operations ops;
	char* args[5];
	
	if (argc != 3)
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
	ops.opendir = hfs_opendir;
	ops.readdir = hfs_readdir;
	ops.readlink = hfs_readlink;
	ops.releasedir = hfs_releasedir;
	
	args[0] = argv[0];
	args[1] = argv[2];
	args[2] = "-oro";
	args[3] = "-d";
	args[4] = 0;
	
	return fuse_main(4, args, &ops, 0);
	
	/*
	HfsplusVolume* vol = HfsplusVolume::openDMGFile(argv[1]);
	HfsplusDirectory* dir = vol->openDirectory("/Evernote.app/Contents/MacOS");
	struct dirent ent;
	
	while (dir->next(&ent))
	{
		std::cout << ent.d_name << ' ' << int(ent.d_type) << std::endl;
	}
	
	delete dir;
	delete vol;
	*/
}

void openDisk(const char* path)
{
	int partIndex = -1;

	g_fileReader = new FileReader(path);

	if (DMGDisk::isDMG(g_fileReader))
		g_partitions = new DMGDisk(g_fileReader);
	else if (AppleDisk::isAppleDisk(g_fileReader))
		g_partitions = new AppleDisk(g_fileReader);
	else
		throw std::runtime_error("Unsupported file format");

	const std::vector<PartitionedDisk::Partition>& parts = g_partitions->partitions();

	for (size_t i = 0; i < parts.size(); i++)
	{
		if (parts[i].type == "Apple_HFS" || parts[i].type == "Apple_HFSX")
		{
			partIndex = i;
			break;
		}
	}

	if (partIndex == -1)
		throw std::runtime_error("No suitable partition found in file");

	g_volume = new HFSVolume(g_partitions->readerForPartition(partIndex));
	g_tree = g_volume->rootCatalogTree();
}

void showHelp(const char* argv0)
{
	std::cerr << "Usage: " << argv0 << " <dmg-file> <mount-point>\n";
}

static void hfs_nativeToStat(const HFSPlusCatalogFileOrFolder& ff, struct stat* stat)
{
	memset(stat, 0, sizeof(*stat));

	stat->st_atime = HFSCatalogBTree::appleToUnixTime(ff.file.accessDate);
	stat->st_mtime = HFSCatalogBTree::appleToUnixTime(ff.file.contentModDate);
	stat->st_ctime = HFSCatalogBTree::appleToUnixTime(ff.file.attributeModDate);
	stat->st_mode = ff.file.permissions.fileMode;
	stat->st_uid = ff.file.permissions.ownerID;
	stat->st_gid = ff.file.permissions.groupID;

	if (ff.file.recordType == RecordType::kHFSPlusFileRecord)
	{
		stat->st_size = ff.file.dataFork.logicalSize;
		stat->st_blocks = ff.file.dataFork.totalBlocks;
		if (S_ISCHR(stat->st_mode) || S_ISBLK(stat->st_mode))
			stat->st_rdev = ff.file.permissions.special.rawDevice;
	}
}

int hfs_getattr(const char* path, struct stat* stat)
{
	HFSPlusCatalogFileOrFolder ff;
	int rv = g_tree->stat(path, &ff);

	if (rv == 0)
		hfs_nativeToStat(ff, stat);

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
	int rv = g_tree->openFile(path, &file);

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

int hfs_opendir(const char* path, struct fuse_file_info* info)
{
	HFSPlusCatalogFileOrFolder ff;
    int rv = g_tree->stat(path, &ff);

	if (rv == 0)
	{
		if (ff.folder.recordType != RecordType::kHFSPlusFolderRecord)
			rv = -ENOTDIR;
	}
	
	return rv;
}

int hfs_readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* info)
{
	std::map<std::string, HFSPlusCatalogFileOrFolder> contents;
	int rv = g_tree->listDirectory(path, contents);

	if (rv == 0)
	{
		for (auto it = contents.begin(); it != contents.end(); it++)
		{
			struct stat st;
			hfs_nativeToStat(it->second, &st);

			if (filler(buf, it->first.c_str(), &st, 0) != 0)
				return -ENOMEM;
		}
	}

	return rv;
}

int hfs_releasedir(const char* path, struct fuse_file_info* info)
{
	return 0;
}
