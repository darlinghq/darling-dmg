#ifndef DMGEXTRACTOR_H
#define DMGEXTRACTOR_H

#include <string>

// Extract the first HFS+/HFSX partition from a read-only disk image into an
// existing, empty directory. This does not require a FUSE device.
void extractDMG(const std::string& imagePath, const std::string& destination);

#endif
