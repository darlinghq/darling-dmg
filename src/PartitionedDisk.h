#ifndef PARTITIONEDDISK_H
#define PARTITIONEDDISK_H
#include "Reader.h"
#include <stdint.h>
#include <string>
#include <vector>

class PartitionedDisk
{
public:
	struct Partition
	{
		std::string name, type;
		uint64_t offset, size; // in byte
	};
public:	
	virtual ~PartitionedDisk() {}

	virtual const std::vector<Partition>& partitions() const = 0;
	virtual Reader* readerForPartition(int index) = 0;
};

#endif

