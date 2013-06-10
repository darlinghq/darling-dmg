#ifndef HFSCATALOGBTREE_H
#define HFSCATALOGBTREE_H
#include "HFSBTree.h"
#include <string>
#include <vector>
#include "HFSBTreeNode.h"

class HFSCatalogBTree : protected HFSBTree
{
public:
	// using HFSBTree::HFSBTree;
	HFSCatalogBTree(HFSFork* fork);	

	int listDirectory(const std::string& path, std::vector<HFSPlusCatalogFileOrFolder*>& contents);
	int stat(std::string path, HFSPlusCatalogFileOrFolder** s, bool lstat = false);
protected:
	int stat(std::string path, HFSPlusCatalogFileOrFolder** s, bool lstat, int currentDepth);
	static HFSPlusCatalogFileOrFolder* findRecordForParentAndName(const HFSBTreeNode& leafNode, HFSCatalogNodeID cnid, const std::string& name);
	std::string readSymlink(HFSPlusCatalogFile* file);

	// does not clear the result argument
	static void findRecordForParentAndName(const HFSBTreeNode& leafNode, HFSCatalogNodeID cnid, std::vector<HFSPlusCatalogFileOrFolder*>& result, const std::string& name = std::string());
private:
	static bool caseInsensitiveComparator(const Key* indexKey, const Key* desiredKey);
	static bool idOnlyComparator(const Key* indexKey, const Key* desiredKey);
};

#endif

