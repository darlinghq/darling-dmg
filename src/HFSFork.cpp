#include "HFSFork.h"
#include "be.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include "HFSExtentsOverflowBTree.h"

HFSFork::HFSFork(HFSVolume* vol, const HFSPlusForkData& fork, HFSCatalogNodeID cnid, bool resourceFork)
: m_volume(vol), m_fork(fork), m_cnid(cnid), m_resourceFork(resourceFork)
{
	for (int i = 0; i < 8; i++)
	{
		if (m_fork.extents[i].blockCount > 0)
			m_extents.push_back(&m_fork.extents[i]);
	}
}

uint64_t HFSFork::size() const
{
	return be(m_fork.logicalSize);
}

int32_t HFSFork::read(void* buf, int32_t count, uint64_t offset)
{
	const auto blockSize = be(m_volume->m_header.blockSize);
	const uint32_t firstBlock = offset / blockSize;
	uint32_t blocksSoFar;
	int firstExtent, extent;
	int firstBlockInFirstExtent;
	uint32_t read = 0;
	
	if (offset > m_fork.logicalSize)
		count = 0;
	else if (offset+count > m_fork.logicalSize)
		count = m_fork.logicalSize - offset;
	
	if (!count)
		return 0;
	
	do
	{
		firstExtent = -1;
		blocksSoFar = 0;

		// locate the first extent
		for (int i = 0; i < m_extents.size(); i++)
		{
			if (be(m_extents[i]->blockCount) + blocksSoFar > firstBlock)
			{
				firstExtent = i;
				firstBlockInFirstExtent = firstBlock - blocksSoFar;
				break;
			}
		
			blocksSoFar += be(m_extents[i]->blockCount);
		}
	
		std::cout << "First extent: " << firstExtent << std::endl;
		std::cout << "Block: " << firstBlockInFirstExtent << std::endl;
	
		if (firstExtent == -1)
		{
			const size_t oldCount = m_extents.size();

			if (!m_cnid)
				throw std::runtime_error("Cannot search extents file, CNID is kHFSNullID");

			if (oldCount > 8)
				throw std::runtime_error("Loaded extent count > 8, but appropriate extent not found");
			if (oldCount < 8)
				throw std::runtime_error("Loaded extent count < 8, but appropriate extent not found");

			m_volume->m_overflowExtents->findExtentsForFile(m_cnid, m_resourceFork, blocksSoFar, m_extents);
			if (m_extents.size() == oldCount)
				throw std::runtime_error("Overflow extents not found for given CNID");
		}

	} while(firstExtent == -1);
	
	// start reading blocks
	extent = firstExtent;
	while (read < count)
	{
		int32_t thistime = std::min<int32_t>(be(m_extents[extent]->blockCount) * blockSize, count-read);
		int32_t reallyRead;
		uint32_t startBlock;
		uint64_t volumeOffset;
		
		if (extent >= m_extents.size())
			break; // TODO: we need to read the extents file
		
		startBlock = be(m_extents[extent]->startBlock);
		
		if (extent == firstExtent)
			startBlock += firstBlockInFirstExtent;
		
		std::cout << "Reading from block: " << startBlock << std::endl;
		volumeOffset = startBlock * blockSize;
		
		if (extent == firstExtent)
			volumeOffset += offset % blockSize;
		
		reallyRead = m_volume->m_reader->read((char*)buf + read, thistime, volumeOffset);
		
		read += reallyRead;
		
		if (reallyRead != thistime)
			break;
		
		extent++;
	}
	
	return read;
}
