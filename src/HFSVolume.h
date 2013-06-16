#ifndef HFSVOLUME_H
#define HFSVOLUME_H
#include "hfsplus.h"
#include "hfs.h"
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

	bool isHFSX() const;
	
	static bool isHFSPlus(Reader* reader);
private:
	void processEmbeddedHFSPlus(HFSMasterDirectoryBlock* block);
private:
	Reader* m_reader;
	Reader* m_embeddedReader;
	HFSExtentsOverflowBTree* m_overflowExtents;
	HFSPlusVolumeHeader m_header;
	
	friend class HFSBTree;
	friend class HFSFork;
};

#endif
