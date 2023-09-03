#ifndef CACHEDREADER_H
#define CACHEDREADER_H
#include "Reader.h"
#include "CacheZone.h"
#include <memory>

class CachedReader : public Reader
{
public:
	CachedReader(std::shared_ptr<Reader> reader, std::shared_ptr<CacheZone> zone, const std::string& tag);
	
	virtual int32_t read(void* buf, int32_t count, uint64_t offset) override;
	virtual uint64_t length() override;
private:
	void nonCachedRead(void* buf, int32_t count, uint64_t offset);
private:
	std::shared_ptr<Reader> m_reader;
	std::shared_ptr<CacheZone> m_zone;
	const std::string m_tag;
};

#endif
