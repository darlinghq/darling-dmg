#ifndef MOUNTPOINTNAME_H
#define MOUNTPOINTNAME_H
#include <string>

// The directory name a volume is mounted under: its name as a single path component.
std::string mountPointName(const std::string& volumeName);

// Makes the mount point directory for a volume named name (see mountPointName) in mountRoot, under a name nothing
// else has taken, and returns its path in mountPoint. 0 or an errno value; EEXIST if no free name was found.
int makeMountPoint(const std::string& mountRoot, const std::string& name, std::string& mountPoint);

#endif
