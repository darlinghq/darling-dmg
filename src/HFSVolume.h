#ifndef HFSVOLUME_H
#define HFSVOLUME_H
#include "hfsplus.h"
#include "hfs.h"
#include "Reader.h"
#include <string>
#include <memory>

class HFSCatalogBTree;
class HFSFork;
class HFSExtentsOverflowBTree;
class HFSAttributeBTree;

class HFSVolume
{
public:
	HFSVolume(std::shared_ptr<Reader> reader);
	~HFSVolume();
	
	void usage(uint64_t& totalBytes, uint64_t& freeBytes) const;
	HFSCatalogBTree* rootCatalogTree();

	bool isHFSX() const;
	inline HFSAttributeBTree* attributes() { return m_attributes; }
	inline uint64_t volumeSize() const { return m_reader->length(); }
	
	static bool isHFSPlus(std::shared_ptr<Reader> reader);
private:
	void processEmbeddedHFSPlus(HFSMasterDirectoryBlock* block);
private:
	std::shared_ptr<Reader> m_reader;
	std::shared_ptr<Reader> m_embeddedReader;
	HFSExtentsOverflowBTree* m_overflowExtents;
	HFSAttributeBTree* m_attributes;
	HFSPlusVolumeHeader m_header;
	
	friend class HFSBTree;
	friend class HFSFork;
};

#endif
