#ifndef CACHETEST_H
#define CACHETEST_H
#include "../src/MemoryReader.h"
#include <list>

class MyMemoryReader : public MemoryReader
{
public:
	using MemoryReader::MemoryReader;

	void setOptimalBoundaries(std::initializer_list<uint64_t> bd);
	virtual void adviseOptimalBlock(uint64_t offset, uint64_t& blockStart, uint64_t& blockEnd) override;
	virtual int32_t read(void* buf, int32_t count, uint64_t offset) override;

private:
	std::list<uint64_t> m_optimalBoundaries;
};

#endif // CACHETEST_H

