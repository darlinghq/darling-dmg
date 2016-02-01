#ifndef HFSCATALOGBTREE_H
#define HFSCATALOGBTREE_H
#include "HFSBTree.h"
#include <string>
#include <vector>
#include <map>
#include <ctime>
#include "hfsplus.h"
#include "HFSBTreeNode.h"
#include "CacheZone.h"
#include <memory>

class HFSCatalogBTree : protected HFSBTree
{
public:
	// using HFSBTree::HFSBTree;
	HFSCatalogBTree(std::shared_ptr<HFSFork> fork, HFSVolume* volume, CacheZone* zone);

	int listDirectory(const std::string& path, std::map<std::string, HFSPlusCatalogFileOrFolder>& contents);
	int stat(std::string path, HFSPlusCatalogFileOrFolder* s, bool noByteSwap = false);
	int openFile(const std::string& path, std::shared_ptr<Reader>& forkOut, bool resourceFork = false);

	bool isCaseSensitive() const;
	
	// Debug only
	void dumpTree() const;

	static time_t appleToUnixTime(uint32_t apple);
protected:
	HFSPlusCatalogFileOrFolder* findRecordForParentAndName(const HFSBTreeNode& leafNode, HFSCatalogNodeID cnid, const std::string& name);
	std::string readSymlink(HFSPlusCatalogFile* file);

	// does not clear the result argument
	void findRecordForParentAndName(const HFSBTreeNode& leafNode, HFSCatalogNodeID cnid, std::map<std::string, HFSPlusCatalogFileOrFolder*>& result, const std::string& name = std::string());
private:
	static int caseInsensitiveComparator(const Key* indexKey, const Key* desiredKey);
	static int caseSensitiveComparator(const Key* indexKey, const Key* desiredKey);
	static int idOnlyComparator(const Key* indexKey, const Key* desiredKey);
	static void fixEndian(HFSPlusCatalogFileOrFolder& ff);
	static void replaceChars(std::string& str, char oldChar, char newChar);
	
	void dumpTree(int nodeIndex, int depth) const;
private:
	HFSVolume* m_volume;
};

#endif

