#include "MemoryReader.h"
#include <cstring>

MemoryReader::MemoryReader(const uint8_t* start, size_t length)
{
	m_data = std::vector<uint8_t>(start, start+length);
}

int32_t MemoryReader::read(void* buf, int32_t count, uint64_t offset)
{
	if (offset > m_data.size())
		return 0;
	if (count+offset > m_data.size())
		count = m_data.size() - offset;
	
	memcpy(buf, &m_data[offset], count);
	return count;
}

uint64_t MemoryReader::length()
{
	return m_data.size();
}
