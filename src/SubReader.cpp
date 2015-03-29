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

void SubReader::adviseOptimalBlock(uint64_t offset, uint64_t& blockStart, uint64_t& blockEnd)
{
	m_parent->adviseOptimalBlock(m_offset+offset, blockStart, blockEnd);

	if (blockStart < m_offset)
		blockStart = m_offset;
	blockStart -= m_offset;

	blockEnd -= m_offset;
	if (blockEnd > m_size)
		blockEnd = m_size;
}
