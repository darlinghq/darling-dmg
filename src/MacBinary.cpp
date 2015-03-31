#include "MacBinary.h"
#include "SubReader.h"
#include "be.h"
#include "exceptions.h"
#include <stdexcept>

MacBinary::MacBinary(std::shared_ptr<Reader> reader)
	: m_reader(reader)
{
	if (m_reader->read(&m_header, sizeof(m_header), 0) != sizeof(m_header))
		throw io_error("Error reading MacBinary header");
}

Reader* MacBinary::getDataFork()
{
	uint32_t extraLen = 0;

	if (be(m_header.signature) == 'mBIN')
		extraLen = be(m_header.sec_header_len);

	return new SubReader(m_reader, sizeof(m_header) + extraLen, be(m_header.data_len));
}

Reader* MacBinary::getResourceFork()
{
	uint32_t extraLen = 0;

	if (be(m_header.signature) == 'mBIN')
		extraLen = be(m_header.sec_header_len);

	extraLen += be(m_header.data_len);
	extraLen = (extraLen+127) / 128 * 128;

	return new SubReader(m_reader, sizeof(m_header) + extraLen, be(m_header.resource_len));
}

