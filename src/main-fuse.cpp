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
#include "GPTDisk.h"
#include "DMGDisk.h"
#include "FileReader.h"
#include "MemoryReader.h"
#include "HFSZlibReader.h"
#include "HFSAttributeBTree.h"
#include "DMGDecompressor.h"
#include "ResourceFork.h"
#include "CachedReader.h"
#include "decmpfs.h"

static const char* RESOURCE_FORK_SUFFIX = "#..namedfork#rsrc";
static const char* XATTR_RESOURCE_FORK = "com.apple.ResourceFork";
static const char* XATTR_FINDER_INFO = "com.apple.FinderInfo";

std::shared_ptr<Reader> g_fileReader;
PartitionedDisk* g_partitions;
HFSVolume* g_volume;
HFSCatalogBTree* g_tree;
CacheZone g_fileZone(6400);

int main(int argc, char** argv)
{
	int argi, rv;
	
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

		rv = fuse_main(args.argc, args.argv, &ops, 0);
	}
	catch (const std::exception& e)
	{
		std::cerr << "Error: " << e.what() << std::endl;
		rv = 1;
	}
	
	delete g_tree;
	delete g_volume;
	delete g_partitions;
	
	return rv;
}

void openDisk(const char* path)
{
	int partIndex = -1;
	uint64_t volumeSize;

	g_fileReader.reset(new FileReader(path));

	if (DMGDisk::isDMG(g_fileReader))
		g_partitions = new DMGDisk(g_fileReader);
	else if (GPTDisk::isGPTDisk(g_fileReader))
		g_partitions = new GPTDisk(g_fileReader);
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
			else
				std::cerr << "Skipping partition of type " << parts[i].type << std::endl;
		}

		if (partIndex == -1)
			throw std::runtime_error("No suitable partition found in file");

		g_volume = new HFSVolume(g_partitions->readerForPartition(partIndex));
	}
	
	volumeSize = g_volume->volumeSize();
	if (volumeSize < 50*1024*1024)
	{
		// limit cache sizes to volume size
		g_fileZone.setMaxBlocks(volumeSize / CacheZone::BLOCK_SIZE / 2);
		HFSBTree::setMaxCacheBlocks(volumeSize / CacheZone::BLOCK_SIZE / 2);
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

decmpfs_disk_header* get_decmpfs(HFSCatalogNodeID cnid, std::vector<uint8_t>& holder)
{
	HFSAttributeBTree* attributes = g_volume->attributes();
	decmpfs_disk_header* hdr;
	
	if (!attributes)
		return nullptr;
	
	if (!attributes->getattr(cnid, DECMPFS_XATTR_NAME, holder))
		return nullptr;
	
	if (holder.size() < 16)
		return nullptr;
	
	hdr = (decmpfs_disk_header*) &holder[0];
	if (hdr->compression_magic != DECMPFS_MAGIC)
		return nullptr;
	
	return hdr;
}

int hfs_getattr(const char* path, struct stat* stat)
{
	std::cerr << "hfs_getattr(" << path << ")\n";
	
	try
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
		{
			hfs_nativeToStat(ff, stat, resourceFork);
			
			// Compressed FS support
			if ((ff.file.permissions.ownerFlags & HFS_PERM_OFLAG_COMPRESSED) && !stat->st_size)
			{
				decmpfs_disk_header* hdr;
				std::vector<uint8_t> xattrData;
				
				hdr = get_decmpfs(ff.file.fileID, xattrData);
				
				if (hdr != nullptr)
					stat->st_size = hdr->uncompressed_size;
			}
		}
		
		std::cout << "hfs_getattr() -> " << rv << std::endl;

		return rv;
	}
	catch (const std::exception& e)
	{
		std::cerr << "ERROR: " << e.what() << std::endl;
		return -EIO;
	}
}

int hfs_readlink(const char* path, char* buf, size_t size)
{
	try
	{
		std::cerr << "hfs_readlink(" << path << ")\n";
		
		std::shared_ptr<Reader> file;
		int rv = g_tree->openFile(path, file);
		if (rv == 0)
		{
			int rd = file->read(buf, size, 0);
			buf[rd] = 0;
		}
		
		return rv;
	}
	catch (const std::exception& e)
	{
		std::cerr << "ERROR: " << e.what() << std::endl;
		return -EIO;
	}
}

