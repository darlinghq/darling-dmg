#ifndef DMGDECOMPRESSOR_H
#define DMGDECOMPRESSOR_H
#include <stdint.h>
#include "dmg.h"

class DMGDecompressor
{
protected:
	DMGDecompressor() {}
public:
	virtual ~DMGDecompressor() {}
	virtual int32_t decompress(const void* input, int32_t inputBytes, void* output, int32_t outputBytes) = 0;
	
	static DMGDecompressor* create(RunType runType);
};

#endif
