#include <iostream>
#include <cstdio>
#include <cstring>
#include <string>
#include <errno.h>
#include <unistd.h>
#include <cstdlib>
#include <getopt.h>
#include <limits.h>
#include <sys/mount.h>
#include <sys/param.h>
#include <vector>

#include <CoreFoundation/CoreFoundation.h>
#include <DiskArbitration/DiskArbitration.h>
#include <darling-dmg/darling_dmg.h>

static void printMount(const std::string& scheme);

static void printHelp();
static int doAttach(int argc, char** argv);
static int doDetach(int argc, char** argv);

int main(int argc, char** argv)
{
	if (argc < 2)
		printHelp();

	if (strcmp(argv[1], "attach") == 0)
		return doAttach(argc, argv);
	else if (strcmp(argv[1], "detach") == 0)
		return doDetach(argc, argv);

	printHelp();
	return 1;
}

static void printHelp()
{
	std::cerr << "Usage: hdiutil <action>\n\n";
	std::cerr << "Possible actions:\n"
		"\tattach [options] <file>\n"
		"\t\tMounts a .dmg file <file> and prints the mount locaton\n"
		"\tdetach [options] <mount-path>\n"
		"\t\tUnmounts a .dmg file mounted at <mount-path>\n";

	exit(1);
}

static std::string g_mountDir;

// Text as the content of an XML element.
static std::string xmlEscaped(const std::string& text)
{
	std::string escaped;
	for (char c : text)
	{
		switch (c)
		{
			case '&': escaped += "&amp;"; break;
			case '<': escaped += "&lt;"; break;
			case '>': escaped += "&gt;"; break;
			default: escaped += c;
		}
	}
	return escaped;
}
static bool puppetstrings = false;
static bool plist = false;

// The user's preferred languages, most preferred first (BCP 47).
static std::vector<std::string> preferredLanguages()
{
	std::vector<std::string> languages;
	CFArrayRef preferred = CFLocaleCopyPreferredLanguages();
	if (preferred == nullptr)
		return languages;

	for (CFIndex i = 0; i < CFArrayGetCount(preferred); i++)
	{
		CFStringRef language = (CFStringRef) CFArrayGetValueAtIndex(preferred, i);
		char buffer[64];
		if (CFStringGetCString(language, buffer, sizeof(buffer), kCFStringEncodingUTF8))
			languages.push_back(buffer);
	}
	CFRelease(preferred);
	return languages;
}

// The agreement text as hdiutil prints it: a single line break joins lines with a space, two or more separate
// paragraphs by an empty line, and the text ends with a line break (an empty line if it ended in two or more).
// QUIRK: this reflowing is not documented. Evidence: black-box observation on macOS 26.7.3 (tier 2), recorded in
// tests/dmg-license.md. Test: tests/dmg-license.sh, "line breaks in the text".
static std::string reflowAgreement(const std::string& text)
{
	std::string result;
	size_t i = text.find_first_not_of('\n');

	while (i != std::string::npos && i < text.size())
	{
		size_t end = text.find('\n', i);
		if (end == std::string::npos)
		{
			result.append(text, i, std::string::npos);
			return result + "\n";
		}
		result.append(text, i, end - i);

		size_t next = text.find_first_not_of('\n', end);
		size_t breaks = (next == std::string::npos ? text.size() : next) - end;
		if (next == std::string::npos)
			return result + (breaks >= 2 ? "\n\n" : "\n");
		result += breaks >= 2 ? "\n\n" : " ";
		i = next;
	}
	return result;
}

