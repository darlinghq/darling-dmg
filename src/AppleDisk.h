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
	AppleDisk(Reader* reader);
	
	virtual const std::vector<Partition>& partitions() const override { return m_partitions; }
	virtual Reader* readerForPartition(int index) override;

	static bool isAppleDisk(Reader* reader);
private:
	AppleDisk(Reader* readerBlock0, Reader* readerPM);
	void load(Reader* readerPM);
	friend class DMGDisk;
private:
	Reader* m_reader;
	Block0 m_block0;
	std::vector<Partition> m_partitions;
};

#endif
