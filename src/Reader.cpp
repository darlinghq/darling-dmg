#include "Reader.h"
#include "CacheZone.h"

void Reader::adviseOptimalBlock(uint64_t offset, uint64_t& blockStart, uint64_t& blockEnd)
{
	// Default implementation returns a block aligned to a 4096-byte boundary
	blockStart = offset & ~(CacheZone::BLOCK_SIZE - 1);
	blockEnd = blockStart + CacheZone::BLOCK_SIZE;

	const uint64_t len = length();
	if (blockEnd > len)
		blockEnd = len;
}