// Shows the image's licence agreement and reads the answer from stdin, one line at a time: a line starting with
// y or Y agrees, one starting with n or N, an empty line or the end of input declines, anything else is asked
// again (leading blanks are skipped). The prompt is only shown on a terminal; -quiet never agrees.
// QUIRK: none of this is documented. Evidence: black-box observation on macOS 26.7.3 (tier 2), recorded in
// tests/dmg-license.md. Tests: tests/dmg-license.sh, "answer ..." cases, "empty answer", "-quiet does not agree",
// "-plist, no answer", "-plist, answer y: text, then the plist" (the text goes to stdout ahead of the plist).
static bool userAgrees(const struct darling_dmg_license* license, bool quiet)
{
	if (quiet)
		return false;

	std::cout << reflowAgreement(license->text) << std::flush;

	bool terminal = isatty(STDIN_FILENO);
	std::string line;
	for (;;)
	{
		if (terminal)
			std::cout << "Agree Y/N? " << std::flush;
		if (!std::getline(std::cin, line))
			return false;

		size_t first = line.find_first_not_of(" \t");
		if (first == std::string::npos)
			return false;
		if (line[first] == 'y' || line[first] == 'Y')
			return true;
		if (line[first] == 'n' || line[first] == 'N')
			return false;
	}
}

static int doAttach(int argc, char** argv)
{
	if (argc < 3)
		printHelp();

	static const struct option longopts[] = {
		{ "puppetstrings", no_argument, NULL, 1 },
		{ "mountpoint", required_argument, NULL, 2 },
		{ "mountroot", required_argument, NULL, 3 },
		{ "quiet", no_argument, NULL, 4 },
		{ "nobrowse", no_argument, NULL, 5 },
		{ "noautoopen", no_argument, NULL, 5 },
		{ "mountrandom", required_argument, NULL, 6 },
		{ "plist", no_argument, NULL, 7 },
		{ "readonly", no_argument, NULL, 8 },
		{ "noidme", no_argument, NULL, 9 },
		{ "noverify", no_argument, NULL, 10 },
		{ "verify", no_argument, NULL, 10 },
		{ "noautofsck", no_argument, NULL, 11 },
		{ "autofsck", no_argument, NULL, 11 },
		{ "private", no_argument, NULL, 12 },
		{ NULL, 0, NULL, 0 }
	};

	std::string mount, dmg, mountroot;
	bool quiet = false;

	mountroot = "/Volumes";

	// Skip "attach"
	argc--;
	argv++;

	int ch;
	while ((ch = getopt_long_only(argc, argv, "", longopts, NULL)) != -1)
	{
		switch (ch)
		{
			case 1:
				puppetstrings = true;
				break;
			case 2:
				mount = optarg;
				break;
			case 3:
				mountroot = optarg;
				break;
			case 4:
				quiet = true;
				break;
			case 5:
				// We don't implement opening Finder yet, so this is our default
				break;
			case 6: {
				size_t opt_len = strlen(optarg);
				bool append_slash = optarg[opt_len - 1] != '/';
				size_t template_len = opt_len + 6 + (append_slash ? 1 : 0);
				char* template_string = new char[template_len + 1];
				strcpy(template_string, optarg);
				if (append_slash)
					template_string[opt_len] = '/';
				for (size_t i = 0; i < 6; ++i)
					template_string[opt_len + i + (append_slash ? 1 : 0)] = 'X';
				template_string[template_len] = '\0';
				if (!mkdtemp(template_string)) {
					delete[] template_string;
					std::cerr << "Failed to create temporary directory" << std::endl;
					return 1;
				}
				mount = std::string(template_string);
				delete[] template_string;
			} break;
			case 7:
				plist = true;
				break;
			case 8:
				// readonly is already enforced by default
				break;
			case 9:
				// IDME (download post-processing) is not supported anyways, so this option does nothing
				break;
			case 10:
			case 11:
				// Verification and fsck flags are ignored by darling-dmg
				break;
			case 12:
				// -private keeps the volume out of Finder and the like on macOS; there is
				// no such thing here, so the option is accepted and ignored.
				break;
			case 0:
				break;
			default:
				std::cerr << "Got ch " << ch << std::endl;
				printHelp();
		}
	}

	if (optind != argc-1)
	{
		printHelp();
	}

	dmg = argv[optind];

	darling_dmg_image* image;
	int err = darling_dmg_open(dmg.c_str(), &image);
	if (err != 0)
	{
		if (!quiet)
			std::cerr << "hdiutil: attach failed - " << (err == EFTYPE ? "image not recognized" : strerror(err)) << std::endl;
		return 1;
	}

	// An image that is attached already is not asked about again: attaching it gives its volume (macOS 26.7.3).
	char attachedPoint[PATH_MAX];
	if (darling_dmg_license_count(image) > 0
		&& darling_dmg_attached_mount_point(image, attachedPoint, sizeof(attachedPoint)) == ENOENT)
	{
		std::vector<std::string> languages = preferredLanguages();
		std::vector<const char*> preferred;
		for (const std::string& language : languages)
			preferred.push_back(language.c_str());

		const struct darling_dmg_license* license = darling_dmg_license_for_languages(image, preferred.data(), preferred.size());
		if (license != nullptr && !userAgrees(license, quiet))
		{
			darling_dmg_close(image);
			if (!quiet)
				std::cerr << "hdiutil: attach canceled" << std::endl;
			return 1;
		}
	}

	struct darling_dmg_attach_options options = { mountroot.c_str(), mount.empty() ? nullptr : mount.c_str() };
	char mountPoint[PATH_MAX];
	err = darling_dmg_attach(image, &options, mountPoint, sizeof(mountPoint));
	if (err != 0)
	{
		if (!quiet)
		{
			std::cerr << "hdiutil: attach failed - " << strerror(err) << std::endl;
			const char* messages = darling_dmg_attach_messages(image);
			if (messages != nullptr)
				std::cerr << messages << std::endl;
		}
		darling_dmg_close(image);
		return 1;
	}
	std::string scheme = darling_dmg_partition_scheme(image);
	darling_dmg_close(image);

	g_mountDir = mountPoint;
	if (!quiet)
		printMount(scheme);
	return 0;
}

