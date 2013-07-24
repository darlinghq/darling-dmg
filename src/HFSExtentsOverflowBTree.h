#ifndef HFSEXTENTSOVERFLOWBTREE_H
#define HFSEXTENTSOVERFLOWBTREE_H
#include "HFSBTree.h"
#include "hfsplus.h"
#include <vector>

class HFSExtentsOverflowBTree : protected HFSBTree
{
public:
	HFSExtentsOverflowBTree(HFSFork* fork);
	void findExtentsForFile(HFSCatalogNodeID cnid, bool resourceFork, uint32_t startBlock, std::vector<HFSPlusExtentDescriptor>& extraExtents);
private:
	static CompareResult cnidComparator(const Key* indexKey, const Key* desiredKey);
};

#endif

