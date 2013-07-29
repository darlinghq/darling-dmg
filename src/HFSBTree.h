#ifndef HFSBTREE_H
#define HFSBTREE_H
#include "HFSVolume.h"
#include "HFSFork.h"
#include "hfsplus.h"
#include <cstddef>
#include <vector>
#include "HFSBTreeNode.h"

class HFSBTree
{
public:
	HFSBTree(HFSFork* fork);
	virtual ~HFSBTree();

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

protected:
	HFSBTreeNode traverseTree(int nodeIndex, const Key* indexKey, KeyComparator comp, bool wildcard);
	void walkTree(int nodeIndex);
protected:
	HFSFork* m_fork;
	char* m_tree;
	BTHeaderRec m_header;
};

#endif
