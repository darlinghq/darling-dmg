#include "HFSCatalogBTree.h"
#include "be.h"
#include <unicode/unistr.h>
#include <sstream>
#ifdef __FreeBSD__
#include <sys/endian.h>
#else
#include <endian.h>
#endif
#include <cstring>
#include "unichar.h"

static const int MAX_SYMLINKS = 50;

extern UConverter *g_utf16be;

HFSCatalogBTree::HFSCatalogBTree(std::shared_ptr<HFSFork> fork, HFSVolume* volume, CacheZone* zone)
	: HFSBTree(fork, zone, "Catalog"), m_volume(volume)
{
}

bool HFSCatalogBTree::isCaseSensitive() const
{
	return m_volume->isHFSX() && m_header.keyCompareType == KeyCompareType::kHFSBinaryCompare;
}

int HFSCatalogBTree::caseInsensitiveComparator(const Key* indexKey, const Key* desiredKey)
{
	const HFSPlusCatalogKey* catIndexKey = reinterpret_cast<const HFSPlusCatalogKey*>(indexKey);
	const HFSPlusCatalogKey* catDesiredKey = reinterpret_cast<const HFSPlusCatalogKey*>(desiredKey);
	UnicodeString desiredName, indexName;
	UErrorCode error = U_ZERO_ERROR;

	//std::cout << "desired: " << be(catDesiredKey->parentID) << ", index: " << be(catIndexKey->parentID) << "\n";
	if (be(catDesiredKey->parentID) < be(catIndexKey->parentID))
	{
		//std::cout << "\t -> bigger\n";
		return 1;
	}
	else if (be(catDesiredKey->parentID) > be(catIndexKey->parentID))
	{
		//std::cout << "\t -> smaller\n";
		return -1;
	}

	desiredName = UnicodeString((char*)catDesiredKey->nodeName.string, be(catDesiredKey->nodeName.length)*2, g_utf16be, error);
	indexName = UnicodeString((char*)catIndexKey->nodeName.string, be(catIndexKey->nodeName.length)*2, g_utf16be, error);
	
	// Hack for "\0\0\0\0HFS+ Private Data" which should come as last in ordering (issue #11)
	if (indexName.charAt(0) == 0)
		return 1;
	else if (desiredName.charAt(0) == 0)
		return -1;
	
	{
		//std::string des, idx;
		//desiredName.toUTF8String(des);
		//indexName.toUTF8String(idx);
		
		int r = indexName.caseCompare(desiredName, 0);
		
		//std::cout << "desired: " << des << " - index: " << idx << " -> r=" << r << std::endl;

		return r;
	}
	
	return 0;
}

int HFSCatalogBTree::caseSensitiveComparator(const Key* indexKey, const Key* desiredKey)
{
	const HFSPlusCatalogKey* catIndexKey = reinterpret_cast<const HFSPlusCatalogKey*>(indexKey);
	const HFSPlusCatalogKey* catDesiredKey = reinterpret_cast<const HFSPlusCatalogKey*>(desiredKey);
	UnicodeString desiredName, indexName;
	UErrorCode error = U_ZERO_ERROR;

	if (be(catDesiredKey->parentID) < be(catIndexKey->parentID))
		return 1;
	else if (be(catDesiredKey->parentID) > be(catIndexKey->parentID))
		return -1;

	desiredName = UnicodeString((char*)catDesiredKey->nodeName.string, be(catDesiredKey->nodeName.length)*2, g_utf16be, error);
	indexName = UnicodeString((char*)catIndexKey->nodeName.string, be(catIndexKey->nodeName.length)*2, g_utf16be, error);
	
	// Hack for "\0\0\0\0HFS+ Private Data" which should come as last in ordering (issue #11)
	if (indexName.charAt(0) == 0)
		return 1;
	else if (desiredName.charAt(0) == 0)
		return 1;

	if (desiredName.length() > 0)
	{
		//std::string des, idx;
		//desiredName.toUTF8String(des);
		//indexName.toUTF8String(idx);
		
		int r = indexName.caseCompare(desiredName, 0);
		
		// std::cout << "desired: " << des << " - index: " << idx << " -> r=" << r << std::endl;

		return r;
	}

	return 0;
}

