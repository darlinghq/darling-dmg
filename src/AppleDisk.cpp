#include "AppleDisk.h"
#include <stdexcept>
#include "be.h"
#include <iostream>
#include "SubReader.h"

AppleDisk::AppleDisk(Reader* reader)
: m_reader(reader)
{
	reader->read(&m_block0, sizeof(m_block0), 0);
	
	if (be(m_block0.sbSig) != BLOCK0_SIGNATURE)
		throw std::runtime_error("Invalid block0 signature");
	
	for (int i = 0; i < 63; i++)
	{
		DPME dpme;
		uint64_t offset = (i+1)*be(m_block0.sbBlkSize);
		Partition part;
		
		reader->read(&dpme, sizeof(dpme), offset);
		
		if (be(dpme.dpme_signature) != DPME_SIGNATURE)
			continue;
		
		//std::cout << "Partition #" << (i+1) << " type: " << dpme.dpme_type << std::endl;
		part.name = dpme.dpme_name;
		part.type = dpme.dpme_type;
		part.offset = uint64_t(be(dpme.dpme_pblock_start)) * be(m_block0.sbBlkSize);
		part.size = uint64_t(be(dpme.dpme_pblocks)) * be(m_block0.sbBlkSize);
		
		m_partitions.push_back(part);
	}
}

bool AppleDisk::isAppleDisk(Reader* reader)
{
	decltype(Block0::sbSig) sig = 0;
	reader->read(&sig, sizeof(sig), 0);
	return be(sig) == BLOCK0_SIGNATURE;
}

Reader* AppleDisk::readerForPartition(unsigned int index)
{
	const Partition& part = m_partitions.at(index);
	return new SubReader(m_reader, part.offset, part.size);
}
