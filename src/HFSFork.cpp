#include "HFSFork.h"
#include "be.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <cassert>
#include "exceptions.h"
#include "HFSExtentsOverflowBTree.h"

HFSFork::HFSFork(HFSVolume* vol, const HFSPlusForkData& fork, HFSCatalogNodeID cnid, bool resourceFork)
: m_volume(vol), m_fork(fork), m_cnid(cnid), m_resourceFork(resourceFork)
{
	for (int i = 0; i < 8; i++)
	{
		auto& elem = m_fork.extents[i];
		if (elem.blockCount > 0)
			m_extents.push_back(HFSPlusExtentDescriptor{ be(elem.startBlock), be(elem.blockCount) });
	}
}

uint64_t HFSFork::length()
{
	return be(m_fork.logicalSize);
}

void HFSFork::loadFromOverflowsFile(uint32_t blocksSoFar)
{
	const size_t oldCount = m_extents.size();

	if (!m_cnid)
		throw std::logic_error("Cannot search extents file, CNID is kHFSNullID");

//	if (oldCount > 8)
//		throw io_error("Loaded extent count > 8, but appropriate extent not found");
	if (oldCount < 8)
		throw io_error("Loaded extent count < 8, but appropriate extent not found");

	m_volume->m_overflowExtents->findExtentsForFile(m_cnid, m_resourceFork, blocksSoFar, m_extents);
	if (m_extents.size() == oldCount)
		throw io_error("Overflow extents not found for given CNID");
}

int32_t HFSFork::read(void* buf, int32_t count, uint64_t offset)
{
	const auto blockSize = be(m_volume->m_header.blockSize);
	const uint32_t firstBlock = offset / blockSize;
	uint32_t blocksSoFar;
	int firstExtent, extent;
	int firstBlockInFirstExtent;
	uint32_t read = 0;
	
	if (offset > be(m_fork.logicalSize))
		count = 0;
	else if (offset+count > be(m_fork.logicalSize))
		count = be(m_fork.logicalSize) - offset;
	
	if (!count)
		return 0;
	
	do
	{
		firstExtent = -1;
		blocksSoFar = 0;

		// locate the first extent
		for (int i = 0; i < m_extents.size(); i++)
		{
			if (m_extents[i].blockCount + blocksSoFar > firstBlock)
			{
				firstExtent = i;
				firstBlockInFirstExtent = firstBlock - blocksSoFar;
				break;
			}
		
			blocksSoFar += m_extents[i].blockCount;
		}
	
		//std::cout << "First extent: " << firstExtent << std::endl;
		//std::cout << "Block: " << firstBlockInFirstExtent << std::endl;
	
		if (firstExtent == -1)
			loadFromOverflowsFile(blocksSoFar);

	} while(firstExtent == -1);
	
	// start reading blocks
	extent = firstExtent;
	while (read < count && read+offset < length())
	{
		int32_t thistime;
		int32_t reallyRead;
		uint32_t startBlock;
		uint64_t volumeOffset;
		
		//thistime = count-read;
		
		if (extent >= m_extents.size())
			loadFromOverflowsFile(blocksSoFar);
		
		thistime = std::min<int64_t>(m_extents[extent].blockCount * uint64_t(blockSize), count-read);
		
		startBlock = m_extents[extent].startBlock;
		
		if (extent == firstExtent)
			startBlock += firstBlockInFirstExtent;
		
		if (thistime == 0)
			throw std::logic_error("Internal error: thistime == 0");
		
		//std::cout << "Remaining to read: " << count-read << std::endl;
		//std::cout << "Extent " << extent << " has " << m_extents[extent].blockCount << " blocks\n";
		//std::cout << "This extent holds " << m_extents[extent].blockCount * uint64_t(blockSize) << " bytes\n";	
		//std::cout << "Reading " << thistime << " from block: " << startBlock << ", block size: " << blockSize <<  std::endl;
		volumeOffset = uint64_t(startBlock) * blockSize;
		
		if (extent == firstExtent)
			volumeOffset += offset % blockSize;
		
		reallyRead = m_volume->m_reader->read((char*)buf + read, thistime, volumeOffset);
		assert(reallyRead <= thistime);
		
		read += reallyRead;
		
		if (reallyRead != thistime)
		{
			//std::cerr << "Short read: " << thistime << " expected, " << reallyRead << " received\n";
			break;
		}
		
		blocksSoFar += m_extents[extent].blockCount;
		//std::cout << "Blocks so far: " << blocksSoFar << std::endl;
		extent++;
	}
	
	assert(read <= count);
	
	return read;
}