// The BSD names DiskArbitration gives the volume mounted at mountPoint and its whole disk; false if it does not
// know the volume.
static bool diskNames(const char* mountPoint, std::string& leaf, std::string& whole)
{
	DASessionRef session = DASessionCreate(NULL);
	if (session == NULL)
		return false;

	CFURLRef url = CFURLCreateFromFileSystemRepresentation(NULL, (const UInt8*) mountPoint, strlen(mountPoint), true);
	DADiskRef disk = url != NULL ? DADiskCreateFromVolumePath(NULL, session, url) : NULL;
	DADiskRef wholeDisk = disk != NULL ? DADiskCopyWholeDisk(disk) : NULL;
	bool found = disk != NULL && wholeDisk != NULL && DADiskGetBSDName(disk) != NULL && DADiskGetBSDName(wholeDisk) != NULL;
	if (found)
	{
		leaf = DADiskGetBSDName(disk);
		whole = DADiskGetBSDName(wholeDisk);
	}

	if (wholeDisk != NULL)
		CFRelease(wholeDisk);
	if (disk != NULL)
		CFRelease(disk);
	if (url != NULL)
		CFRelease(url);
	CFRelease(session);
	return found;
}

static void printTableLine(const std::string& device, const std::string& hint, const std::string& mountPoint)
{
	printf("%-20s\t%-31s\t%s\n", device.c_str(), hint.c_str(), mountPoint.c_str());
}

static void printPlistString(const char* key, const std::string& value)
{
	std::cout << "\t\t\t<key>" << key << "</key>" << std::endl;
	std::cout << "\t\t\t<string>" << xmlEscaped(value) << "</string>" << std::endl;
}

