#include "HFSCatalogBTree.h"
#include "be.h"
#include <unicode/unistr.h>
#include <sstream>
#include "unichar.h"

static const int MAX_SYMLINKS = 50;

HFSCatalogBTree::HFSCatalogBTree(HFSFork* fork)
	: HFSBTree(fork)
{
}

bool HFSCatalogBTree::caseInsensitiveComparator(const Key* indexKey, const Key* desiredKey)
{
	const HFSPlusCatalogKey* catIndexKey = reinterpret_cast<const HFSPlusCatalogKey*>(&indexKey);
	const HFSPlusCatalogKey* catDesiredKey = reinterpret_cast<const HFSPlusCatalogKey*>(&desiredKey);
	UnicodeString desiredName, indexName;

	if (catDesiredKey->parentID < be(catIndexKey->parentID))
		return false;

	desiredName = UnicodeString((char*)catDesiredKey->nodeName.string, catDesiredKey->nodeName.length, "UTF-16BE");
	indexName = UnicodeString((char*)catIndexKey->nodeName.string, be(catIndexKey->nodeName.length), "UTF-16BE");

	if (desiredName.caseCompare(indexName, 0) < 0)
		return false;

	return true;
}

bool HFSCatalogBTree::idOnlyComparator(const Key* indexKey, const Key* desiredKey)
{
	const HFSPlusCatalogKey* catIndexKey = reinterpret_cast<const HFSPlusCatalogKey*>(&indexKey);
	const HFSPlusCatalogKey* catDesiredKey = reinterpret_cast<const HFSPlusCatalogKey*>(&desiredKey);

	return catDesiredKey->parentID >= be(catIndexKey->parentID);
}

int HFSCatalogBTree::listDirectory(const std::string& path, std::vector<HFSPlusCatalogFileOrFolder*>& contents)
{
	HFSPlusCatalogFileOrFolder* dir = nullptr;
	int rv;
	RecordType recType;
	std::vector<HFSBTreeNode> leaves;
	HFSPlusCatalogKey key;

	contents.clear();

	// determine the CNID of the directory
	rv = stat(path, &dir);
	if (rv != 0)
		return rv;

	recType = RecordType(be(uint16_t(dir->folder.recordType)));

    if (recType != RecordType::kHFSPlusFolderRecord)
		return -ENOTDIR;

	// find leaves that may contain directory elements
	key.parentID = dir->folder.folderID;
	leaves = findLeafNodes((Key*) &key, idOnlyComparator); 

	for (const HFSBTreeNode& leaf : leaves)
		findRecordForParentAndName(leaf, key.parentID, contents);

	return 0;
}

static std::vector<std::string> split(const std::string &s, char delim)
{
	std::vector<std::string> elems;
	std::stringstream ss(s);
	std::string item;

	while (std::getline(ss, item, delim))
		elems.push_back(item);
    
	return elems;
}
int HFSCatalogBTree::stat(std::string path, HFSPlusCatalogFileOrFolder** s, bool lstat)
{
	return stat(path, s, lstat, 0);
}

int HFSCatalogBTree::stat(std::string path, HFSPlusCatalogFileOrFolder** s, bool lstat, int currentDepth)
{
	std::vector<std::string> elems;
	HFSCatalogNodeID parent = kHFSRootParentID;
	HFSPlusCatalogFileOrFolder* last = nullptr;

	*s = nullptr;

	if (path.compare(0, 1, "/"))
		path = path.substr(1);

	elems = split(path, '/');

	for (size_t i = 0; i < elems.size(); i++)
	{
		const std::string& elem = elems[i];
		UnicodeString ustr = UnicodeString::fromUTF8(elem);
		HFSBTreeNode leafNode;
		HFSPlusCatalogKey desiredKey;

		if (ustr.length() > 255) // FIXME: there is a UCS-2 vs UTF-16 issue here!
			return -ENAMETOOLONG;

		desiredKey.nodeName.length = ustr.extract(0, ustr.length(), (char*) desiredKey.nodeName.string, "UTF-16BE");
		desiredKey.parentID = parent;

		leafNode = findLeafNode((Key*) &desiredKey, caseInsensitiveComparator);
		if (leafNode.isInvalid())
			return -ENOENT;

		last = findRecordForParentAndName(leafNode, parent, elem);
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

		parent = last->folder.folderID;
	}

	*s = last;
	return 0;
}
HFSPlusCatalogFileOrFolder* HFSCatalogBTree::findRecordForParentAndName(const HFSBTreeNode& leafNode, HFSCatalogNodeID cnid, const std::string& name)
{
	std::vector<HFSPlusCatalogFileOrFolder*> result;
	findRecordForParentAndName(leafNode, cnid, result, name);

	if (result.empty())
		return nullptr;
	else
		return result[0];
}

void HFSCatalogBTree::findRecordForParentAndName(const HFSBTreeNode& leafNode, HFSCatalogNodeID cnid, std::vector<HFSPlusCatalogFileOrFolder*>& result, const std::string& name)
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
				if (be(recordKey->parentID) == cnid && (name.empty() || EqualNoCase(recordKey->nodeName, name)))
					result.push_back(ff);
				break;
			}
			case RecordType::kHFSPlusFolderThreadRecord:
			case RecordType::kHFSPlusFileThreadRecord:
				break;
		}
	}
}


