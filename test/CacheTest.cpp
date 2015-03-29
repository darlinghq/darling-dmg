#include "../src/CacheZone.h"
#include "../src/CachedReader.h"
#include "../src/MemoryReader.h"
#include <memory>
#include <random>
#include <array>
#include <iostream>
#include "CacheTest.h"

#define BOOST_TEST_MODULE CacheTest
#include <boost/test/unit_test.hpp>

static void generateRandomData(std::vector<uint8_t>& testData);

BOOST_AUTO_TEST_CASE(CacheTest)
{
	std::shared_ptr<MyMemoryReader> memoryReader;
	std::unique_ptr<CachedReader> cachedReader;
	std::vector<uint8_t> testData;
	CacheZone zone(50);

	// Generate 20000 bytes of random data
	generateRandomData(testData);

	memoryReader.reset(new MyMemoryReader(&testData[0], testData.size()));

	// Specify a few optimal boundaries
	memoryReader->setOptimalBoundaries({ 4096, 3*4096 });

	cachedReader.reset(new CachedReader(memoryReader, &zone, "MyMemoryReader"));

	// Read all the data via cachedReader, 500 bytes at a time
	for (int i = 0; i < 20000; i += 500)
	{
		std::array<uint8_t, 500> buf;

		cachedReader->read(buf.begin(), buf.size(), i);

		// Verify data integrity
		//for (int j = 0; j < 500; j++)
		//{
		//	if (buf[j] != testData[i+j])
		//		std::cout << "Mismatch at byte " << j << std::endl;
		//}
		BOOST_CHECK(std::equal(buf.begin(), buf.end(), &testData[i]));
	}

	// The cache should contain 5 blocks, up to 4096 bytes each
	BOOST_CHECK_EQUAL(zone.size(), 5);
}

static void generateRandomData(std::vector<uint8_t>& randomData)
{
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<> dis(0, 255);

	randomData.reserve(20000);

	for (int i = 0; i < 20000; i++)
		randomData.push_back(dis(gen));
}

void MyMemoryReader::setOptimalBoundaries(std::initializer_list<uint64_t> bd)
{
	m_optimalBoundaries.assign(bd);
}

void MyMemoryReader::adviseOptimalBlock(uint64_t offset, uint64_t& blockStart, uint64_t& blockEnd)
{
	if (m_optimalBoundaries.empty())
		return MemoryReader::adviseOptimalBlock(offset, blockStart, blockEnd);

	auto it = std::upper_bound(m_optimalBoundaries.begin(), m_optimalBoundaries.end(), offset);

	if (it == m_optimalBoundaries.end())
	{
		blockStart = m_optimalBoundaries.back();
		blockEnd = length();
	}
	else
	{
		blockEnd = *it;

		if (it != m_optimalBoundaries.begin())
		{
			it--;
			blockStart = *it;
		}
		else
			blockStart = 0;
	}
}

int32_t MyMemoryReader::read(void* buf, int32_t count, uint64_t offset)
{
	// Check that this read starts and ends on an optimal boundary
	bool startsOnOptimal, endsOnOptimal;

	std::cout << "Read: offset=" << offset << ", count=" << count << std::endl;

	startsOnOptimal = offset == 0;
	startsOnOptimal |= std::find(m_optimalBoundaries.begin(), m_optimalBoundaries.end(), offset) != m_optimalBoundaries.end();

	BOOST_CHECK(startsOnOptimal);

	endsOnOptimal = (offset+count) == length();
	endsOnOptimal |= std::find(m_optimalBoundaries.begin(), m_optimalBoundaries.end(), offset+count) != m_optimalBoundaries.end();

	BOOST_CHECK(endsOnOptimal);

	return MemoryReader::read(buf, count, offset);
}
