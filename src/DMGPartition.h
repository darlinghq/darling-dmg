#ifndef DMGPARTITION_H
#define DMGPARTITION_H
#include "Reader.h"
#include "dmg.h"
#include <memory>
#include <map>

class DMGPartition : public Reader
{
public:
	DMGPartition(std::shared_ptr<Reader> disk, BLKXTable* table);
    ~DMGPartition();
	
	virtual int32_t read(void* buf, int32_t count, uint64_t offset) override;
	virtual uint64_t length() override;
	virtual void adviseOptimalBlock(uint64_t offset, uint64_t& blockStart, uint64_t& blockEnd) override;
private:
	int32_t readRun(void* buf, int32_t runIndex, uint64_t offsetInSector, int32_t count);
private:
	std::shared_ptr<Reader> m_disk;
	BLKXTable* m_table;
	std::map<uint64_t, uint32_t> m_sectors;
};

#endif
