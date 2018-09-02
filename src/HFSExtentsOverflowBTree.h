#ifndef HFSEXTENTSOVERFLOWBTREE_H
#define HFSEXTENTSOVERFLOWBTREE_H
#include "HFSBTree.h"
#include "hfsplus.h"
#include "CacheZone.h"
#include <vector>
#include <memory>

class HFSExtentsOverflowBTree : protected HFSBTree
{
public:
	HFSExtentsOverflowBTree(std::shared_ptr<HFSFork> fork, std::shared_ptr<CacheZone> zone);
	void findExtentsForFile(HFSCatalogNodeID cnid, bool resourceFork, uint32_t startBlock, std::vector<HFSPlusExtentDescriptor>& extraExtents);
private:
	static int cnidComparator(const Key* indexKey, const Key* desiredKey);
};

#endif

