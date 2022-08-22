#include "HFSHighLevelVolume.h"
#include <stdint.h>
#include <cstring>
#include "HFSAttributeBTree.h"
#include "HFSZlibReader.h"
#include "MemoryReader.h"
#include "ResourceFork.h"
#include "exceptions.h"
#include "decmpfs.h"
#include <assert.h>
#include <limits>

static const char* RESOURCE_FORK_SUFFIX = "#..namedfork#rsrc";
static const char* XATTR_RESOURCE_FORK = "com.apple.ResourceFork";
static const char* XATTR_FINDER_INFO = "com.apple.FinderInfo";

HFSHighLevelVolume::HFSHighLevelVolume(std::shared_ptr<HFSVolume> volume)
	: m_volume(volume)
{
	uint64_t volumeSize = this->volumeSize();
	if (volumeSize < 50*1024*1024)
	{
		// limit cache sizes to volume size
		m_volume->getFileZone()->setMaxBlocks(volumeSize / CacheZone::BLOCK_SIZE / 2);
		m_volume->getBtreeZone()->setMaxBlocks(volumeSize / CacheZone::BLOCK_SIZE / 2);
	}

	m_tree.reset(m_volume->rootCatalogTree());
}

static bool string_endsWith(const std::string& str, const std::string& what)
{
	if (str.size() < what.size())
		return false;
	else
		return str.compare(str.size()-what.size(), what.size(), what) == 0;
}

std::map<std::string, struct stat> HFSHighLevelVolume::listDirectory(const std::string& path)
{
	std::map<std::string, std::shared_ptr<HFSPlusCatalogFileOrFolder>> contents;
	std::map<std::string, struct stat> rv;
	int err;

	err = m_tree->listDirectory(path, contents);

	if (err != 0)
		throw file_not_found_error(path);

	for (auto it = contents.begin(); it != contents.end(); it++)
	{
		struct stat st;
		hfs_nativeToStat_decmpfs(*(it->second), &st, string_endsWith(it->first, RESOURCE_FORK_SUFFIX));

		rv[it->first] = st;
	}

	return rv;
}

struct stat HFSHighLevelVolume::stat(const std::string& path)
{
	HFSPlusCatalogFileOrFolder ff;
	std::string spath = path;
	int rv;
	bool resourceFork = false;
	struct stat stat;

	if (string_endsWith(path, RESOURCE_FORK_SUFFIX))
	{
		spath.resize(spath.length() - (sizeof(RESOURCE_FORK_SUFFIX)-1) );
		resourceFork = true;
	}

	rv = m_tree->stat(spath.c_str(), &ff);
	if (rv != 0)
		throw file_not_found_error(spath);

	hfs_nativeToStat_decmpfs(ff, &stat, resourceFork);

	return stat;
}

void HFSHighLevelVolume::hfs_nativeToStat_decmpfs(const HFSPlusCatalogFileOrFolder& ff, struct stat* stat, bool resourceFork)
{
	assert(stat != nullptr);

	hfs_nativeToStat(ff, stat, resourceFork);

	// Compressed FS support
	if ((ff.file.permissions.ownerFlags & HFS_PERM_OFLAG_COMPRESSED) && !stat->st_size)
	{
		decmpfs_disk_header* hdr;
		std::vector<uint8_t> xattrData;

		hdr = get_decmpfs(be(ff.file.fileID), xattrData);

		if (hdr != nullptr)
			stat->st_size = hdr->uncompressed_size;
	}
}

