#ifndef MEMORYREADER_H
#define MEMORYREADER_H
#include "Reader.h"
#include <vector>
#include <stddef.h>

class MemoryReader : public Reader
{
public:
	MemoryReader(const uint8_t* start, size_t length);
	virtual int32_t read(void* buf, int32_t count, uint64_t offset) override;
	virtual uint64_t length() override;
private:
	std::vector<uint8_t> m_data;
};

#endif
