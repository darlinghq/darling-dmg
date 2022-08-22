#include "DMGPartition.h"
#include "be.h"
#include <stdexcept>
#include <cstring>
#include "DMGDecompressor.h"
#include <memory>
#include <algorithm>
//#include <cstdio>
#include <iostream>
#include <limits>
#include "SubReader.h"
#include "exceptions.h"

static const int SECTOR_SIZE = 512;

DMGPartition::DMGPartition(std::shared_ptr<Reader> disk, BLKXTable* table)
: m_disk(disk), m_table(table)
{
	for (uint32_t i = 0; i < be(m_table->blocksRunCount); i++)
	{
		RunType type = RunType(be(m_table->runs[i].type));
		if (type == RunType::Comment || type == RunType::Terminator)
			continue;
#ifdef DEBUG
		assert(be(m_table->runs[i].sectorCount) <= std::numeric_limits<uint64_t>::max() / SECTOR_SIZE);
#endif
		m_sectors[be(m_table->runs[i].sectorStart)] = i;
		
#ifdef DEBUG
		std::cout << "Sector " << i << " has type 0x" << std::hex << uint32_t(type) << std::dec << ", starts at byte "
			<< be(m_table->runs[i].sectorStart)*SECTOR_SIZE << ", compressed length: "
			<< be(m_table->runs[i].compLength) << ", compressed offset: " << be(m_table->runs[i].compOffset) + be(m_table->dataStart) << std::endl;
#endif
	}
}

DMGPartition::~DMGPartition()
{
	delete m_table;
}

void DMGPartition::adviseOptimalBlock(uint64_t offset, uint64_t& blockStart, uint64_t& blockEnd)
{
	std::map<uint64_t, uint32_t>::iterator itRun = m_sectors.upper_bound(offset / SECTOR_SIZE);

	if (itRun == m_sectors.begin())
		throw io_error("Invalid run sector data");

	if (itRun == m_sectors.end())
		blockEnd = length();
	else
		blockEnd = itRun->first * SECTOR_SIZE;

	itRun--;

	blockStart = itRun->first * SECTOR_SIZE;
	
	// Issue #22: empty areas may be larger than 2**31 (causing bugs in callers).
	// Moreover, there is no such thing as "optimal block" in zero-filled areas.
	RunType runType = RunType(be(m_table->runs[itRun->second].type));
	if (runType == RunType::ZeroFill || runType == RunType::Unknown || runType == RunType::Raw)
		Reader::adviseOptimalBlock(offset, blockStart, blockEnd);
}

int32_t DMGPartition::read(void* buf, int32_t count, uint64_t offset)
{

	if ( count < 0 )
		return -1;
	if (offset > length())
		return 0;
	if (count > length() - offset) // here, offset is >= length()  =>  length()-offset >= 0.   (Do not test like ' if (count+offset > length()) ', risk of overflow)
		count = int32_t(length() - offset); // here, length()-offset >= 0 AND < count  =>  length()-offset < INT32_MAX, cast is safe

	int32_t done = 0;
	
	while (done < count)
	{
		std::map<uint64_t, uint32_t>::iterator itRun = m_sectors.upper_bound((offset + done) / SECTOR_SIZE);
		uint64_t offsetInSector = 0;
		int32_t thistime;

		if (offset+done >= length())
			break; // read beyond EOF
		
		if (itRun == m_sectors.begin())
			throw io_error("Invalid run sector data");
		
		itRun--; // move to the sector we want to read

		//std::cout << "Reading from offset " << offset << " " << count << " bytes\n";
		//std::cout << "Run sector " << itRun->first << " run index=" << itRun->second << std::endl;
		
		if (!done)
			offsetInSector = offset - itRun->first*SECTOR_SIZE;
		
		thistime = readRun(((char*)buf) + done, itRun->second, offsetInSector, count-done);
		if (!thistime)
			throw io_error("Unexpected EOF from readRun");
		
		done += thistime;
	}
	
	return done;
}

int32_t DMGPartition::readRun(void* buf, int32_t runIndex, uint64_t offsetInSector, int32_t count)
{
	// readRun is private. Assuming count is > 0
	BLKXRun* run = &m_table->runs[runIndex];
	RunType runType = RunType(be(run->type));
	
	uint64_t compLength = be(run->sectorCount)*SECTOR_SIZE; // no overflow because assert in ctor
	if ( offsetInSector > compLength )
		return 0;

	count = (int32_t)std::min<uint64_t>(count, compLength - offsetInSector); // safe cast, result of min is < count.
	
#ifdef DEBUG
	std::cout << "readRun(): runIndex = " << runIndex << ", offsetInSector = " << offsetInSector << ", count = " << count << std::endl;
#endif
	
	switch (runType)
	{
		case RunType::Unknown: // My guess is that this value indicates a hole in the file (sparse file)
		case RunType::ZeroFill:
			//std::cout << "ZeroFill\n";
			memset(buf, 0, count);
			return count;
		case RunType::Raw:
			//std::cout << "Raw\n";
			return m_disk->read(buf, count, be(run->compOffset) + be(m_table->dataStart) + offsetInSector);
		case RunType::LZFSE:
#ifndef COMPILE_WITH_LZFSE
			throw function_not_implemented_error("LZFSE is not yet supported");
#endif
		case RunType::Zlib:
		case RunType::Bzip2:
		case RunType::ADC:
		{
			std::unique_ptr<DMGDecompressor> decompressor;
			std::shared_ptr<Reader> subReader;
			
			subReader.reset(new SubReader(m_disk, be(run->compOffset) + be(m_table->dataStart), be(run->compLength)));
			decompressor.reset(DMGDecompressor::create(runType, subReader));
			
			if (!decompressor)
				throw std::logic_error("DMGDecompressor::create() returned nullptr!");

			int32_t dec = decompressor->decompress((uint8_t*)buf, count, offsetInSector);
			if (dec < count)
				throw io_error("Error decompressing stream");
			return count;
		}
		default:
			return 0;
	}
}

uint64_t DMGPartition::length()
{
	return be(m_table->sectorCount) * SECTOR_SIZE;
}
