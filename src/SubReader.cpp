#include "SubReader.h"

SubReader::SubReader(std::shared_ptr<Reader> parent, uint64_t offset, uint64_t size)
: m_parent(parent), m_offset(offset), m_size(size)
{
}

int32_t SubReader::read(void* buf, int32_t count, uint64_t offset)
{
	if (offset > m_size)
		return 0;
	if (offset+count > m_size)
		count = m_size - offset;
	
	return m_parent->read(buf, count, offset + m_offset);
}

uint64_t SubReader::length()
{
	return m_size;
}
