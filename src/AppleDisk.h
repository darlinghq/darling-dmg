#ifndef APPLEDISK_H
#define APPLEDISK_H
#include "apm.h"
#include "Reader.h"
#include <string>
#include <vector>

class AppleDisk
{
public:
	AppleDisk(Reader* reader);
	
	struct Partition
	{
		std::string name, type;
		uint64_t offset, size; // in byte
	};
	
	const std::vector<Partition>& partitions() const { return m_partitions; }
	Reader* readerForPartition(unsigned int index);
private:
	Reader* m_reader;
	Block0 m_block0;
	std::vector<Partition> m_partitions;
};

#endif