std::shared_ptr<Reader> HFSHighLevelVolume::openFile(const std::string& path)
{
	std::shared_ptr<Reader> file;
	std::string spath = path;
	int rv = 0;
	bool resourceFork = false, compressed = false;
	HFSPlusCatalogFileOrFolder ff;

	if (string_endsWith(path, RESOURCE_FORK_SUFFIX))
	{
		spath.resize(spath.length() - (sizeof(RESOURCE_FORK_SUFFIX)-1));
		resourceFork = true;
	}

	if (!resourceFork)
	{
		// stat
		rv = m_tree->stat(spath.c_str(), &ff);
		compressed = ff.file.permissions.ownerFlags & HFS_PERM_OFLAG_COMPRESSED;
	}

	if (rv != 0)
		throw file_not_found_error(path);

	if (!compressed)
	{
		rv = m_tree->openFile(spath.c_str(), file, resourceFork);

		if (rv != 0)
			throw file_not_found_error(path);
	}
	else
	{
		decmpfs_disk_header* hdr;
		std::vector<uint8_t> holder;

		hdr = get_decmpfs(be(ff.file.fileID), holder);

		if (!hdr)
			throw file_not_found_error(path);


#ifdef DEBUG
		std::cout << "Opening compressed file, compression type: " << int(hdr->compression_type) << std::endl;
#endif
		switch (DecmpfsCompressionType(hdr->compression_type))
		{
			case DecmpfsCompressionType::UncompressedInline:
				file.reset(new MemoryReader(hdr->attr_bytes, hdr->uncompressed_size));
				break;
			case DecmpfsCompressionType::CompressedInline:
				file.reset(new MemoryReader(hdr->attr_bytes, holder.size() - 16));
				file.reset(new HFSZlibReader(file, hdr->uncompressed_size, true));
				break;
			case DecmpfsCompressionType::CompressedResourceFork:
			{
				rv = m_tree->openFile(spath.c_str(), file, true);
				if (rv == 0)
				{
					std::unique_ptr<ResourceFork> rsrc (new ResourceFork(file));
					file = rsrc->getResource(DECMPFS_MAGIC, DECMPFS_ID);

					if (file)
						file.reset(new HFSZlibReader(file, hdr->uncompressed_size));
					else
						throw function_not_implemented_error("Could not find decmpfs resource in resource fork");
				}
				break;
			}
			default:
				throw function_not_implemented_error("Unknown compression type");
		}
	}

	file.reset(new CachedReader(file, m_volume->getFileZone(), path));

	return file;
}

void getXattrFinderInfo(const HFSPlusCatalogFileOrFolder& ff, uint8_t buf[32])
{
	FileInfo& newUserInfo = (*((FileInfo*)buf));
	FolderInfo& newFolderInfo = (*((FolderInfo*)buf));
	ExtendedFileInfo& newFinderInfo (*((ExtendedFileInfo*)(buf+16)));
	ExtendedFolderInfo& newExtendedFolderInfo (*((ExtendedFolderInfo*)(buf+16)));
	if (be(ff.file.recordType) == RecordType::kHFSPlusFileRecord)
	{
		// Push finder only if there is non zero data in it, excepted non-exposed field.
		newUserInfo = ff.file.userInfo;
		if ( be(newUserInfo.fileType) == kSymLinkFileType )
			memset(&newUserInfo.fileType, 0, sizeof(newUserInfo.fileType));
		else
			newUserInfo.fileType = newUserInfo.fileType;
		if ( be(newUserInfo.fileCreator) == kSymLinkCreator )
			memset(&newUserInfo.fileCreator, 0, sizeof(newUserInfo.fileCreator));
		else
			newUserInfo.fileCreator = newUserInfo.fileCreator;
		
		newFinderInfo = ff.file.finderInfo;
		newFinderInfo.document_id = 0;
		newFinderInfo.date_added = 0;
		newFinderInfo.write_gen_counter = 0;
	}else{
		// Folder don't hace ressource fork
		// Push finder only if there is non zero data in it, excepted non-exposed field.
		newFolderInfo = ff.folder.userInfo;
		newExtendedFolderInfo = ff.folder.finderInfo;
		newExtendedFolderInfo.document_id = 0;
		newExtendedFolderInfo.date_added = 0;
		newExtendedFolderInfo.write_gen_counter = 0;
	}
}

std::vector<std::string> HFSHighLevelVolume::listXattr(const std::string& path)
{
	std::vector<std::string> output;
	HFSPlusCatalogFileOrFolder ff;
	int err;

	// get CNID
	err = m_tree->stat(path, &ff);

	if (err != 0)
		throw file_not_found_error(path);

	uint8_t buf[32];
	const char zero[32] = { 0 };
	getXattrFinderInfo(ff, buf);
	if (  memcmp(buf, zero, 32) != 0 )  // push FinderInfo only is non zero
		output.push_back(XATTR_FINDER_INFO);

	// Push ressource fork only if there is one
	if (be(ff.folder.recordType) == RecordType::kHFSPlusFileRecord  &&  ff.file.resourceFork.logicalSize != 0  &&  !(ff.file.permissions.ownerFlags & HFS_PERM_OFLAG_COMPRESSED)) {
		output.push_back(XATTR_RESOURCE_FORK);
	}

	if (m_volume->attributes())
	{
		for (const auto& kv : m_volume->attributes()->getattr(be(ff.file.fileID))) {
			if (!(ff.file.permissions.ownerFlags & HFS_PERM_OFLAG_COMPRESSED)  ||  kv.first != "com.apple.decmpfs")
				output.push_back(kv.first);
		}
	}

	return output;
}

