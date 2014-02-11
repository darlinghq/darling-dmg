#include "CacheZone.h"
#include <algorithm>
#include <cstring>

CacheZone::CacheZone(size_t maxBlocks)
: m_maxBlocks(maxBlocks)
{
}

void CacheZone::setMaxBlocks(size_t max)
{
	m_maxBlocks = max;
	evictCache();
}

void CacheZone::store(const std::string& vfile, uint64_t blockId, const uint8_t* data, size_t bytes)
{
	CacheKey key = CacheKey(blockId, vfile);
	CacheEntry entry;
	std::unordered_map<CacheKey, CacheEntry>::iterator it;
	
	entry.data = std::vector<uint8_t>(data, data+bytes);
	
	it = m_cache.insert(m_cache.begin(), { key, entry });
	m_cacheAge.push_back(key);
	it->second.itAge = --m_cacheAge.end();
	
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
	
	maxBytes = std::min(it->second.data.size() - offset, maxBytes);
	memcpy(data, &it->second.data[offset], maxBytes);
	
	m_cacheAge.erase(it->second.itAge);
	m_cacheAge.push_back(key);
	it->second.itAge = --m_cacheAge.end();
	m_hits++;
	
	return maxBytes;
}

void CacheZone::evictCache()
{
	while (m_cache.size() > m_maxBlocks)
	{
		CacheKey& key = m_cacheAge.front();
		m_cache.erase(key);
		m_cacheAge.erase(m_cacheAge.begin());
	}
}
