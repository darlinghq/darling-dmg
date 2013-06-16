#include "DMGPartition.h"
#include "be.h"
#include <stdexcept>
#include <cstring>
#include "DMGDecompressor.h"
#include <memory>
#include <algorithm>
//#include <cstdio>
#include <iostream>
#include "SubReader.h"

static const int SECTOR_SIZE = 512;

DMGPartition::DMGPartition(Reader* disk, BLKXTable* table)
: m_disk(disk), m_table(table)
{
	for (uint32_t i = 0; i < be(m_table->blocksRunCount); i++)
	{
		RunType type = RunType(be(m_table->runs[i].type));
		if (type == RunType::Comment || type == RunType::Terminator)
			continue;
		
		m_sectors[be(m_table->runs[i].sectorStart)] = i;
		printf("Sector %d has type 0x%x, starts at byte %d, compressed length %d\n", i, type, be(m_table->runs[i].sectorStart)*512, be(m_table->runs[i].compLength));
	}
}

DMGPartition::~DMGPartition()
{
	delete m_table;
}

int32_t DMGPartition::read(void* buf, int32_t count, uint64_t offset)
{
	int32_t done = 0;
	
	while (done < count)
	{
		std::map<uint64_t, uint32_t>::iterator itRun = m_sectors.upper_bound(offset / SECTOR_SIZE);
		uint64_t offsetInSector = 0;
		int32_t thistime;

		if (itRun == m_sectors.end())
			break; // read beyond EOF
		else if (itRun == m_sectors.begin())
			throw std::runtime_error("Invalid run sector data");
		
		itRun--; // move to the sector we want to read

		std::cout << "Reading from offset " << offset << " " << count << " bytes\n";
		std::cout << "Run sector " << itRun->first << " run index=" << itRun->second << std::endl;
		
		if (!done)
			offsetInSector = offset - itRun->first*SECTOR_SIZE;
		
		thistime = readRun(((char*)buf) + done, itRun->second, offsetInSector, count-done);
		if (!thistime)
			throw std::runtime_error("Unexpected EOF from readRun");
		
		done += thistime;
	}
	
	return done;
}

int32_t DMGPartition::readRun(void* buf, int32_t runIndex, uint64_t offsetInSector, int32_t count)
{
	BLKXRun* run = &m_table->runs[runIndex];
	RunType runType = RunType(be(run->type));
	
	std::cout << "readRun(): offsetInSector = " << offsetInSector << std::endl;
	
	switch (runType)
	{
		case RunType::ZeroFill:
			std::cout << "ZeroFill\n";
			memset(buf, 0, count);
			return count;
		case RunType::Raw:
			std::cout << "Raw\n";
			return m_disk->read(buf, count, be(run->compOffset) + be(m_table->dataStart) + offsetInSector);
		case RunType::Zlib:
		case RunType::Bzip2:
		case RunType::ADC:
		{
			std::unique_ptr<DMGDecompressor> decompressor;
			std::unique_ptr<Reader> subReader;
			uint32_t done = 0;
			
			subReader.reset(new SubReader(m_disk, be(run->compOffset) + be(m_table->dataStart), be(run->compLength)));
			decompressor.reset(DMGDecompressor::create(runType, subReader.get()));
			
			if (!decompressor)
				throw std::logic_error("DMGDecompressor::create() returned nullptr!");
			
			while (done < count)
			{
				int32_t dec;
				
				if (offsetInSector > 0)
				{
					std::unique_ptr<char[]> waste(new char[offsetInSector]);
					dec = decompressor->decompress(waste.get(), offsetInSector);
				}
				else
					dec = decompressor->decompress(((char*)buf)+done, count-done);
				std::cout << "Decompressor returned " << dec << std::endl;
				
				if (dec < 0)
					throw std::runtime_error("Error decompressing stream");
				else if (dec == 0)
					break;
				
				if (offsetInSector > 0)
					offsetInSector -= dec;
				else
					done += dec;
			}
			
			return done;
		}
		default:
			return 0;
	}
}

uint64_t DMGPartition::length()
{
	return be(m_table->sectorCount) * SECTOR_SIZE;
}