std::vector<uint8_t> HFSHighLevelVolume::getXattr(const std::string& path, const std::string& name)
{
	int rv;
	std::string spath = path;
	std::vector<uint8_t> output;

	if (string_endsWith(spath, RESOURCE_FORK_SUFFIX))
		spath.resize(spath.length() - strlen(RESOURCE_FORK_SUFFIX));

	if (name == XATTR_RESOURCE_FORK)
	{
		std::shared_ptr<Reader> file;

		rv = m_tree->openFile(spath.c_str(), file, true);
		if (rv == -EISDIR)
			throw operation_not_permitted_error();
		if (rv < 0)
			throw file_not_found_error(path);
			
		if (file->length() == 0)
			throw attribute_not_found_error();
		if (file->length() > std::numeric_limits<int>::max())
			throw io_error("Attribute too big");

		output.resize(file->length());

		file->read(&output[0], int32_t(file->length()), 0);
	}
	else if (name == XATTR_FINDER_INFO)
	{
		HFSPlusCatalogFileOrFolder ff;

		rv = m_tree->stat(spath.c_str(), &ff);
		if (rv != 0)
			throw file_not_found_error(spath);

		uint8_t buf[32];
		const char zero[32] = { 0 };
		getXattrFinderInfo(ff, buf);
		if (  memcmp(buf, zero, 32) != 0 ) // push FinderInfo only is non zero
			output.insert(output.end(), reinterpret_cast<uint8_t*>(buf), reinterpret_cast<uint8_t*>(buf)+32);
	}
	else
	{
		HFSPlusCatalogFileOrFolder ff;

		// get CNID
		rv = m_tree->stat(spath.c_str(), &ff);

		if (rv != 0)
			throw file_not_found_error(spath);

		if (!m_volume->attributes())
			throw attribute_not_found_error();
		if (!m_volume->attributes()->getattr(be(ff.file.fileID), name, output))
			throw attribute_not_found_error();
	}

	return output;
}

void HFSHighLevelVolume::hfs_nativeToStat(const HFSPlusCatalogFileOrFolder& ff, struct stat* stat, bool resourceFork)
{
	assert(stat != nullptr);
	memset(stat, 0, sizeof(*stat));

#if defined(__APPLE__) && !defined(DARLING)
	stat->st_birthtime = HFSCatalogBTree::appleToUnixTime(be(ff.file.createDate));
#endif
	stat->st_atime = HFSCatalogBTree::appleToUnixTime(be(ff.file.accessDate));
	stat->st_mtime = HFSCatalogBTree::appleToUnixTime(be(ff.file.contentModDate));
	stat->st_ctime = HFSCatalogBTree::appleToUnixTime(be(ff.file.attributeModDate));
	stat->st_mode = be(ff.file.permissions.fileMode);
	stat->st_uid = be(ff.file.permissions.ownerID);
	stat->st_gid = be(ff.file.permissions.groupID);
	stat->st_ino = be(ff.file.fileID);
	stat->st_blksize = 512;
	stat->st_nlink = be(ff.file.permissions.special.linkCount);

	if (be(ff.file.recordType) == RecordType::kHFSPlusFileRecord)
	{
		if (!resourceFork)
		{
			stat->st_size = be(ff.file.dataFork.logicalSize);
			stat->st_blocks = be(ff.file.dataFork.totalBlocks);
		}
		else
		{
			stat->st_size = be(ff.file.resourceFork.logicalSize);
			stat->st_blocks = be(ff.file.resourceFork.totalBlocks);
		}

		if (S_ISCHR(stat->st_mode) || S_ISBLK(stat->st_mode))
			stat->st_rdev = be(ff.file.permissions.special.rawDevice);
	}

	if (!stat->st_mode)
	{
		if (be(ff.file.recordType) == RecordType::kHFSPlusFileRecord)
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

decmpfs_disk_header* HFSHighLevelVolume::get_decmpfs(HFSCatalogNodeID cnid, std::vector<uint8_t>& holder)
{
	HFSAttributeBTree* attributes = m_volume->attributes();
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
