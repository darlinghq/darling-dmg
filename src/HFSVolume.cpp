#include "HFSVolume.h"
#include <stdexcept>
#include "be.h"
#include "HFSFork.h"
#include "HFSCatalogBTree.h"
#include "HFSExtentsOverflowBTree.h"

HFSVolume::HFSVolume(Reader* reader)
: m_reader(reader), m_overflowExtents(nullptr)
{
	reader->read(&m_header, sizeof(m_header), 1024);
	
	if (be(m_header.signature) != HFSP_SIGNATURE && be(m_header.signature) != HFSX_SIGNATURE)
		throw std::runtime_error("Invalid HFS signature");

	HFSFork* fork = new HFSFork(this, m_header.extentsFile);
	m_overflowExtents = new HFSExtentsOverflowBTree(fork);
}

HFSVolume::~HFSVolume()
{
	delete m_overflowExtents;
}

void HFSVolume::usage(uint64_t& totalBytes, uint64_t& freeBytes) const
{
	totalBytes = be(m_header.blockSize) * be(m_header.totalBlocks);
	freeBytes = be(m_header.blockSize) * be(m_header.freeBlocks);
}

HFSCatalogBTree* HFSVolume::rootCatalogTree()
{
	HFSFork* fork = new HFSFork(this, m_header.catalogFile);
	HFSCatalogBTree* btree = new HFSCatalogBTree(fork);
	
	return btree;
}

HFSFork* HFSVolume::openFile(const std::string& path, bool resourceFork)
{
	return nullptr;
}