// What attach prints, as on macOS (tests/dmg-attach.md): a line for the whole disk with its partition scheme, then
// one for the volume's partition with its mount point; with -plist, the volume's entry and then the whole disk's.
// Darling's disks always have the volume on the first partition, also for an image without a partition map (its
// whole disk then has no scheme). If DiskArbitration does not know the volume, only the mount point is given.
static void printMount(const std::string& scheme)
{
	std::string leaf, whole;
	bool named = diskNames(g_mountDir.c_str(), leaf, whole);
	// the type of the volume's partition in the map
	std::string unmappedHint = scheme == "GUID_partition_scheme" ? "48465300-0000-11AA-AA11-00306543ECAC" : "Apple_HFS";

	if (plist) {
		std::cout << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << std::endl;
		std::cout << "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">" << std::endl;
		std::cout << "<plist version=\"1.0\">" << std::endl;
		std::cout << "<dict>" << std::endl;
		std::cout << "\t<key>system-entities</key>" << std::endl;
		std::cout << "\t<array>" << std::endl;
		std::cout << "\t\t<dict>" << std::endl;
		printPlistString("content-hint", "Apple_HFS");
		if (named)
			printPlistString("dev-entry", "/dev/" + leaf);
		printPlistString("mount-point", g_mountDir);
		std::cout << "\t\t\t<key>potentially-mountable</key>" << std::endl;
		std::cout << "\t\t\t<true/>" << std::endl;
		printPlistString("unmapped-content-hint", unmappedHint);
		printPlistString("volume-kind", "hfs");
		std::cout << "\t\t</dict>" << std::endl;
		if (named) {
			std::cout << "\t\t<dict>" << std::endl;
			printPlistString("content-hint", scheme);
			printPlistString("dev-entry", "/dev/" + whole);
			std::cout << "\t\t\t<key>potentially-mountable</key>" << std::endl;
			std::cout << "\t\t\t<false/>" << std::endl;
			printPlistString("unmapped-content-hint", scheme);
			std::cout << "\t\t</dict>" << std::endl;
		}
		std::cout << "\t</array>" << std::endl;
		std::cout << "</dict>" << std::endl;
		std::cout << "</plist>" << std::endl;
	} else if (named) {
		printTableLine("/dev/" + whole, scheme, "");
		printTableLine("/dev/" + leaf, "Apple_HFS", g_mountDir);
	} else {
		std::cout << g_mountDir << std::endl;
	}
	fflush(stdout);
}

struct RequestResult
{
	bool done;
	DAReturn status;
	std::string string; // the dissenter's status string, if it has one
};

static void requestDone(DADiskRef disk, DADissenterRef dissenter, void* context)
{
	RequestResult* result = (RequestResult*) context;
	result->done = true;
	result->status = dissenter != NULL ? DADissenterGetStatus(dissenter) : kDAReturnSuccess;
	CFStringRef string = dissenter != NULL ? DADissenterGetStatusString(dissenter) : NULL;
	char text[1024];
	if (string != NULL && CFStringGetCString(string, text, sizeof(text), kCFStringEncodingUTF8))
		result->string = text;
}

// Runs the session's run loop until the request is done.
static void waitFor(const RequestResult& result)
{
	while (!result.done)
		CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1, true);
}

// The errno hdiutil reports for a failed unmount or eject.
static int errnoForStatus(DAReturn status)
{
	if ((status & 0xFFFFFF00) == 0xC000)
		return status & 0xFF; // the errno the unmount failed with (see diskarbitrationd's DAOperations.c)
	switch (status)
	{
		case kDAReturnBusy: return EBUSY;
		case kDAReturnBadArgument: return EINVAL;
		case kDAReturnNotPrivileged: return EPERM;
		case kDAReturnNotMounted: return EINVAL;
		case kDAReturnUnsupported: return ENOTSUP;
		default: return EIO;
	}
}

// Reports a failed request, as on macOS: the dissenter's string (if any) on stdout without a newline, then the
// error on stderr. Returns the exit status, the low byte of the request's status.
static int requestFailed(const char* verb, const std::string& name, const RequestResult& result, bool quiet)
{
	if (!quiet)
	{
		std::cout << result.string << std::flush;
		std::cerr << "hdiutil: couldn't " << verb << " \"" << name << "\" - " << strerror(errnoForStatus(result.status)) << std::endl;
	}
	return result.status & 0xFF;
}

// The disk a detach argument names: a /dev/ disk name, or the mount point of a volume. NULL if DiskArbitration
// knows no such disk.
static DADiskRef diskForArgument(DASessionRef session, const char* argument)
{
	if (strncmp(argument, "/dev/", 5) == 0)
	{
		DADiskRef disk = DADiskCreateFromBSDName(NULL, session, argument + 5);
		CFDictionaryRef description = disk != NULL ? DADiskCopyDescription(disk) : NULL;
		if (description == NULL)
		{
			if (disk != NULL)
				CFRelease(disk);
			return NULL;
		}
		CFRelease(description);
		return disk;
	}

	char path[PATH_MAX];
	if (realpath(argument, path) == nullptr)
		return NULL;
	CFURLRef url = CFURLCreateFromFileSystemRepresentation(NULL, (const UInt8*) path, strlen(path), true);
	if (url == NULL)
		return NULL;
	DADiskRef disk = DADiskCreateFromVolumePath(NULL, session, url);
	CFRelease(url);
	return disk;
}

