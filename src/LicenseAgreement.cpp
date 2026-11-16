#include "LicenseAgreement.h"
#include <cerrno>
#include <iconv.h>

namespace
{

// The first resource ID of the agreement resources; LPic entries give the others as offsets from it.
const int kBaseResourceID = 5000;

struct Region
{
	uint16_t code;         // Mac region code (Script Manager "ver..." constants)
	const char* language;  // BCP 47
	const char* encoding;  // iconv name of the region's Mac text encoding
};

// Region codes to languages as CFLocaleCreateCanonicalLocaleIdentifierFromScriptManagerCodes reports them on
// macOS 26.7.3 (black-box, see tests/dmg-license.md); the text encoding is the one the Mac script of that
// language uses. Regions without a converter available here are left out.
const Region kRegions[] = {
	{ 0, "en-US", "MACINTOSH" }, { 1, "fr-FR", "MACINTOSH" }, { 2, "en-GB", "MACINTOSH" },
	{ 3, "de-DE", "MACINTOSH" }, { 4, "it-IT", "MACINTOSH" }, { 5, "nl-NL", "MACINTOSH" },
	{ 6, "nl-BE", "MACINTOSH" }, { 7, "sv-SE", "MACINTOSH" }, { 8, "es-ES", "MACINTOSH" },
	{ 9, "da-DK", "MACINTOSH" }, { 10, "pt-PT", "MACINTOSH" }, { 11, "fr-CA", "MACINTOSH" },
	{ 12, "nb-NO", "MACINTOSH" }, { 13, "he-IL", "MACHEBREW" }, { 14, "ja-JP", "SHIFT_JIS" },
	{ 15, "en-AU", "MACINTOSH" }, { 16, "ar", "MACARABIC" }, { 17, "fi-FI", "MACINTOSH" },
	{ 18, "fr-CH", "MACINTOSH" }, { 19, "de-CH", "MACINTOSH" }, { 20, "el-GR", "MACGREEK" },
	{ 21, "is-IS", "MACICELAND" }, { 23, "el-CY", "MACGREEK" }, { 24, "tr-TR", "MACTURKISH" },
	{ 25, "hr-HR", "MACCROATIAN" }, { 36, "it-CH", "MACINTOSH" }, { 39, "ro-RO", "MACROMANIA" },
	{ 41, "lt-LT", "MACCENTRALEUROPE" }, { 42, "pl-PL", "MACCENTRALEUROPE" }, { 43, "hu-HU", "MACCENTRALEUROPE" },
	{ 44, "et-EE", "MACCENTRALEUROPE" }, { 45, "lv-LV", "MACCENTRALEUROPE" }, { 47, "fo-FO", "MACICELAND" },
	{ 49, "ru-RU", "MACCYRILLIC" }, { 51, "ko-KR", "EUC-KR" }, { 52, "zh-CN", "GB2312" },
	{ 53, "zh-TW", "BIG5" }, { 54, "th-TH", "MACTHAI" }, { 56, "cs-CZ", "MACCENTRALEUROPE" },
	{ 57, "sk-SK", "MACCENTRALEUROPE" }, { 61, "be-BY", "MACCYRILLIC" }, { 62, "uk-UA", "MACUKRAINE" },
	{ 65, "sr-RS", "MACCYRILLIC" }, { 66, "sl-SI", "MACCROATIAN" }, { 67, "mk-MK", "MACCYRILLIC" },
	{ 68, "hr-HR", "MACCROATIAN" }, { 71, "pt-BR", "MACINTOSH" }, { 72, "bg-BG", "MACCYRILLIC" },
	{ 73, "ca-ES", "MACINTOSH" }, { 82, "en-CA", "MACINTOSH" }, { 86, "es-419", "MACINTOSH" },
	{ 92, "de-AT", "MACINTOSH" }, { 98, "fr-BE", "MACINTOSH" }, { 100, "en-SG", "MACINTOSH" },
	{ 101, "nn-NO", "MACINTOSH" }, { 102, "af-ZA", "MACINTOSH" }, { 108, "en-IE", "MACINTOSH" },
};

const Region* regionForCode(uint16_t code)
{
	for (const Region& region : kRegions)
	{
		if (region.code == code)
			return &region;
	}
	return nullptr;
}

uint16_t readBE16(const std::vector<uint8_t>& data, size_t offset)
{
	return uint16_t(data[offset] << 8 | data[offset + 1]);
}

// Converts text in the given encoding to UTF-8; false if it is not valid in that encoding.
bool toUTF8(const uint8_t* bytes, size_t length, const char* encoding, std::string& out)
{
	iconv_t cd = iconv_open("UTF-8", encoding);
	if (cd == (iconv_t) -1)
		return false;

	out.clear();
	char* in = (char*) bytes;
	size_t inLeft = length;
	bool ok = true;
	char buffer[1024];

	while (ok && inLeft > 0)
	{
		char* outPtr = buffer;
		size_t outLeft = sizeof(buffer);
		size_t result = iconv(cd, &in, &inLeft, &outPtr, &outLeft);
		out.append(buffer, outPtr - buffer);
		if (result == (size_t) -1 && errno != E2BIG)
			ok = false;
	}

	iconv_close(cd);
	return ok;
}

// Mac text uses CR for line breaks; each one becomes a '\n' (so a CR LF pair is two line breaks).
void normalizeLineBreaks(std::string& text)
{
	for (char& c : text)
	{
		if (c == '\r')
			c = '\n';
	}
}

// Reads the Pascal strings of a 'STR#' resource: language name, agree, disagree, print, save, prompt.
bool readStrings(const std::vector<uint8_t>& data, const char* encoding, LicenseAgreement& agreement)
{
	if (data.size() < 2)
		return false;

	std::string* targets[] = { &agreement.languageName, &agreement.agree, &agreement.disagree, &agreement.print,
		&agreement.save, &agreement.message };
	size_t count = readBE16(data, 0);
	size_t offset = 2;

	for (size_t i = 0; i < count && i < sizeof(targets) / sizeof(targets[0]); i++)
	{
		if (offset >= data.size() || offset + 1 + data[offset] > data.size())
			return false;
		std::string value;
		if (!toUTF8(&data[offset + 1], data[offset], encoding, value))
			return false;
		normalizeLineBreaks(value);
		*targets[i] = value;
		offset += 1 + data[offset];
	}
	return count >= sizeof(targets) / sizeof(targets[0]);
}

} // namespace

