#ifndef CACHEZONE_H
#define CACHEZONE_H
#include <stddef.h>
#include <stdint.h>
#include <chrono>
#include <string>
#include <vector>
#include <list>
#include <unordered_map>

namespace std {
template <typename A, typename B> struct hash<std::pair<A, B>>
{
	size_t operator()(const std::pair<A, B>& t) const
	{
		return std::hash<A>()(t.first) ^ std::hash<B>()(t.second);
	}
};
}

class CacheZone
{
public:
	CacheZone(size_t maxBlocks);
	
	enum { BLOCK_SIZE = 4096 };
	
	void store(const std::string& vfile, uint64_t blockId, const uint8_t* data, size_t bytes);
	size_t get(const std::string& vfile, uint64_t blockId, uint8_t* data, size_t offset, size_t maxBytes);
	
	void setMaxBlocks(size_t max);
	inline size_t maxBlocks() const { return m_maxBlocks; }
	
	inline float hitRate() const { return float(m_hits) / float(m_queries); }
	inline size_t size() const { return m_cache.size(); }
private:
	void evictCache();
private:
	typedef std::pair<uint64_t, std::string> CacheKey;
	
	struct CacheEntry
	{
		std::list<CacheKey>::iterator itAge;
		std::vector<uint8_t> data;
	};
	
	typedef std::unordered_map<CacheKey, CacheEntry> Cache;
	
	Cache m_cache;
	std::list<CacheKey> m_cacheAge;
	size_t m_maxBlocks;
	uint64_t m_queries = 0, m_hits = 0;
};



#endif
