#include "CacheZone.h"
#include <algorithm>
#include <cstring>

CacheZone::CacheZone(size_t maxBlocks)
: m_maxBlocks(maxBlocks)
{
}

void CacheZone::store(const std::string& vfile, uint64_t blockId, const uint8_t* data, size_t bytes)
{
	CacheKey key = CacheKey(blockId, vfile);
	CacheEntry entry = CacheEntry(data, data+bytes);
	
	m_cache.insert({ key, entry });
	m_cacheAge.insert({ std::chrono::system_clock::now(), key });
	
	if (m_cache.size() > m_maxBlocks)
		evictCache();
}

size_t CacheZone::get(const std::string& vfile, uint64_t blockId, uint8_t* data, size_t offset, size_t maxBytes)
{
	CacheKey key = CacheKey(blockId, vfile);
	auto it = m_cache.find(key);
	
	m_queries++;
	
	if (it == m_cache.end())
		return 0;
	
	maxBytes = std::min(it->second.size() - offset, maxBytes);
	memcpy(data, &it->second[offset], maxBytes);
	
	m_hits++;
	
	return maxBytes;
}

void CacheZone::evictCache()
{
	while (m_cache.size() > m_maxBlocks)
	{
		CacheKey key = m_cacheAge.begin()->second;
		m_cache.erase(key);
		m_cacheAge.erase(m_cacheAge.begin());
	}
}
