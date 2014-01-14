#ifndef GPTDISK_H
#define GPTDISK_H
#include "Reader.h"
#include "gpt.h"
#include "PartitionedDisk.h"

class GPTDisk : public PartitionedDisk
{
public:
	GPTDisk(Reader* readerWholeDisk);
	GPTDisk(Reader* protectiveMBR, Reader* partitionTable);
	
	virtual const std::vector<Partition>& partitions() const override { return m_partitions; }
	virtual Reader* readerForPartition(int index) override;
	
	static bool isGPTDisk(Reader* reader);
private:
	void loadPartitions(Reader* table);
	static std::string makeGUID(const GUID& guid);
private:
	Reader* m_reader;
	std::vector<Partition> m_partitions;
};

#endif
