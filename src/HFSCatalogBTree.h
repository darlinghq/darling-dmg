#ifndef HFSCATALOGBTREE_H
#define HFSCATALOGBTREE_H
#include "HFSBTree.h"
#include <string>
#include <vector>
#include <map>
#include <ctime>
#include "hfsplus.h"
#include "HFSBTreeNode.h"

class HFSCatalogBTree : protected HFSBTree
{
public:
	// using HFSBTree::HFSBTree;
	HFSCatalogBTree(HFSFork* fork, HFSVolume* volume);

	int listDirectory(const std::string& path, std::map<std::string, HFSPlusCatalogFileOrFolder>& contents);
	int stat(std::string path, HFSPlusCatalogFileOrFolder* s, bool noByteSwap = false);
	int openFile(const std::string& path, Reader** forkOut, bool resourceFork = false);

	static time_t appleToUnixTime(uint32_t apple);
protected:
	static HFSPlusCatalogFileOrFolder* findRecordForParentAndName(const HFSBTreeNode& leafNode, HFSCatalogNodeID cnid, const std::string& name);
	std::string readSymlink(HFSPlusCatalogFile* file);

	// does not clear the result argument
	static void findRecordForParentAndName(const HFSBTreeNode& leafNode, HFSCatalogNodeID cnid, std::map<std::string, HFSPlusCatalogFileOrFolder*>& result, const std::string& name = std::string());
private:
	static bool caseInsensitiveComparator(const Key* indexKey, const Key* desiredKey);
	static bool idOnlyComparator(const Key* indexKey, const Key* desiredKey);
	static void fixEndian(HFSPlusCatalogFileOrFolder& ff);
private:
	HFSVolume* m_volume;
};

#endif

