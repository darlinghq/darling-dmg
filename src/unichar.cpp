#include "unichar.h"
#include <iconv.h>
#include <unicode/unistr.h>

std::string UnicharToString(uint16_t length, const unichar* string)
{
	std::string result;

	UnicodeString str((char*) string, length*2, "UTF-16BE");
	str.toUTF8String(result);

	return result;
}

bool EqualNoCase(const HFSString& str1, const std::string& str2)
{
	UnicodeString ustr = UnicodeString::fromUTF8(str2);
	UnicodeString ustr2 = UnicodeString((char*)str1.string, be(str1.length)*2, "UTF-16BE");
	return ustr.caseCompare(ustr2, 0) == 0;
}