int HFSCatalogBTree::idOnlyComparator(const Key* indexKey, const Key* desiredKey)
{
	const HFSPlusCatalogKey* catIndexKey = reinterpret_cast<const HFSPlusCatalogKey*>(indexKey);
	const HFSPlusCatalogKey* catDesiredKey = reinterpret_cast<const HFSPlusCatalogKey*>(desiredKey);
	
	//std::cerr << "idOnly: desired: " << be(catDesiredKey->parentID) << ", index: " << be(catIndexKey->parentID) << std::endl;

	if (be(catDesiredKey->parentID) > be(catIndexKey->parentID))
		return -1;
	else if (be(catIndexKey->parentID) > be(catDesiredKey->parentID))
		return 1;
	else
		return 0;
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
	key.parentID = htobe32(dir.folder.folderID);
	leaves = findLeafNodes((Key*) &key, idOnlyComparator);

	for (const HFSBTreeNode& leaf : leaves)
	{
		//std::cerr << "**** Looking for elems with CNID " << be(key.parentID) << std::endl;
		findRecordForParentAndName(leaf, be(key.parentID), beContents);
	}

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
	HFSBTreeNode leafNode;
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
		//UnicodeString ustr = UnicodeString::fromUTF8(elem);
		HFSPlusCatalogKey desiredKey;
		HFSCatalogNodeID parentID = last ? be(last->folder.folderID) : kHFSRootParentID;

		//if (ustr.length() > 255) // FIXME: there is a UCS-2 vs UTF-16 issue here!
		//	return -ENAMETOOLONG;

		desiredKey.nodeName.length = StringToUnichar(elem, desiredKey.nodeName.string, sizeof(desiredKey.nodeName.string));
		//desiredKey.nodeName.length = ustr.extract(0, ustr.length(), (char*) desiredKey.nodeName.string, "UTF-16BE") / 2;
		desiredKey.nodeName.length = htobe16(desiredKey.nodeName.length);

		desiredKey.parentID = htobe32(parentID);

		leafNode = findLeafNode((Key*) &desiredKey, isCaseSensitive() ? caseSensitiveComparator : caseInsensitiveComparator);
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
	
	//std::cout << "File/folder flags: 0x" << std::hex << s->file.flags << std::endl;

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
		//{
			//std::string name = UnicharToString(recordKey->nodeName);
			//std::cerr << "RecType " << int(recType) << ", ParentID: " << be(recordKey->parentID) << ", nodeName " << name << std::endl;
		//}

		switch (recType)
		{
			case RecordType::kHFSPlusFolderRecord:
			case RecordType::kHFSPlusFileRecord:
			{
				// skip "\0\0HFS+ Private Data"
				if (recordKey->nodeName.string[0] != 0 && be(recordKey->parentID) == cnid)
				{
					bool equal = name.empty();
					if (!equal)
					{
						if (isCaseSensitive())
							equal = EqualCase(recordKey->nodeName, name);
						else
							equal = EqualNoCase(recordKey->nodeName, name);
					}

					if (equal)
					{
						std::string name = UnicharToString(recordKey->nodeName);
						result[name] = ff;
					}
				}
				//else
				//	std::cerr << "CNID not matched - " << cnid << " required\n";
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
	swap(permissions.fileMode);
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
	return apple - offset;
}

int HFSCatalogBTree::openFile(const std::string& path, std::shared_ptr<Reader>& forkOut, bool resourceFork)
{
	HFSPlusCatalogFileOrFolder ff;
	int rv;

	forkOut.reset();

	rv = stat(path, &ff, true);
	if (rv < 0)
		return rv;

	if (RecordType(be(uint16_t(ff.folder.recordType))) != RecordType::kHFSPlusFileRecord)
		return -EISDIR;

	forkOut.reset(new HFSFork(m_volume, resourceFork ? ff.file.resourceFork : ff.file.dataFork,
		be(ff.file.fileID), resourceFork));

	return 0;
}

void HFSCatalogBTree::dumpTree() const
{
	dumpTree(be(m_header.rootNode), 0);
}

void HFSCatalogBTree::dumpTree(int nodeIndex, int depth) const
{
	HFSBTreeNode node(m_reader, nodeIndex, be(m_header.nodeSize));
	
	switch (node.kind())
	{
		case NodeKind::kBTIndexNode:
		{
			for (size_t i = 0; i < node.recordCount(); i++)
			{
				UErrorCode error = U_ZERO_ERROR;
				HFSPlusCatalogKey* key = node.getRecordKey<HFSPlusCatalogKey>(i);
				UnicodeString keyName((char*)key->nodeName.string, be(key->nodeName.length)*2, g_utf16be, error);
				std::string str;
				
				keyName.toUTF8String(str);
				
#ifdef DEBUG
				std::cout << "dumpTree(i): " << std::string(depth, ' ') << str << "(" << be(key->parentID) << ")\n";
#endif
				
				// recurse down
				uint32_t* childIndex = node.getRecordData<uint32_t>(i);
				dumpTree(be(*childIndex), depth+1);
			}
			
			break;
		}
		case NodeKind::kBTLeafNode:
		{
			for (int i = 0; i < node.recordCount(); i++)
			{
				HFSPlusCatalogKey* recordKey;
				UErrorCode error = U_ZERO_ERROR;
				UnicodeString keyName;
				std::string str;
				
				recordKey = node.getRecordKey<HFSPlusCatalogKey>(i);
				keyName = UnicodeString((char*)recordKey->nodeName.string, be(recordKey->nodeName.length)*2, g_utf16be, error);
				keyName.toUTF8String(str);
				
#ifdef DEBUG
				std::cout << "dumpTree(l): " << std::string(depth, ' ') << str << "(" << be(recordKey->parentID) << ")\n";
#endif
			}
			
			break;
		}
		case NodeKind::kBTHeaderNode:
		case NodeKind::kBTMapNode:
			break;
		default:
			std::cerr << "Invalid node kind! Kind: " << int(node.kind()) << std::endl;
			
	}
}
