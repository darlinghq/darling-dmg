#ifndef DMGDECOMPRESSOR_H
#define DMGDECOMPRESSOR_H
#include <stdint.h>
#include "dmg.h"
#include "Reader.h"

class DMGDecompressor
{
protected:
	DMGDecompressor(Reader* reader);
	int readSome(char** ptr);
	void processed(int bytes);
public:
	virtual ~DMGDecompressor() {}
	virtual int32_t decompress(void* output, int32_t outputBytes) = 0;
	
	static DMGDecompressor* create(RunType runType, Reader* reader);
private:
	Reader* m_reader;
	uint16_t m_pos;
	char m_buf[512];
};

#endif