std::vector<LicenseAgreement> loadLicenseAgreements(const ResourceLoader& loadResource)
{
	std::vector<LicenseAgreement> agreements;
	std::vector<uint8_t> lpic;

	// LPic: default language, entry count, then per entry the region code, the resource ID offset and a
	// two-byte-text flag (all big-endian 16-bit). A table whose size does not match its count is unusable.
	if (!loadResource("LPic", kBaseResourceID, lpic) || lpic.size() < 4)
		return agreements;
	size_t count = readBE16(lpic, 2);
	if (lpic.size() != 4 + count * 6)
		return agreements;
	uint16_t defaultLanguage = readBE16(lpic, 0);

	std::vector<int> resourceIDs;
	std::vector<uint16_t> regionCodes;
	for (size_t i = 0; i < count; i++)
	{
		uint16_t regionCode = readBE16(lpic, 4 + i * 6);
		int resourceID = kBaseResourceID + int16_t(readBE16(lpic, 4 + i * 6 + 2));
		const Region* region = regionForCode(regionCode);
		if (region == nullptr)
			continue;

		LicenseAgreement agreement;
		std::vector<uint8_t> data;
		agreement.language = region->language;

		if (loadResource("TEXT", resourceID, data))
		{
			if (!toUTF8(data.data(), data.size(), region->encoding, agreement.text))
				continue;
		}
		else if (loadResource("RTF ", resourceID, data))
		{
			agreement.isRTF = true;
			if (!toUTF8(data.data(), data.size(), region->encoding, agreement.text))
				continue;
		}
		else
			continue;
		normalizeLineBreaks(agreement.text);

		if (loadResource("STR#", resourceID, data))
			agreement.hasStrings = readStrings(data, region->encoding, agreement);

		agreements.push_back(agreement);
		resourceIDs.push_back(resourceID);
		regionCodes.push_back(regionCode);
	}

	if (agreements.empty())
		return agreements;

	// The default language field is described both as a region code and, by tools that write it, as a resource
	// ID (5000 and up); either names an entry. If it names none, the first language is the default.
	size_t defaultIndex = 0;
	for (size_t i = 0; i < agreements.size(); i++)
	{
		if (defaultLanguage >= kBaseResourceID ? resourceIDs[i] == defaultLanguage : regionCodes[i] == defaultLanguage)
		{
			defaultIndex = i;
			break;
		}
	}
	agreements[defaultIndex].isDefault = true;

	return agreements;
}
