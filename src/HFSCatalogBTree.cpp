#include "HFSCatalogBTree.h"
#include "be.h"
#include <unicode/unistr.h>
#include <sstream>
#include <endian.h>
#include <cstring>
#include "unichar.h"

static const int MAX_SYMLINKS = 50;

HFSCatalogBTree::HFSCatalogBTree(HFSFork* fork, HFSVolume* volume)
	: HFSBTree(fork), m_volume(volume)
{
}

bool HFSCatalogBTree::caseInsensitiveComparator(const Key* indexKey, const Key* desiredKey)
{
	const HFSPlusCatalogKey* catIndexKey = reinterpret_cast<const HFSPlusCatalogKey*>(indexKey);
	const HFSPlusCatalogKey* catDesiredKey = reinterpret_cast<const HFSPlusCatalogKey*>(desiredKey);
	UnicodeString desiredName, indexName;

	if (catDesiredKey->parentID < be(catIndexKey->parentID))
	{
		//std::cout << "desired: " << catDesiredKey->parentID << ", index: " << be(catIndexKey->parentID) << std::endl;
		return false;
	}

	desiredName = UnicodeString((char*)catDesiredKey->nodeName.string, catDesiredKey->nodeName.length*2, "UTF-16BE");
	indexName = UnicodeString((char*)catIndexKey->nodeName.string, be(catIndexKey->nodeName.length)*2, "UTF-16BE");

	if (desiredName.length() > 0 && desiredName.caseCompare(indexName, 0) < 0 && catDesiredKey->parentID < be(catIndexKey->parentID))
	{
		//std::string des, idx;
		//desiredName.toUTF8String(des);
		//indexName.toUTF8String(idx);
		//std::cout << "Rejected, desired: " << des << " - index: " << idx << std::endl;
		return false;
	}

	return true;
}

bool HFSCatalogBTree::idOnlyComparator(const Key* indexKey, const Key* desiredKey)
{
	const HFSPlusCatalogKey* catIndexKey = reinterpret_cast<const HFSPlusCatalogKey*>(indexKey);
	const HFSPlusCatalogKey* catDesiredKey = reinterpret_cast<const HFSPlusCatalogKey*>(desiredKey);

	return catDesiredKey->parentID >= be(catIndexKey->parentID);
}

int HFSCatalogBTree::listDirectory(const std::string& path, std::map<std::string, HFSPlusCatalogFileOrFolder>& contents)
{
	HFSPlusCatalogFileOrFolder dir;
	int rv;
	RecordType recType;
	std::vector<HFSBTreeNode> leaves;
	HFSPlusCatalogKey key;
	std::map<std::string, HFSPlusCatalogFileOrFolder*> beContents;

	contents.clear();

	// determine the CNID of the directory
	rv = stat(path, &dir);
	if (rv != 0)
		return rv;

    if (dir.folder.recordType != RecordType::kHFSPlusFolderRecord)
		return -ENOTDIR;

	// find leaves that may contain directory elements
	key.parentID = dir.folder.folderID;
	leaves = findLeafNodes((Key*) &key, idOnlyComparator); 

	for (const HFSBTreeNode& leaf : leaves)
		findRecordForParentAndName(leaf, key.parentID, beContents);

	for (auto it = beContents.begin(); it != beContents.end(); it++)
	{
		HFSPlusCatalogFileOrFolder native;

		memcpy(&native, it->second, sizeof(native));

#if __BYTE_ORDER == __LITTLE_ENDIAN
		fixEndian(native);
#endif

		contents[it->first] = native;
	}

	return 0;
}

static void split(const std::string &s, char delim, std::vector<std::string>& elems)
{
	std::stringstream ss(s);
	std::string item;

	while (std::getline(ss, item, delim))
		elems.push_back(item);
}

