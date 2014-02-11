#ifndef APPLEDISK_H
#define APPLEDISK_H
#include "apm.h"
#include "Reader.h"
#include "PartitionedDisk.h"
#include <string>
#include <vector>

class AppleDisk : public PartitionedDisk
{
public:
	AppleDisk(std::shared_ptr<Reader> reader);
	
	virtual const std::vector<Partition>& partitions() const override { return m_partitions; }
	virtual std::shared_ptr<Reader> readerForPartition(int index) override;

	static bool isAppleDisk(std::shared_ptr<Reader> reader);
private:
	AppleDisk(std::shared_ptr<Reader> readerBlock0, std::shared_ptr<Reader> readerPM);
	void load(std::shared_ptr<Reader> readerPM);
	friend class DMGDisk;
private:
	std::shared_ptr<Reader> m_reader;
	Block0 m_block0;
	std::vector<Partition> m_partitions;
};

#endif
