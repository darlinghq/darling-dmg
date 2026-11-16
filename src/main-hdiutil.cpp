#include <iostream>
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
#include <darling-dmg/darling_dmg.h>

static void printMount();

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

	if (darling_dmg_license_count(image) > 0)
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
	darling_dmg_close(image);

	g_mountDir = mountPoint;
	if (!quiet)
		printMount();
	return 0;
}

static void printMount()
{
	if (plist) {
		std::cout << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << std::endl;
		std::cout << "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">" << std::endl;
		std::cout << "<plist version=\"1.0\">" << std::endl;
		std::cout << "<dict>" << std::endl;
		std::cout << "\t<key>system-entities</key>" << std::endl;
		std::cout << "\t<array>" << std::endl;
		std::cout << "\t\t<dict>" << std::endl;
    std::cout << "\t\t\t<key>content-hint</key>" << std::endl;
    std::cout << "\t\t\t<string>GUID_partition_scheme</string>" << std::endl;
    std::cout << "\t\t\t<key>dev-entry</key>" << std::endl;
    std::cout << "\t\t\t<string>/dev/disk1</string>" << std::endl;
    std::cout << "\t\t\t<key>potentially-mountable</key>" << std::endl;
    std::cout << "\t\t\t<false/>" << std::endl;
    std::cout << "\t\t\t<key>unmapped-content-hint</key>" << std::endl;
    std::cout << "\t\t\t<string>GUID_partition_scheme</string>" << std::endl;
    std::cout << "\t\t</dict>" << std::endl;
		std::cout << "\t\t<dict>" << std::endl;
		std::cout << "\t\t\t<key>content-hint</key>" << std::endl;
		std::cout << "\t\t\t<string>Apple_HFS</string>" << std::endl;
		std::cout << "\t\t\t<key>dev-entry</key>" << std::endl;
		std::cout << "\t\t\t<string>/dev/disk1s1</string>" << std::endl;
		std::cout << "\t\t\t<key>mount-point</key>" << std::endl;
		std::cout << "\t\t\t<string>" << xmlEscaped(g_mountDir) << "</string>" << std::endl;
		std::cout << "\t\t\t<key>potentially-mountable</key>" << std::endl;
		std::cout << "\t\t\t<true/>" << std::endl;
		std::cout << "\t\t\t<key>unmapped-content-hint</key>" << std::endl;
		std::cout << "\t\t\t<string>48465300-0000-11AA-AA11-00306543ECAC</string>" << std::endl;
		std::cout << "\t\t\t<key>volume-kind</key>" << std::endl;
		std::cout << "\t\t\t<string>hfs</string>" << std::endl;
		std::cout << "\t\t</dict>" << std::endl;
		std::cout << "\t</array>" << std::endl;
		std::cout << "</dict>" << std::endl;
		std::cout << "</plist>" << std::endl;
	} else if (!puppetstrings) {
		std::cout << g_mountDir << std::endl;
	} else {
		// FIXME: This is kind of fake...
		// puppetstrings should produce output like:
		// /dev/disk1              GUID_partition_scheme           
		// /dev/disk1s1            Apple_HFS                       /Volumes/MachOView
		std::cout << "/dev/disk1\tGUID_partition_scheme\t\n";
		std::cout << "/dev/disk1s1\tApple_HFS\t" << g_mountDir << std::endl;
	}
}


// The name a detach message gives the image. macOS names the disk ("disk5"); until disk images have disk names
// here, the mount point stands in for it.
static std::string detachName(const char* mountPoint)
{
	return mountPoint;
}

// Detaches the disk image mounted at the given path. Messages and exit statuses follow hdiutil on macOS:
// '"<disk>" ejected.' on success; on a failed unmount 'couldn't unmount "<disk>" - <error>' and the errno as the
// exit status; 'detach failed - No such file or directory' and status 1 for a path no image is mounted on.
static int doDetach(int argc, char** argv)
{
	bool quiet = false;
	int flags = 0;

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
				flags |= MNT_FORCE;
				break;
			case 0:
				break;
			default:
				printHelp();
		}
	}

	if (optind != argc - 1)
		printHelp();

	char path[PATH_MAX];
	struct statfs sfs;
	if (realpath(argv[optind], path) == nullptr || statfs(path, &sfs) != 0 || strcmp(sfs.f_mntonname, path) != 0)
	{
		if (!quiet)
			std::cerr << "hdiutil: detach failed - " << strerror(ENOENT) << std::endl;
		return 1;
	}

	std::string name = detachName(path);
	int err = darling_dmg_detach(path, (flags & MNT_FORCE) != 0);
	if (err != 0)
	{
		if (!quiet)
			std::cerr << "hdiutil: couldn't unmount \"" << name << "\" - " << strerror(err) << std::endl;
		return err;
	}

	if (!quiet)
		std::cout << "\"" << name << "\" ejected." << std::endl;
	return 0;
}
