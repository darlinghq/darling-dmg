#include "CachedReader.h"
#include <stdexcept>

//#define NO_CACHE

CachedReader::CachedReader(std::shared_ptr<Reader> reader, CacheZone* zone, const std::string& tag)
: m_reader(reader), m_zone(zone), m_tag(tag)
{
}

int32_t CachedReader::read(void* buf, int32_t count, uint64_t offset)
{
#ifndef NO_CACHE
	int32_t done = 0; // from 0 till count
	int32_t lastFetchPos = 0; // pos from 0 till count
	
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
		
		thistime -= blockOffset;
		
		fromCache = m_zone->get(m_tag, blockNumber, ((uint8_t*) buf) + done, blockOffset, thistime);
		
		// Something was retrieved from cache
		if (fromCache > 0)
		{
			// Fetch all previous data from lastFetchPos till (offset+done) from backing store
			const int32_t toRead = done - lastFetchPos;
			const int32_t pos = offset + lastFetchPos;
			
			if (toRead > 0)
			{
				// Perform non-cached read, while saving everything read into the cache
				nonCachedRead(((char*) buf) + lastFetchPos, toRead, pos);
			}
			
			// We move lastFetchPos past the current cached read
			lastFetchPos = offset+done+thistime;
			
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
		const int32_t pos = offset + lastFetchPos;
		
		nonCachedRead(((char*) buf) + lastFetchPos, toRead, pos);
	}
	
	return done;
#else
	return m_reader->read(buf, count, offset);
#endif
}

void CachedReader::nonCachedRead(void* buf, int32_t count, uint64_t offset)
{
	int32_t cachingPos;
	int32_t rd;
	
	rd = m_reader->read(buf, count, offset);

	if (rd != count)
		throw std::runtime_error("Short read from backing reader");

	// Save whatever was read into cache
	cachingPos = offset;
	
	// Round up to BLOCK_SIZE. unaligned blocks are not possible
	if ((offset % CacheZone::BLOCK_SIZE) > 0)
		cachingPos += CacheZone::BLOCK_SIZE - (offset % CacheZone::BLOCK_SIZE);
	
	while (cachingPos < offset + rd)
	{
		int32_t thistime = CacheZone::BLOCK_SIZE;
		int32_t remaining = (offset+rd) - cachingPos;
		
		if (remaining < CacheZone::BLOCK_SIZE)
		{
			// We don't have a full block
			// Is it because we're at EOF? Cache it nontheless.
			
			if (cachingPos + CacheZone::BLOCK_SIZE > length())
				thistime = remaining;
			else
				break; // We don't cache short blocks only caused by short reads
		}
		
		m_zone->store(m_tag, cachingPos / CacheZone::BLOCK_SIZE, ((uint8_t*) buf) + cachingPos - offset, thistime);
		
		cachingPos += thistime;
	}
}

uint64_t CachedReader::length()
{
	return m_reader->length();
}