int hfs_open(const char* path, struct fuse_file_info* info)
{
	try
	{
		std::cerr << "hfs_open(" << path << ")\n";
		
		std::shared_ptr<Reader> file;
		std::string spath = path;
		int rv = 0;
		bool resourceFork = false, compressed = false;
		HFSPlusCatalogFileOrFolder ff;
		
		if (string_endsWith(path, RESOURCE_FORK_SUFFIX))
		{
			spath.resize(spath.length() - strlen(RESOURCE_FORK_SUFFIX));
			resourceFork = true;
		}
		
		if (!resourceFork)
		{
			// stat
			rv = g_tree->stat(spath.c_str(), &ff);
			compressed = ff.file.permissions.ownerFlags & HFS_PERM_OFLAG_COMPRESSED;
		}

		if (rv != 0)
			return rv;
		
		if (!compressed)
			rv = g_tree->openFile(spath.c_str(), file, resourceFork);
		else
		{
			decmpfs_disk_header* hdr;
			std::vector<uint8_t> holder;
			
			hdr = get_decmpfs(ff.file.fileID, holder);
			
			if (!hdr)
				rv = -ENOENT;
			else
			{
				std::cout << "Opening compressed file, compression type: " << int(hdr->compression_type) << std::endl;
				switch (DecmpfsCompressionType(hdr->compression_type))
				{
					case DecmpfsCompressionType::UncompressedInline:
						file.reset(new MemoryReader(hdr->attr_bytes, hdr->uncompressed_size));
						rv = 0;
						break;
					case DecmpfsCompressionType::CompressedInline:
						file.reset(new MemoryReader(hdr->attr_bytes, holder.size() - 16));
						file.reset(new HFSZlibReader(file, hdr->uncompressed_size, true));
						rv = 0;
						break;
					case DecmpfsCompressionType::CompressedResourceFork:
					{
						rv = g_tree->openFile(spath.c_str(), file, true);
						if (rv == 0)
						{
							std::unique_ptr<ResourceFork> rsrc (new ResourceFork(file));
							file = rsrc->getResource(DECMPFS_MAGIC, DECMPFS_ID);
							
							if (file)
								file.reset(new HFSZlibReader(file, hdr->uncompressed_size));
							else
								rv = -ENOSYS;
						}
						break;
					}
					default:
						rv = -ENOSYS;
				}
			}
		}

		if (rv == 0)
		{
			std::shared_ptr<Reader>* fh = new std::shared_ptr<Reader>(new CachedReader(file, &g_fileZone, path));
			info->fh = uint64_t(fh);
		}
		
		return rv;
	}
	catch (const std::exception& e)
	{
		std::cerr << "ERROR: " << e.what() << std::endl;
		return -EIO;
	}
}

int hfs_read(const char* path, char* buf, size_t bytes, off_t offset, struct fuse_file_info* info)
{
	try
	{
		std::shared_ptr<Reader>& file = *(std::shared_ptr<Reader>*) info->fh;
		return file->read(buf, bytes, offset);
	}
	catch (const std::exception& e)
	{
		std::cerr << "ERROR: " << e.what() << std::endl;
		return -EIO;
	}
}

int hfs_release(const char* path, struct fuse_file_info* info)
{
	std::cout << "File cache zone: hit rate: " << g_fileZone.hitRate() << ", size: " << g_fileZone.size() << " blocks\n";
	try
	{
		std::shared_ptr<Reader>* file = (std::shared_ptr<Reader>*) info->fh;
		delete file;
		info->fh = 0;
		
		return 0;
	}
	catch (const std::exception& e)
	{
		std::cerr << "ERROR: " << e.what() << std::endl;
		return -EIO;
	}
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
	try
	{
		std::cerr << "hfs_readdir(" << path << ")\n";
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
	catch (const std::exception& e)
	{
		std::cerr << "ERROR: " << e.what() << std::endl;
		return -EIO;
	}
}

int hfs_getxattr(const char* path, const char* name, char* value, size_t vlen)
{
	try
	{
		int rv;
		std::string spath = path;
		
		std::cerr << "hfs_getxattr(" << path << ", " << name << ")\n";
		
		if (string_endsWith(spath, RESOURCE_FORK_SUFFIX))
			spath.resize(spath.length() - strlen(RESOURCE_FORK_SUFFIX));
		
		if (strcmp(name, XATTR_RESOURCE_FORK) == 0)
		{
			std::shared_ptr<Reader> file;

			rv = g_tree->openFile(spath.c_str(), file, true);
			if (rv < 0)
				return rv;
		
			rv = std::min<int>(std::numeric_limits<int>::max(), file->length());
		
			if (vlen >= rv)
				rv = file->read(value, rv, 0);
		}
		else if (strcmp(name, XATTR_FINDER_INFO) == 0)
		{
			HFSPlusCatalogFileOrFolder ff;
		
			rv = g_tree->stat(spath.c_str(), &ff);
			if (rv != 0)
				return rv;

			rv = 32;

			if (vlen >= rv)
			{
				if (ff.file.recordType == RecordType::kHFSPlusFileRecord)
				{
					memcpy(value, &ff.file.userInfo, sizeof(ff.file.userInfo));
					memcpy(value + sizeof(ff.file.userInfo), &ff.file.finderInfo, sizeof(ff.file.finderInfo));
				}
				else
				{
					memcpy(value, &ff.folder.userInfo, sizeof(ff.folder.userInfo));
					memcpy(value + sizeof(ff.folder.userInfo), &ff.folder.finderInfo, sizeof(ff.folder.finderInfo));
				}
			}
		}
		else
		{
			HFSPlusCatalogFileOrFolder ff;
			std::vector<uint8_t> data;
			
			// get CNID
			rv = g_tree->stat(spath.c_str(), &ff);
			
			if (rv != 0)
				return rv;
			
			if (g_volume->attributes()->getattr(ff.file.fileID, name, data))
			{
				if (vlen >= data.size())
					memcpy(value, &data[0], data.size());
			
				rv = data.size();
			}
			else
				rv = -ENODATA;
		}
		
		return rv;
	}
	catch (const std::exception& e)
	{
		std::cerr << "ERROR: " << e.what() << std::endl;
		return -EIO;
	}
}

inline void insertString(std::vector<char>& dst, const char* str)
{
	dst.insert(dst.end(), str, str+strlen(str)+1);
}

int hfs_listxattr(const char* path, char* buffer, size_t size)
{
	std::vector<char> output;
	HFSPlusCatalogFileOrFolder ff;
	int rv;
	
	insertString(output, XATTR_RESOURCE_FORK);
	insertString(output, XATTR_FINDER_INFO);
	
	// get CNID
	rv = g_tree->stat(path, &ff);
	
	if (rv != 0)
		return rv;
	
	for (const auto& kv : g_volume->attributes()->getattr(ff.file.fileID))
		insertString(output, kv.first.c_str());
	
	if (size >= output.size())
		memcpy(buffer, &output[0], output.size());
	
	return output.size();
}