// Detaches the volume mounted at path, which DiskArbitration does not know (it is outside the guest's tree, which
// getfsstat lists), with the disk image library. The mount point names the image in the messages.
static int detachUnknownVolume(const char* argument, bool force, bool quiet)
{
	char path[PATH_MAX];
	struct statfs sfs;
	if (strncmp(argument, "/dev/", 5) == 0 || realpath(argument, path) == nullptr || statfs(path, &sfs) != 0
		|| strcmp(sfs.f_mntonname, path) != 0)
	{
		if (!quiet)
			std::cerr << "hdiutil: detach failed - " << strerror(ENOENT) << std::endl;
		return 1;
	}

	int err = darling_dmg_detach(path, force);
	if (err != 0)
	{
		if (!quiet)
			std::cerr << "hdiutil: couldn't unmount \"" << path << "\" - " << strerror(err) << std::endl;
		return err;
	}
	if (!quiet)
		std::cout << "\"" << path << "\" ejected." << std::endl;
	return 0;
}

// Detaches the disk image a mount point or /dev/ disk name belongs to, through DiskArbitration: its whole disk is
// unmounted (forcibly with -force), then ejected. Messages and exit statuses follow hdiutil on macOS
// (tests/dmg-attach.md): '"<disk>" ejected.' on success; 'detach failed - No such file or directory' and status 1
// for something that is not a disk; otherwise 'couldn't unmount|eject "<disk>" - <error>'. A volume DiskArbitration
// does not know is detached without it.
static int doDetach(int argc, char** argv)
{
	bool quiet = false;
	bool force = false;

	if (argc < 3)
		printHelp();

	static const struct option longopts[] = {
		{ "quiet", no_argument, NULL, 1 },
		{ "force", no_argument, NULL, 2 },
		{ NULL, 0, NULL, 0 }
	};

	// Skip "detach"
	argc--;
	argv++;

	int ch;
	while ((ch = getopt_long_only(argc, argv, "", longopts, NULL)) != -1)
	{
		switch (ch)
		{
			case 1:
				quiet = true;
				break;
			case 2:
				force = true;
				break;
			case 0:
				break;
			default:
				printHelp();
		}
	}

	if (optind != argc - 1)
		printHelp();

	DASessionRef session = DASessionCreate(NULL);
	DADiskRef disk = session != NULL ? diskForArgument(session, argv[optind]) : NULL;
	DADiskRef whole = disk != NULL ? DADiskCopyWholeDisk(disk) : NULL;
	if (whole == NULL || DADiskGetBSDName(whole) == NULL)
	{
		if (whole != NULL)
			CFRelease(whole);
		if (disk != NULL)
			CFRelease(disk);
		if (session != NULL)
			CFRelease(session);
		return detachUnknownVolume(argv[optind], force, quiet);
	}
	std::string name = DADiskGetBSDName(whole);
	DASessionScheduleWithRunLoop(session, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);

	int exitStatus = 0;
	RequestResult unmounted = {};
	DADiskUnmount(whole, kDADiskUnmountOptionWhole | (force ? kDADiskUnmountOptionForce : 0), requestDone, &unmounted);
	waitFor(unmounted);
	if (unmounted.status != kDAReturnSuccess)
	{
		exitStatus = requestFailed("unmount", name, unmounted, quiet);
	}
	else
	{
		RequestResult ejected = {};
		DADiskEject(whole, kDADiskEjectOptionDefault, requestDone, &ejected);
		waitFor(ejected);
		if (ejected.status != kDAReturnSuccess)
			exitStatus = requestFailed("eject", name, ejected, quiet);
		else if (!quiet)
			std::cout << "\"" << name << "\" ejected." << std::endl;
	}

	DASessionUnscheduleFromRunLoop(session, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
	CFRelease(whole);
	CFRelease(disk);
	CFRelease(session);
	return exitStatus;
}
