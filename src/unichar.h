#ifndef UNICHAR_H
#define UNICHAR_H
#include <string>
#include "hfsplus.h"
#include "be.h"

std::string UnicharToString(uint16_t length, const unichar* string);
bool EqualNoCase(const HFSString& str1, const std::string& str2);

inline std::string UnicharToString(const HFSString& str) { return UnicharToString(be(str.length), str.string); }

#endif
