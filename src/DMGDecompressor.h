#ifndef DMGDECOMPRESSOR_H
#define DMGDECOMPRESSOR_H
#include <stdint.h>
#include "dmg.h"
#include "Reader.h"
#include <memory>

class DMGDecompressor
{
protected:
	DMGDecompressor(std::shared_ptr<Reader> reader);
	int32_t readSome(char** ptr);
	void processed(int bytes);
	uint64_t readerLength() const { return m_reader->length(); }
public:
	virtual ~DMGDecompressor() {}
	virtual int32_t decompress(void* output, int32_t count, int64_t offset) = 0;
	
	static DMGDecompressor* create(RunType runType, std::shared_ptr<Reader> reader);
private:
	std::shared_ptr<Reader> m_reader;
	uint64_t m_pos;
	char m_buf[8*1024];
};

#endif
