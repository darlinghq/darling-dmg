#include "HFSVolume.h"
#include <stdexcept>
#include "be.h"
#include "HFSFork.h"
#include "HFSCatalogBTree.h"
#include "HFSExtentsOverflowBTree.h"
#include "HFSAttributeBTree.h"
#include "SubReader.h"
#include "exceptions.h"

HFSVolume::HFSVolume(std::shared_ptr<Reader> reader)
: m_reader(reader), m_embeddedReader(nullptr), m_overflowExtents(nullptr), m_attributes(nullptr),
  m_fileZone(std::make_shared<CacheZone>(6400)), m_btreeZone(std::make_shared<CacheZone>(6400))
{
	static_assert(sizeof(HFSPlusVolumeHeader) >= sizeof(HFSMasterDirectoryBlock), "Bad read is about to happen");
	
	if (m_reader->read(&m_header, sizeof(m_header), 1024) != sizeof(m_header))
		throw io_error("Cannot read volume header");
	
	if (be(m_header.signature) == HFS_SIGNATURE)
	{
		HFSMasterDirectoryBlock* block = reinterpret_cast<HFSMasterDirectoryBlock*>(&m_header);
		processEmbeddedHFSPlus(block);
	}
	
	if (be(m_header.signature) != HFSP_SIGNATURE && be(m_header.signature) != HFSX_SIGNATURE)
		throw io_error("Invalid HFS+/HFSX signature");

	std::shared_ptr<HFSFork> fork (new HFSFork(this, m_header.extentsFile));
	m_overflowExtents = new HFSExtentsOverflowBTree(fork, m_btreeZone);
	
	if (m_header.attributesFile.logicalSize != 0)
	{
		fork.reset(new HFSFork(this, m_header.attributesFile, kHFSAttributesFileID));
		m_attributes = new HFSAttributeBTree(fork, m_btreeZone);
	}
}

HFSVolume::~HFSVolume()
{
	delete m_attributes;
	delete m_overflowExtents;
	//delete m_embeddedReader;
}

void HFSVolume::processEmbeddedHFSPlus(HFSMasterDirectoryBlock* block)
{
	uint32_t blockSize = be(block->drAlBlkSiz);
	uint64_t offset, length;

	if (be(block->drEmbedSigWord) != HFSP_SIGNATURE && be(block->drEmbedSigWord) != HFSX_SIGNATURE)
		throw function_not_implemented_error("Original HFS is not supported");

	offset = blockSize * be(block->drEmbedExtent.startBlock) + 512 * be(block->drAlBlSt);
	length = blockSize * be(block->drEmbedExtent.blockCount);
	
#ifdef DEBUG
	std::cout << "HFS+ partition is embedded at offset: " << offset << ", length: " << length << std::endl;
#endif
	
	m_embeddedReader.reset(new SubReader(m_reader, offset, length));
	m_reader = m_embeddedReader;
	
	m_reader->read(&m_header, sizeof(m_header), 1024);
}

bool HFSVolume::isHFSPlus(std::shared_ptr<Reader> reader)
{
	HFSPlusVolumeHeader header;
	if (reader->read(&header, sizeof(header), 1024) != sizeof(header))
		return false;
	
	if (be(header.signature) == HFS_SIGNATURE)
	{
		HFSMasterDirectoryBlock* block = reinterpret_cast<HFSMasterDirectoryBlock*>(&header);
		return be(block->drEmbedSigWord) == HFSP_SIGNATURE || be(block->drEmbedSigWord) == HFSX_SIGNATURE;
	}
	
	return be(header.signature) == HFSP_SIGNATURE || be(header.signature) == HFSX_SIGNATURE;
}

bool HFSVolume::isHFSX() const
{
	return be(m_header.signature) == HFSX_SIGNATURE;
}

void HFSVolume::usage(uint64_t& totalBytes, uint64_t& freeBytes) const
{
	totalBytes = be(m_header.blockSize) * be(m_header.totalBlocks);
	freeBytes = be(m_header.blockSize) * be(m_header.freeBlocks);
}

HFSCatalogBTree* HFSVolume::rootCatalogTree()
{
	std::shared_ptr<HFSFork> fork (new HFSFork(this, m_header.catalogFile, kHFSCatalogFileID));
	HFSCatalogBTree* btree = new HFSCatalogBTree(fork, this, m_btreeZone);
	
	return btree;
}


