#include "DMGPartition.h"
#include "be.h"
#include "adc.h"
#include <stdexcept>
#include <cstring>
#include <zlib.h>
#include <bzlib.h>
#include <memory>
#include <algorithm>

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
		
		if (itRun == m_sectors.end())
			break; // read beyond EOF
		else if (itRun == m_sectors.begin())
			throw std::runtime_error("Invalid run sector data");
		
		itRun--; // move to the sector we want to read
		
		if (!done)
			offsetInSector = itRun->first*SECTOR_SIZE - offset;
		
		done += readRun(((char*)buf) + done, itRun->second, offsetInSector, count-done);
	}
	
	return done;
}

int32_t DMGPartition::readRun(void* buf, int32_t runIndex, uint64_t offsetInSector, int32_t count)
{
	BLKXRun* run = &m_table->runs[runIndex];
	RunType runType = RunType(be(run->type));
	switch (runType)
	{
		case RunType::ZeroFill:
			memset(buf, 0, count);
			return count;
		case RunType::Raw:
			return m_disk->read(buf, count, be(run->compOffset));
		case RunType::Zlib:
		{
			z_stream strm;
			std::unique_ptr<char[]> rdbuf;
			uint32_t done = 0, inpos = 0;
			
			memset(&strm, 0, sizeof(strm));
			if (inflateInit(&strm) != Z_OK)
				throw std::bad_alloc();
			
			rdbuf.reset(new char[512]);
			
			while (done < count)
			{
				int32_t toRead = std::min<int32_t>(be(run->compLength)-inpos, 512);
				int32_t read = m_disk->read(rdbuf.get(), toRead, be(run->compOffset) + inpos);
				int32_t dec;
				
				strm.avail_in = read;
				strm.next_in = reinterpret_cast<Bytef*>(rdbuf.get());
				strm.avail_out = count - done;
				strm.next_out = ((Bytef*)buf) + done;
				
				dec = inflate(&strm, Z_NO_FLUSH);
				if (dec < 0)
					throw std::runtime_error("Error inflating stream");
				done += dec;
			}
			
			inflateEnd(&strm);
			return done;
		}
		case RunType::Bzip2:
		{
			bz_stream strm;
			std::unique_ptr<char[]> rdbuf;
			uint32_t done = 0, inpos = 0;
			
			memset(&strm, 0, sizeof(strm));
			
			if (BZ2_bzDecompressInit(&strm, 0, false) != BZ_OK)
				throw std::bad_alloc();
			
			rdbuf.reset(new char[512]);
			
			while (done < count)
			{
				int32_t toRead = std::min<int32_t>(be(run->compLength)-inpos, 512);
				int32_t read = m_disk->read(rdbuf.get(), toRead, be(run->compOffset) + inpos);
				int32_t dec;
				
				strm.avail_in = read;
				strm.next_in = rdbuf.get();
				strm.avail_out = count - done;
				strm.next_out = ((char*)buf) + done;
				
				dec = BZ2_bzDecompress(&strm);
				if (dec < 0)
					throw std::runtime_error("Error unbzipping stream");
				done += dec;
			}
			
			BZ2_bzDecompressEnd(&strm);
			return done;
		}
		case RunType::ADC:
		{
			std::unique_ptr<char[]> rdbuf;
			uint32_t done = 0, inpos = 0;
			
			rdbuf.reset(new char[512]);
			
			while (done < count)
			{
				int32_t toRead = std::min<int32_t>(be(run->compLength)-inpos, 512);
				int32_t read = m_disk->read(rdbuf.get(), toRead, be(run->compOffset) + inpos);
				int32_t dec;
				
				adc_decompress(read, (uint8_t*) rdbuf.get(), count - done, ((uint8_t*)buf) + done, &dec);
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
