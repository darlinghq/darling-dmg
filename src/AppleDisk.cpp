#include "AppleDisk.h"
#include <stdexcept>
#include "be.h"
#include "exceptions.h"
#include <iostream>
#include "SubReader.h"

AppleDisk::AppleDisk(std::shared_ptr<Reader> reader)
	: m_reader(reader)
{
	load(nullptr);
}

AppleDisk::AppleDisk(std::shared_ptr<Reader> readerBlock0, std::shared_ptr<Reader> readerPM)
: m_reader(readerBlock0)
{
	load(readerPM);
}

void AppleDisk::load(std::shared_ptr<Reader> readerPM)
{
	size_t blockSize;
	
	m_reader->read(&m_block0, sizeof(m_block0), 0);
	
	if (be(m_block0.sbSig) != BLOCK0_SIGNATURE)
		throw io_error("Invalid block0 signature");
	
	blockSize = be(m_block0.sbBlkSize);
	
	if (!blockSize)
	{
		int lastOK = -1;
		blockSize = 512;
		
		for (int i = 0; i < 63; i++)
		{
			DPME dpme;
			uint64_t offset;

			if (!readerPM)
				offset = (i+1)*blockSize;
			else
				offset = i*blockSize;
			
			if (!readerPM)
				m_reader->read(&dpme, sizeof(dpme), offset);
			else
				readerPM->read(&dpme, sizeof(dpme), offset);
			
			if (be(dpme.dpme_signature) != DPME_SIGNATURE)
				continue;
			
			if (lastOK != i-1)
			{
				blockSize *= i - lastOK;
				break;
			}
			lastOK = i;
		}
	}
	
#ifdef DEBUG
	std::cout << "Block size: " << blockSize << std::endl;
#endif
	
	for (int i = 0; i < 63; i++)
	{
		DPME dpme;
		uint64_t offset;
		Partition part;

		if (!readerPM)
			offset = (i+1)*blockSize;
		else
			offset = i*blockSize;
		
		if (!readerPM)
		{
			if (m_reader->read(&dpme, sizeof(dpme), offset) != sizeof(dpme))
				break;
		}
		else
		{
			if (readerPM->read(&dpme, sizeof(dpme), offset) != sizeof(dpme))
				break;
		}
		
		if (be(dpme.dpme_signature) != DPME_SIGNATURE)
			continue;
		
#ifdef DEBUG
		std::cout << "Partition #" << (i+1) << " type: " << dpme.dpme_type << std::endl;
#endif
		part.name = dpme.dpme_name;
		part.type = dpme.dpme_type;
		part.offset = uint64_t(be(dpme.dpme_pblock_start)) * blockSize;
		part.size = uint64_t(be(dpme.dpme_pblocks)) * blockSize;

#ifdef DEBUG
		std::cout << "\tBlock start: " << uint64_t(be(dpme.dpme_pblock_start)) << std::endl;
#endif
		
		m_partitions.push_back(part);
	}
}

bool AppleDisk::isAppleDisk(std::shared_ptr<Reader> reader)
{
	decltype(Block0::sbSig) sig = 0;
	reader->read(&sig, sizeof(sig), 0);
	return be(sig) == BLOCK0_SIGNATURE;
}

std::shared_ptr<Reader> AppleDisk::readerForPartition(int index)
{
	const Partition& part = m_partitions.at(index);
	return std::shared_ptr<Reader>(new SubReader(m_reader, part.offset, part.size));
}
