#ifndef HFSATTRIBUTEBTREE_H
#define HFSATTRIBUTEBTREE_H
#include "HFSBTree.h"
#include "CacheZone.h"
#include <map>
#include <vector>
#include <string>
#include <memory>
#include <stdint.h>

class HFSAttributeBTree : protected HFSBTree
{
public:
	HFSAttributeBTree(std::shared_ptr<HFSFork> fork, std::shared_ptr<CacheZone> zone);
	
	typedef std::map<std::string, std::vector<uint8_t>> AttributeMap;
	
	AttributeMap getattr(HFSCatalogNodeID cnid);
	bool getattr(HFSCatalogNodeID cnid, const std::string& attrName, std::vector<uint8_t>& data);
private:
	static int cnidComparator(const Key* indexKey, const Key* desiredKey);
	static int cnidAttrComparator(const Key* indexKey, const Key* desiredKey);
};

#endif
