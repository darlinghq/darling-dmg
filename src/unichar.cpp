#include "unichar.h"
#include <iconv.h>
#include <unicode/unistr.h>
#include <unicode/ucnv.h>
#include <unicode/errorcode.h>
#include <cassert>
#include <iostream>
using icu::UnicodeString;
UConverter *g_utf16be;

static void InitConverter() __attribute__((constructor));
static void ExitConverter() __attribute__((destructor));

std::string UnicharToString(uint16_t length, const unichar* string)
{
	std::string result;
	UErrorCode error = U_ZERO_ERROR;

	UnicodeString str((char*) string, length*2, g_utf16be, error);
	
	assert(U_SUCCESS(error));
	str.toUTF8String(result);

	return result;
}

bool EqualNoCase(const HFSString& str1, const std::string& str2)
{
	UErrorCode error = U_ZERO_ERROR;
	UnicodeString ustr = UnicodeString::fromUTF8(str2);
	UnicodeString ustr2 = UnicodeString((char*)str1.string, be(str1.length)*2, g_utf16be, error);
	
	assert(U_SUCCESS(error));
	
	return ustr.caseCompare(ustr2, 0) == 0;
}

bool EqualCase(const HFSString& str1, const std::string& str2)
{
	UErrorCode error = U_ZERO_ERROR;
	UnicodeString ustr = UnicodeString::fromUTF8(str2);
	UnicodeString ustr2 = UnicodeString((char*)str1.string, be(str1.length)*2, g_utf16be, error);
	
	assert(U_SUCCESS(error));
	
	return ustr == ustr2;
}

uint16_t StringToUnichar(const std::string& in, unichar* out, size_t maxLength)
{
	UErrorCode error = U_ZERO_ERROR;
	UnicodeString str = UnicodeString::fromUTF8(in);
	auto bytes = str.extract((char*) out, maxLength*sizeof(unichar), g_utf16be, error);
	
	assert(U_SUCCESS(error));
	
	return bytes / sizeof(unichar);
}

void InitConverter()
{
	UErrorCode error = U_ZERO_ERROR;
	g_utf16be = ucnv_open("UTF-16BE", &error);
	
	assert(U_SUCCESS(error));
}

void ExitConverter()
{
	ucnv_close(g_utf16be);
}
