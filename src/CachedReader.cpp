#include "CachedReader.h"
#include <stdexcept>
#include <algorithm>
#include <iostream>
#include <limits>
#include "exceptions.h"

//#define NO_CACHE

CachedReader::CachedReader(std::shared_ptr<Reader> reader, std::shared_ptr<CacheZone> zone, const std::string& tag)
: m_reader(reader), m_zone(zone), m_tag(tag)
{
}

int32_t CachedReader::read(void* buf, int32_t count, uint64_t offset)
{
#ifndef NO_CACHE
	int32_t done = 0; // from 0 till count
	int32_t lastFetchPos = 0; // pos from 0 till count
	
#ifdef DEBUG
	std::cout << "CachedReader::read(): offset=" << offset << ", count=" << count << std::endl;
#endif

	if (count+offset > length())
		count = length() - offset;
	
	while (done < count)
	{
		int32_t thistime = std::min<int32_t>(count - done, CacheZone::BLOCK_SIZE);
		uint64_t blockNumber = (offset+done) / CacheZone::BLOCK_SIZE;
		uint64_t blockOffset = 0;
		size_t fromCache;
		
		if (done == 0) // this may also happen when cache doesn't contain a full block, but not on a R/O filesystem
			blockOffset = offset % CacheZone::BLOCK_SIZE;
		
		thistime = std::min<int32_t>(thistime, CacheZone::BLOCK_SIZE - blockOffset);
		if (thistime == 0)
			throw std::logic_error("Internal error: thistime == 0");
		
		fromCache = m_zone->get(m_tag, blockNumber, ((uint8_t*) buf) + done, blockOffset, thistime);
		
		// Something was retrieved from cache
		if (fromCache > 0)
		{
			// Fetch all previous data from lastFetchPos till (offset+done) from backing store
			const int32_t toRead = done - lastFetchPos;
			const uint64_t pos = offset + lastFetchPos;
			
			if (toRead > 0)
			{
				// Perform non-cached read, while saving everything read into the cache
				nonCachedRead(((char*) buf) + lastFetchPos, toRead, pos);
			}
			
			// We move lastFetchPos past the current cached read
			lastFetchPos = done+thistime;
			
			done += fromCache;
		}
		else
		{
			// We pretend that the data was read, we'll read it later via nonCachedRead()
			//lastFetchPos += thistime;
			done += thistime;
		}
	}
	
	if (lastFetchPos < count)
	{
		// Uncached blocks at the end of the requested range
		const int32_t toRead = done - lastFetchPos;
		const uint64_t pos = offset + lastFetchPos;
		
		nonCachedRead(((char*) buf) + lastFetchPos, toRead, pos);
	}
	
	return done;
#else
	return m_reader->read(buf, count, offset);
#endif
}

void CachedReader::nonCachedRead(void* buf, int32_t count, uint64_t offset)
{
	uint64_t blockStart, blockEnd;
	std::unique_ptr<uint8_t[]> optimalBlockBuffer;
	uint32_t optimalBlockBufferSize = 0;
	uint64_t readPos = offset;

#ifdef DEBUG
	std::cout << "CachedReader::nonCachedRead(): offset=" << offset << ", count=" << count << std::endl;
#endif

	while (readPos < offset+count)
	{
		int32_t thistime, rd;

		m_reader->adviseOptimalBlock(readPos, blockStart, blockEnd);

		// Does the returned block contain what we asked for?
		if (blockStart > readPos || blockEnd <= readPos)
			throw std::logic_error("Illegal range returned by adviseOptimalBlock()");
		if (blockEnd - blockStart > std::numeric_limits<int32_t>::max())
			throw std::logic_error("Range returned by adviseOptimalBlock() is too large");

		thistime = blockEnd-blockStart;
		if (thistime > optimalBlockBufferSize)
		{
			optimalBlockBufferSize = thistime;
			optimalBlockBuffer.reset(new uint8_t[optimalBlockBufferSize]);
		}

#ifdef DEBUG
		std::cout << "Reading from backing reader: offset=" << blockStart << ", count=" << thistime << std::endl;
#endif
		rd = m_reader->read(optimalBlockBuffer.get(), thistime, blockStart);

		if (rd < thistime)
			throw io_error("Short read from backing reader");

		// Align to the next BLOCK_SIZE aligned block
		uint64_t cachePos = (blockStart + (CacheZone::BLOCK_SIZE-1)) & ~uint64_t(CacheZone::BLOCK_SIZE-1);

		// And start storing everything we've just read into cache
		while (cachePos < blockEnd)
		{
			m_zone->store(m_tag, cachePos / CacheZone::BLOCK_SIZE, &optimalBlockBuffer[cachePos - blockStart],
					std::min<size_t>(blockEnd-cachePos, CacheZone::BLOCK_SIZE));
			cachePos += CacheZone::BLOCK_SIZE;
		}

		// Copy into output buffer
		uint32_t optimalOffset = 0; // offset into optimalBlockBuffer to start copying from
		uint32_t outputOffset; // offset into 'buf' to copy to
		uint32_t toCopy;

		if (readPos > blockStart)
			optimalOffset = readPos - blockStart;
		outputOffset = readPos - offset;
		toCopy = std::min<uint32_t>(offset+count - readPos, thistime - optimalOffset);

#ifdef DEBUG
		std::cout << "Copying " << toCopy << " bytes into output buffer at offset " << outputOffset << " from internal offset " << optimalOffset << std::endl;
#endif
		// if (toCopy+optimalOffset > thistime)
		// 	throw std::logic_error("Internal error");
		std::copy_n(&optimalBlockBuffer[optimalOffset], toCopy, reinterpret_cast<uint8_t*>(buf) + outputOffset);

		readPos += toCopy;
	}
}

uint64_t CachedReader::length()
{
	return m_reader->length();
}
