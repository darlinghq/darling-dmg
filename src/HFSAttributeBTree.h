#ifndef HFSATTRIBUTEBTREE_H
#define HFSATTRIBUTEBTREE_H
#include "HFSBTree.h"
#include <map>
#include <vector>
#include <string>
#include <stdint.h>

class HFSAttributeBTree : protected HFSBTree
{
public:
	// using HFSBTree::HFSBTree;
	HFSAttributeBTree(HFSFork* fork);
	
	typedef std::map<std::string, std::vector<uint8_t>> AttributeMap;
	
	AttributeMap getattr(HFSCatalogNodeID cnid);
	bool getattr(HFSCatalogNodeID cnid, const std::string& attrName, std::vector<uint8_t>& data);
private:
	static int cnidComparator(const Key* indexKey, const Key* desiredKey);
	static int cnidAttrComparator(const Key* indexKey, const Key* desiredKey);
};

#endif
