#ifndef DMGPARTITION_H
#define DMGPARTITION_H
#include "Reader.h"
#include "dmg.h"

class DMGPartition : public Reader
{
public:
	DMGPartition(Reader* disk, BLKXTable* table);
    ~DMGPartition();
	
	virtual int32_t read(void* buf, int32_t count, uint64_t offset) override;
	virtual uint64_t length() override;
private:
	Reader* m_disk;
	BLKXTable* m_table;
};

#endif
