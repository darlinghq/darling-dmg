#include "HFSZlibReader.h"
#include <cstring>
#include <stdexcept>
#include "be.h"

static const unsigned int RUN_LENGTH = 64*1024;

HFSZlibReader::HFSZlibReader(std::shared_ptr<Reader> parent, uint64_t uncompressedSize)
: m_reader(parent), m_uncompressedSize(uncompressedSize)
{
	// read the compression table (little endian)
	// uint32_t numEntries
	// uint64_t offsets[num_entries+1]
	//
	// Each offset points to the start of a 64 KB block (when uncompressed)
	
	uint32_t numEntries;
	
	if (m_reader->read(&numEntries, sizeof(numEntries), 0) != sizeof(numEntries))
		throw std::runtime_error("Short read of compression map");
	
	numEntries = le(numEntries);
	m_offsets.resize(numEntries+1);
	
	if (m_reader->read(&m_offsets[0], sizeof(uint64_t) * (numEntries+1), sizeof(numEntries)) != sizeof(uint64_t) * (numEntries+1))
		throw std::runtime_error("Short read of compression map entries");
	
	for (size_t i = 0; i < m_offsets.size(); i++)
		m_offsets[i] = le(m_offsets[i]);
	
	zlibInit();
}

HFSZlibReader::~HFSZlibReader()
{
	zlibExit();
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
	}
	
	// We're skipping forward in the current run. Waste decompress the data in between.
	if (m_lastEnd < offset)
	{
		char waste[512];
		
		while (m_lastEnd < offset)
		{
			int thistime = std::min(sizeof(waste), offset - m_lastEnd);
			readRun(runIndex, waste, thistime, m_lastEnd);
		}
	}
	
	// Decompress
	char inputBuffer[512];
	int32_t done = 0;
	
	while (done < count)
	{
		int32_t read;
		int status, thistime;
		
		thistime = count - done;
		read = m_reader->read(inputBuffer, sizeof(inputBuffer), m_inputPos + m_offsets[runIndex]);
		
		// Special handling for uncompressed blocks
		if (done == 0 && read > 0 && (inputBuffer[0] & 0xf) == 0xf)
		{
			count = std::min<int32_t>(count, m_offsets[runIndex+1] - m_offsets[runIndex]);
			read = m_reader->read(buf, count, m_inputPos);
			m_inputPos += read;
			m_lastEnd += read;
			return read;
		}
		
		m_strm.next_in = (Bytef*) inputBuffer;
		m_strm.next_out = ((Bytef*) buf) + done;
		m_strm.avail_in = read;
		m_strm.avail_out = thistime;
		
		status = inflate(&m_strm, Z_SYNC_FLUSH);
		
		if (status < 0)
			throw std::runtime_error("Inflate error");
		
		done += thistime - m_strm.avail_out;
		m_inputPos += read - m_strm.avail_in;
		
		if (status == Z_STREAM_END)
			break;
	}
	
	m_lastEnd += done;
	
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
			throw std::runtime_error("Short read from readRun");
		
		done += read;
	}
	
	return done;
}

uint64_t HFSZlibReader::length()
{
	return m_uncompressedSize;
}
