#include "HFSZlibReader.h"
#include <cstring>
#include <stdexcept>
#include <algorithm>
#include "exceptions.h"
#include "be.h"

// HFS+ compresses data in 64KB blocks
static const unsigned int RUN_LENGTH = 64*1024;

HFSZlibReader::HFSZlibReader(std::shared_ptr<Reader> parent, uint64_t uncompressedSize, bool singleRun)
: m_reader(parent), m_uncompressedSize(uncompressedSize)
{
	// read the compression table (little endian)
	// uint32_t numEntries
	// uint32_t offsets[(num_entries+1)*2] (offset, length)
	//
	// Each offset points to the start of a 64 KB block
	
	if (!singleRun)
	{
		uint32_t numEntries;
		std::unique_ptr<uint32_t[]> entries;
		
		if (m_reader->read(&numEntries, sizeof(numEntries), 0) != sizeof(numEntries))
			throw io_error("Short read of compression map");
		
		numEntries = le(numEntries);
		entries.reset(new uint32_t[(numEntries+1) * 2]);
		
		if (m_reader->read(entries.get(), sizeof(uint32_t) * 2 * (numEntries+1), sizeof(numEntries)) != sizeof(uint32_t) * 2 * (numEntries+1))
			throw io_error("Short read of compression map entries");
		
		for (size_t i = 0; i < numEntries+1; i++)
			m_offsets.push_back(std::make_pair(le(entries[i*2]), le(entries[i*2+1])));
	}
	else
	{
		// This branch is only used for zlib data stored within extended attributes.
		// In this case, the reader here is a MemoryReader with a small amount of data,
		// thus it is OK to cast the length to uint32_t.
		m_offsets.push_back(std::pair<uint32_t,uint32_t>(0, m_reader->length()));
	}
	
	zlibInit();
}

HFSZlibReader::~HFSZlibReader()
{
	zlibExit();
}

void HFSZlibReader::adviseOptimalBlock(uint64_t offset, uint64_t& blockStart, uint64_t& blockEnd)
{
	blockStart = offset & ~uint64_t(RUN_LENGTH-1);
	blockEnd = std::min(blockStart + RUN_LENGTH, length());
}

void HFSZlibReader::zlibInit()
{
	memset(&m_strm, 0, sizeof(m_strm));
	if (inflateInit(&m_strm) != Z_OK)
		throw std::bad_alloc();
}

void HFSZlibReader::zlibExit()
{
	inflateEnd(&m_strm);
	m_inputPos = 0;
	m_lastEnd = 0;
}

int32_t HFSZlibReader::readRun(int runIndex, void* buf, int32_t count, uint64_t offset)
{
	// We're in a different run or we're skipping back in the run, so we can't resume anything
	if (runIndex != m_lastRun || offset < m_lastEnd)
	{
		zlibExit();
		zlibInit();
		m_lastEnd = 0;
		m_inputPos = 0;
		m_lastUncompressed = false;
	}
	
	// We're skipping forward in the current run. Waste decompress the data in between.
	if (m_lastEnd < offset)
	{
		char waste[512];
		
		while (m_lastEnd < offset)
		{
			int thistime = std::min<uint64_t>(sizeof(waste), offset - m_lastEnd);
			readRun(runIndex, waste, thistime, m_lastEnd);
		}
	}
	
	// Decompress
	char inputBuffer[512];
	int32_t done = 0;
	int32_t readCompressedSoFar = 0;
	
	while (done < count)
	{
		int32_t read = 0;
		int status, thistime;
		
		thistime = count - done;
  
		int thisTimeCompressed = std::min<uint64_t>(m_offsets[runIndex].second-readCompressedSoFar, sizeof(inputBuffer));
		
		if (!m_lastUncompressed)
			read = m_reader->read(inputBuffer, thisTimeCompressed, m_inputPos + m_offsets[runIndex].first);
		readCompressedSoFar += read;
		
		// Special handling for uncompressed blocks
		if (m_lastUncompressed || (done == 0 && read > 0 && m_inputPos==0 && (inputBuffer[0] & 0xf) == 0xf))
		{
			if (!m_lastUncompressed)
				m_inputPos++;
			
			count = std::min<int32_t>(count, m_offsets[runIndex].second - offset - 1);
			read = m_reader->read(buf, count, m_inputPos + m_offsets[runIndex].first);
			m_inputPos += read;
			
			m_lastEnd += read;
			m_lastUncompressed = true;
			return read;
		}
		
		m_strm.next_in = (Bytef*) inputBuffer;
		m_strm.next_out = ((Bytef*) buf) + done;
		m_strm.avail_in = read;
		m_strm.avail_out = thistime;
		
		status = inflate(&m_strm, Z_SYNC_FLUSH);
		
		if (status < 0)
			throw io_error("Inflate error");
		
		done += thistime - m_strm.avail_out;
		m_inputPos += read - m_strm.avail_in;
		
		if (status == Z_STREAM_END)
			break;
	}
	
	m_lastEnd += done;
	m_lastRun = runIndex;
	
	return done;
}


int32_t HFSZlibReader::read(void* buf, int32_t count, uint64_t offset)
{
	int32_t done = 0;
	
	if (offset+count > m_uncompressedSize)
		count = m_uncompressedSize - offset;
	
	while (done < count)
	{
		uint64_t runOffset = 0;
		uint32_t thisTime, read;
		const int runIndex = (offset+done) / RUN_LENGTH;
		
		// runOffset only relevant in first run
		if (done == 0)
			runOffset = offset % RUN_LENGTH;
		
		thisTime = std::min<uint32_t>(RUN_LENGTH, count-done);
		read = readRun(runIndex, static_cast<char*>(buf) + done, thisTime, runOffset);
		
		if (read != thisTime)
			throw io_error("Short read from readRun");
		
		done += read;
	}
	
	return done;
}

uint64_t HFSZlibReader::length()
{
	return m_uncompressedSize;
}
