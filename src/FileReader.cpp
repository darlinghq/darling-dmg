#include "FileReader.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <cstring>
#include <stdexcept>
#include <sstream>

FileReader::FileReader(const std::string& path)
: m_fd(-1)
{
	m_fd = ::open(path.c_str(), O_RDONLY);

	if (m_fd == -1)
	{
		std::stringstream ss;
		ss << "Cannot open " << path << ": " << strerror(errno);
		throw std::runtime_error(ss.str());
	}
}

FileReader::~FileReader()
{
	if (m_fd != -1)
		::close(m_fd);
}

int32_t FileReader::read(void* buf, int32_t count, uint64_t offset)
{
	if (m_fd == -1)
		return -1;
	
	static_assert(sizeof(off_t) == 8, "off_t is too small");
	
	return ::pread(m_fd, buf, count, offset);
}

uint64_t FileReader::length()
{
	return ::lseek(m_fd, 0, SEEK_END);
}
