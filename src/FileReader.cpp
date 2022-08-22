#include "FileReader.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <cstring>
#include <stdexcept>
#include <iostream>
#include "exceptions.h"

FileReader::FileReader(const std::string& path)
: m_fd(-1)
{
	m_fd = ::open(path.c_str(), O_RDONLY);

	if (m_fd == -1)
	{
#ifdef DEBUG
		std::cerr << "Cannot open " << path << ": " << strerror(errno) << std::endl;
#endif
		throw file_not_found_error(path);
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
	
	return (int32_t)::pread(m_fd, buf, count, offset); // safe cast, pread returned value is < count
}

uint64_t FileReader::length()
{
	return ::lseek(m_fd, 0, SEEK_END);
}
