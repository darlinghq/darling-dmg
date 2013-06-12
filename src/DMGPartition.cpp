#include "DMGPartition.h"
#include "be.h"

DMGPartition::DMGPartition(Reader* disk, BLKXTable* table)
: m_disk(disk), m_table(table)
{
}

DMGPartition::~DMGPartition()
{
	delete m_table;
}

int32_t DMGPartition::read(void* buf, int32_t count, uint64_t offset)
{
}

uint64_t DMGPartition::length()
{
	return be(m_table->sectorCount) * 512;
}