int HFSCatalogBTree::stat(std::string path, HFSPlusCatalogFileOrFolder* s, bool noByteSwap)
{
	std::vector<std::string> elems;
	//HFSCatalogNodeID parent = kHFSRootParentID;
	HFSPlusCatalogFileOrFolder* last = nullptr;

	memset(s, 0, sizeof(*s));

	if (path.compare(0, 1, "/") == 0)
		path = path.substr(1);
	if (!path.empty() && path.compare(path.length()-1, 1, "/") == 0)
		path = path.substr(0, path.length()-1);

	elems.push_back(std::string());
	split(path, '/', elems);

	for (size_t i = 0; i < elems.size(); i++)
	{
		const std::string& elem = elems[i];
		UnicodeString ustr = UnicodeString::fromUTF8(elem);
		HFSBTreeNode leafNode;
		HFSPlusCatalogKey desiredKey;
		HFSCatalogNodeID parentID = last ? be(last->folder.folderID) : kHFSRootParentID;

		if (ustr.length() > 255) // FIXME: there is a UCS-2 vs UTF-16 issue here!
			return -ENAMETOOLONG;

		desiredKey.nodeName.length = ustr.extract(0, ustr.length(), (char*) desiredKey.nodeName.string, "UTF-16BE") / 2;

		desiredKey.parentID = parentID;

		leafNode = findLeafNode((Key*) &desiredKey, caseInsensitiveComparator);
		if (leafNode.isInvalid())
			return -ENOENT;

		last = findRecordForParentAndName(leafNode, parentID, elem);
		if (!last)
			return -ENOENT;

		// resolve symlinks, check if directory...
		// FUSE takes care of this
		/*
		{
			RecordType recType = RecordType(be(uint16_t(last->folder.recordType)));
			const bool isLastElem = i+1 == elems.size();

			if (recType == RecordType::kHFSPlusFileRecord)
			{
				if (last->file.permissions.fileMode & HFSPLUS_S_IFLNK && (!lstat || !isLastElem))
				{
					if (currentDepth >= MAX_SYMLINKS)
						return -ELOOP;
					// TODO: deal with symlinks
					// recurse with increased depth
				}
				else if (!isLastElem)
					return -ENOTDIR;
			}
		}
		*/

		//parent = last->folder.folderID;
	}

	memcpy(s, last, sizeof(*s));

#if __BYTE_ORDER == __LITTLE_ENDIAN
	if (!noByteSwap)
		fixEndian(*s);
#endif

	return 0;
}
HFSPlusCatalogFileOrFolder* HFSCatalogBTree::findRecordForParentAndName(const HFSBTreeNode& leafNode, HFSCatalogNodeID cnid, const std::string& name)
{
	std::map<std::string, HFSPlusCatalogFileOrFolder*> result;
	findRecordForParentAndName(leafNode, cnid, result, name);

	if (result.empty())
		return nullptr;
	else
		return result.begin()->second;
}

void HFSCatalogBTree::findRecordForParentAndName(const HFSBTreeNode& leafNode, HFSCatalogNodeID cnid, std::map<std::string, HFSPlusCatalogFileOrFolder*>& result, const std::string& name)
{
	for (int i = 0; i < leafNode.recordCount(); i++)
	{
		HFSPlusCatalogKey* recordKey;
		HFSPlusCatalogFileOrFolder* ff;
		uint16_t recordOffset;
		RecordType recType;

		recordKey = leafNode.getRecordKey<HFSPlusCatalogKey>(i);
		ff = leafNode.getRecordData<HFSPlusCatalogFileOrFolder>(i);

		recType = RecordType(be(uint16_t(ff->folder.recordType)));

		switch (recType)
		{
			case RecordType::kHFSPlusFolderRecord:
			case RecordType::kHFSPlusFileRecord:
			{
				// skip "\0\0HFS+ Private Data"
				if (recordKey->nodeName.string[0] != 0)
				{
					if (be(recordKey->parentID) == cnid && (name.empty() || EqualNoCase(recordKey->nodeName, name)))
					{
						std::string name = UnicharToString(recordKey->nodeName);
						result[name] = ff;
					}
				}
				break;
			}
			case RecordType::kHFSPlusFolderThreadRecord:
			case RecordType::kHFSPlusFileThreadRecord:
				break;
		}
	}
}


void HFSCatalogBTree::fixEndian(HFSPlusCatalogFileOrFolder& ff)
{
#define swap(x) ff.folder.x = be(ff.folder.x)
	ff.folder.recordType = RecordType(be(uint16_t(ff.folder.recordType)));
	swap(flags);
	swap(valence);
	swap(folderID);
	swap(createDate);
	swap(contentModDate);
	swap(attributeModDate);
	swap(accessDate);
	swap(backupDate);
	swap(textEncoding);
	swap(reserved);
	swap(permissions.ownerID);
	swap(permissions.groupID);
	//swap(permissions.adminFlags);
	//swap(permissions.ownerFlags);
	swap(permissions.special.rawDevice);

#undef swap
#define swap(x) ff.file.x = be(ff.file.x)

	if (ff.file.recordType == RecordType::kHFSPlusFileRecord)
	{
		swap(dataFork.logicalSize);
		swap(dataFork.clumpSize);
		swap(dataFork.totalBlocks);

		swap(resourceFork.logicalSize);
		swap(resourceFork.clumpSize);
		swap(resourceFork.totalBlocks);
	}
#undef swap
}

time_t HFSCatalogBTree::appleToUnixTime(uint32_t apple)
{
	const time_t offset = 2082844800L;
	return apple + offset;
}

int HFSCatalogBTree::openFile(const std::string& path, Reader** forkOut, bool resourceFork)
{
	HFSPlusCatalogFileOrFolder ff;
	int rv;

	*forkOut = nullptr;

	rv = stat(path, &ff, true);
	if (rv < 0)
		return rv;

	if (RecordType(be(uint16_t(ff.folder.recordType))) != RecordType::kHFSPlusFileRecord)
		return -EISDIR;

	*forkOut = new HFSFork(m_volume, resourceFork ? ff.file.resourceFork : ff.file.dataFork,
		ff.file.fileID, resourceFork);

	return 0;
}

