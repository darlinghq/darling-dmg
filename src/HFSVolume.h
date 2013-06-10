#ifndef HFSVOLUME_H
#define HFSVOLUME_H
#include "hfsplus.h"
#include "Reader.h"
#include <string>

class HFSCatalogBTree;
class HFSFork;
class HFSExtentsOverflowBTree;

class HFSVolume
{
public:
	HFSVolume(Reader* reader);
	~HFSVolume();
	
	void usage(uint64_t& totalBytes, uint64_t& freeBytes) const;
	HFSCatalogBTree* rootCatalogTree();
	HFSFork* openFile(const std::string& path, bool resourceFork = false);
private:
	Reader* m_reader;
	HFSExtentsOverflowBTree* m_overflowExtents;
	HFSPlusVolumeHeader m_header;
	
	friend class HFSBTree;
	friend class HFSFork;
};

#endif
