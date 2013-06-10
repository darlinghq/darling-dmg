#include "HFSExtentsOverflowBTree.h"
#include "be.h"
#include <stdexcept>

HFSExtentsOverflowBTree::HFSExtentsOverflowBTree(HFSFork* fork)
	: HFSBTree(fork)
{
}

void HFSExtentsOverflowBTree::findExtentsForFile(HFSCatalogNodeID cnid, bool resourceFork, uint32_t startBlock, std::vector<const HFSPlusExtentDescriptor*>& extraExtents)
{
	HFSPlusExtentKey key;
	std::vector<HFSBTreeNode> leaves;
	bool first = true;

	key.forkType = resourceFork ? 0xff : 0;
	key.fileID = cnid;

	leaves = findLeafNodes((Key*) &key, cnidComparator);

	for (const HFSBTreeNode& leaf : leaves)
	{
		for (int i = 0; i < leaf.recordCount(); i++)
		{
			HFSPlusExtentKey* recordKey = leaf.getRecordKey<HFSPlusExtentKey>(i);
			HFSPlusExtentDescriptor* extents;

			if (recordKey->forkType != key.forkType || be(recordKey->fileID) != cnid)
				continue;
			if (be(recordKey->startBlock) < startBlock) // skip descriptors already contained in the extents file
				continue;

			if (first)
			{
				if (be(recordKey->startBlock) != startBlock)
					throw std::runtime_error("Unexpected startBlock value");
				first = false;
			}

			extents = leaf.getRecordData<HFSPlusExtentDescriptor>(i);

			// up to 8 extent descriptors per record
			for (int x = 0; x < 8; x++)
			{
				if (!extents[x].blockCount)
					break;

				extraExtents.push_back(extents + i);
			}
		}
	}
}

bool HFSExtentsOverflowBTree::cnidComparator(const Key* indexKey, const Key* desiredKey)
{
	const HFSPlusExtentKey* indexExtentKey = reinterpret_cast<const HFSPlusExtentKey*>(indexKey);
	const HFSPlusExtentKey* desiredExtentKey = reinterpret_cast<const HFSPlusExtentKey*>(desiredKey);

	if (indexExtentKey->forkType != desiredExtentKey->forkType)
		return false;

	if (be(indexExtentKey->fileID) > desiredExtentKey->fileID)
		return false;

	return true;
}

