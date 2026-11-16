#ifndef LICENSEAGREEMENT_H
#define LICENSEAGREEMENT_H
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

// A licence agreement embedded in a UDIF image, in one language, with every string converted to UTF-8.
struct LicenseAgreement
{
	std::string language; // BCP 47
	bool isDefault = false;
	bool isRTF = false;
	std::string text;
	bool hasStrings = false;
	std::string languageName, agree, disagree, print, save, message;
};

// Fetches the data of a resource by type and ID; false if there is none.
typedef std::function<bool(const std::string& type, int id, std::vector<uint8_t>& data)> ResourceLoader;

// Reads the agreements from the 'LPic' (language table), 'TEXT'/'RTF ' and 'STR#' resources. An image without a
// language table, or with one that is malformed, has none. Languages whose region code or text encoding is not
// known are left out.
std::vector<LicenseAgreement> loadLicenseAgreements(const ResourceLoader& loadResource);

#endif
