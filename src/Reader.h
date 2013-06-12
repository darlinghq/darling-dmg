#ifndef READER_H
#define READER_H
#include <stdint.h>

class Reader
{
public:
	virtual ~Reader() {}
	virtual int32_t read(void* buf, int32_t count, uint64_t offset) = 0;
	virtual uint64_t length() = 0;
};

#endif
