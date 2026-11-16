#include "MountPointName.h"
#include <cerrno>
#include <sys/stat.h>
#include <unistd.h>

namespace
{

// How many " <n>" suffixes are tried before giving up on a name.
const unsigned kMaxSuffixes = 1000;

// QUIRK: when <root>/<name> is taken, the volume is mounted at "<root>/<name> <n>" with the lowest free n, where n
// starts at 1 in /Volumes but at 2 under any other mount root (hdiutil attach -mountroot). Not documented.
// Evidence: black-box observation on macOS 26.7.3 (tier 2), recorded in tests/dmg-attach.md.
// Test: tests/dmg-attach.sh, "two images attached at once" and "same name under -mountroot";
// tests/dmg-mount-point-name.cpp, "taken name, outside /Volumes: 2".
unsigned firstSuffixForTakenName(const std::string& mountRoot)
{
	return mountRoot == "/Volumes" ? 1 : 2;
}

} // namespace

std::string mountPointName(const std::string& volumeName)
{
	// HFS names may contain '/', which POSIX paths show as ':' (macOS mounts a volume named "A/B" at "/Volumes/A:B").
	// A NUL cannot be part of a path at all.
	std::string name;
	for (char c : volumeName)
	{
		if (c == '/')
			name += ':';
		else if (c != '\0')
			name += c;
	}

	// QUIRK (inference, tier 5, awaiting maintainer approval): a volume whose stored name is empty, "." or ".." is
	// mounted as "untitled". hdiutil create stores "untitled" itself when given an empty name, so macOS's handling
	// of an empty stored name could not be observed (tests/dmg-attach.md).
	// Test: tests/dmg-mount-point-name.cpp, "empty name", "dot names", "a name of only NUL".
	if (name.empty() || name == "." || name == "..")
		name = "untitled";
	return name;
}

int makeMountPoint(const std::string& mountRoot, const std::string& name, std::string& mountPoint)
{
	std::string base = mountRoot == "/" ? "/" + name : mountRoot + "/" + name;
	unsigned suffix = firstSuffixForTakenName(mountRoot);
	mountPoint = base;

	for (unsigned tries = 0; tries <= kMaxSuffixes; tries++)
	{
		if (mkdir(mountPoint.c_str(), 0777) == 0)
			return 0;
		if (errno != EEXIST)
			return errno;
		// On macOS the system removes a mount point it made in /Volumes once its volume is unmounted, however that
		// happens. Darling has nothing doing that yet, so a bare unmount(2) leaves the empty directory behind; an
		// empty directory there with nothing mounted on it is such a leftover and the name is free. (rmdir fails
		// for a directory that is not empty and for a mount point.) Callers must keep other attaches from choosing
		// a name until their volume is mounted, or one could take the other's still empty directory.
		if (mountRoot == "/Volumes" && rmdir(mountPoint.c_str()) == 0)
		{
			if (mkdir(mountPoint.c_str(), 0777) == 0)
				return 0;
			if (errno != EEXIST)
				return errno;
		}
		mountPoint = base + " " + std::to_string(suffix++);
	}
	return EEXIST;
}
