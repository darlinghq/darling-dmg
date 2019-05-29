#include "MemoryReader.h"
#include <cstring>

MemoryReader::MemoryReader(const uint8_t* start, size_t length)
{
	m_data = std::vector<uint8_t>(start, start+length);
}

int32_t MemoryReader::read(void* buf, int32_t count, uint64_t offset)
{
	if ( count < 0 )
		return -1;
	if (offset > m_data.size())
		return 0;
	if (count > m_data.size() - offset) // here, offset is >= m_data.size()  =>  m_data.size()-offset >= 0.   (Do not test like ' if (offset+count > be(m_fork.logicalSize)) ', offset+count could overflow)
		count = int32_t(m_data.size() - offset); // here, m_data.size()-offset >= 0 AND < count  =>  m_data.size()-offset < INT32_MAX, cast is safe
	
	memcpy(buf, &m_data[offset], count);
	return count;
}

uint64_t MemoryReader::length()
{
	return m_data.size();
}
