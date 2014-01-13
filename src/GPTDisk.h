#ifndef GPTDISK_H
#define GPTDISK_H
#include "Reader.h"
#include "PartitionedDisk.h"

class GPTDisk : public PartitionedDisk
{
public:
	GPTDisk(Reader* reader);
	
	virtual const std::vector<Partition>& partitions() const override { return m_partitions; }
	virtual Reader* readerForPartition(int index) override { return nullptr; }
	
	static bool isGPTDisk(Reader* reader);
private:
	Reader* m_reader;
	std::vector<Partition> m_partitions;
};

#endif
