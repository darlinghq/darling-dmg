#ifndef HFSBTREE_H
#define HFSBTREE_H
#include "HFSVolume.h"
#include "HFSFork.h"
#include "hfsplus.h"
#include <cstddef>
#include <vector>
#include <memory>
#include "HFSBTreeNode.h"
#include "CachedReader.h"

class HFSBTree
{
public:
	HFSBTree(std::shared_ptr<HFSFork> fork, const char* cacheTag);

	struct Key
	{
		uint16_t length;
		char data[];
	} __attribute__((packed));
	enum CompareResult
	{
		Smaller = -1, Equal = 0, Greater = 1
	};

	// Returns true if the desiredKey >= indexKey
	typedef int (*KeyComparator)(const Key* indexKey, const Key* desiredKey);

	// Used when searching for an exact key (e.g. a specific file in a folder)
	HFSBTreeNode findLeafNode(const Key* indexKey, KeyComparator comp, bool wildcard = false);

	// Sued when searching for an inexact key (e.g. when listing a folder)
	// Return value includes the leaf node where the comparator returns true for the first time when approaching from the right,
	// and all following nodes for which the comparator returns true as well.
	std::vector<HFSBTreeNode> findLeafNodes(const Key* indexKey, KeyComparator comp);
	
	// Set maximum cache size in blocks for all btrees
	static void setMaxCacheBlocks(size_t numBlocks);

protected:
	HFSBTreeNode traverseTree(int nodeIndex, const Key* indexKey, KeyComparator comp, bool wildcard);
	void walkTree(int nodeIndex);
protected:
	std::shared_ptr<HFSFork> m_fork;
	std::shared_ptr<Reader> m_reader;
	//char* m_tree;
	BTHeaderRec m_header;
};

#endif
