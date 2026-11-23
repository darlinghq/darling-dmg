#ifndef UNICHAR_H
#define UNICHAR_H
#include <string>
#include "hfsplus.h"
#include "be.h"

std::string UnicharToString(uint16_t length, const unichar* string);
bool EqualNoCase(const HFSString& str1, const std::string& str2);
bool EqualCase(const HFSString& str1, const std::string& str2);
uint16_t StringToUnichar(const std::string& in, unichar* out, size_t maxLength /* in unichars */);

// An HFS+ name holds at most 255 units; a larger length (damaged or crafted volume) is cut to that.
inline std::string UnicharToString(const HFSString& str)
{
	uint16_t length = be(str.length);
	return UnicharToString(length > 255 ? 255 : length, str.string);
}

#endif
