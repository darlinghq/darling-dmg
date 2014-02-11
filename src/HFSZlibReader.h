#ifndef ZLIBREADER_H
#define ZLIBREADER_H
#include "Reader.h"
#include <stdint.h>
#include <zlib.h>
#include <memory>
#include <vector>

class HFSZlibReader : public Reader
{
public:
	HFSZlibReader(std::shared_ptr<Reader> parent, uint64_t uncompressedSize, bool singleRun = false);
	virtual ~HFSZlibReader();
	
	virtual int32_t read(void* buf, int32_t count, uint64_t offset) override;
	virtual uint64_t length() override;
private:
	int32_t readRun(int runIndex, void* buf, int32_t count, uint64_t offset);
	void zlibInit();
	void zlibExit();
private:
	std::shared_ptr<Reader> m_reader;
	bool m_ownParentReader;
	uint64_t m_uncompressedSize;
	z_stream m_strm;
	int m_lastRun = -1;
	uint64_t m_lastEnd = 0, m_inputPos = 0;
	bool m_lastUncompressed = false;
	std::vector<std::pair<uint32_t,uint32_t>> m_offsets;
};

#endif
