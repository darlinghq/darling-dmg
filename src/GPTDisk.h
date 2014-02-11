#ifndef GPTDISK_H
#define GPTDISK_H
#include "Reader.h"
#include "gpt.h"
#include "PartitionedDisk.h"
#include <memory>

class GPTDisk : public PartitionedDisk
{
public:
	GPTDisk(std::shared_ptr<Reader> readerWholeDisk);
	GPTDisk(std::shared_ptr<Reader> protectiveMBR, std::shared_ptr<Reader> partitionTable);
	
	virtual const std::vector<Partition>& partitions() const override { return m_partitions; }
	virtual std::shared_ptr<Reader> readerForPartition(int index) override;
	
	static bool isGPTDisk(std::shared_ptr<Reader> reader);
private:
	void loadPartitions(std::shared_ptr<Reader> table);
	static std::string makeGUID(const GUID& guid);
private:
	std::shared_ptr<Reader> m_reader;
	std::vector<Partition> m_partitions;
};

#endif
