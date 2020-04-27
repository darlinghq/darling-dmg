#include <iostream>
#include <cstring>
#include <string>
#include <errno.h>
#include <unistd.h>
#include <cstdlib>
#include <elfcalls.h>
#include <sys/wait.h>
#include <spawn.h>
#include <getopt.h>

static void printMount();

#define main main_fuse
#define BEFORE_MOUNT_EXTRA printMount(); daemon(false, false)
#include "main-fuse.cpp"
#undef main

static void printHelp();
static void doFork(void);
static int doAttach(int argc, char** argv);
static int doDetach(int argc, char** argv);
static void addFusermountIntoPath();

extern "C" int __darling_vchroot_expand(const char* path, char* out);

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
static bool puppetstrings = false;
static bool plist = false;

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
		{ NULL, 0, NULL, 0 }
	};

	std::string prefix, mount, dmg;
	std::string mountname, mountroot;
	const char *p, *p2;
	int fd;
	char output[] = "/tmp/hdiutilXXXXXX";
	const char* args[7];
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

	if (access(dmg.c_str(), R_OK) != 0)
	{
		std::cerr << "Cannot access " << dmg.c_str() << std::endl;
		return 1;
	}

	p = strrchr(argv[optind], '/');
	if (p == nullptr)
		p = argv[optind];
	else
		p++;

	p2 = strrchr(argv[optind], '.');
	if (p2 == nullptr)
		p2 = argv[optind] + strlen(argv[optind]);

	mountname = std::string(p, p2-p);

	if (mount.empty())
	{
		mount = mountroot;
		if (mount[mount.length()-1] != '/')
			mount += '/';
		mount += mountname;
	}

	if (mkdir(mount.c_str(), 0777) == -1 && errno != EEXIST)
	{
		std::cerr << "Cannot mkdir " << mount << std::endl;
		return 1;
	}
	g_mountDir = mount;

	fd = mkstemp(output);

	// redirect stderr into temp file
	dup2(2, 255);
	close(2);

	dup2(fd, 2);

	args[0] = "darling-dmg";
	args[1] = dmg.c_str();

	char linux_path[4096];
	__darling_vchroot_expand(mount.c_str(), linux_path);
	args[2] = linux_path;

	std::cerr << "Will pass " << args[2] << std::endl;

	args[3] = "-f"; // Fork has to be done by Darling code, otherwise the LKM will not talk to us any more
	args[4] = "-o";
	args[5] = "nonempty";
	args[6] = nullptr;

	addFusermountIntoPath();

	if (main_fuse(4, args) != 0)
	{
		char buf[512];
		int rd;

		// redirect stderr back
		close(2);
		dup2(255, 2);

		lseek(fd, SEEK_SET, 0);

		while ((rd = read(fd, buf, sizeof(buf))) > 0)
		{
			write(2, buf, rd);
		}
		close(fd);
		unlink(output);

		return 1;
	}

	close(fd);
	unlink(output);

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
		std::cout << "\t\t\t<string>" << g_mountDir << "</string>" << std::endl;
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

extern "C"
{
	extern const struct elf_calls* _elfcalls;
	extern char **environ;
}


static int doDetach(int argc, char** argv)
{
	pid_t pid;
	int (*elf_posix_spawnp)(pid_t* pid, const char* path, const posix_spawn_file_actions_t *file_actions, const posix_spawnattr_t *attrp, char *const argv[], char *const envp[]);

	if (argc < 3)
		printHelp();

	static const struct option longopts[] = {
		{ "quiet", no_argument, NULL, 1 },
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
				break;
			case 0:
				break;
			default:
				printHelp();
		}
	}

	addFusermountIntoPath();

	char linux_path[4096];
	__darling_vchroot_expand(argv[optind], linux_path);
	const char* pargv[] = { "fusermount", "-u", linux_path, nullptr };

	*((void**)(&elf_posix_spawnp)) = _elfcalls->dlsym_fatal(nullptr, "posix_spawnp");
	if (elf_posix_spawnp(&pid, "fusermount", nullptr, nullptr, (char* const*) pargv, environ) != 0)
	{
		std::cerr << "Failed to execute fusermount!\n";
		return 1;
	}
	else
	{
		int status;
		waitpid(pid, &status, 0);
		rmdir(argv[optind]);

		return 0;
	}
}

void addFusermountIntoPath()
{
	std::string path = getenv("PATH");
	int (*elf_setenv)(const char *name, const char *value, int overwrite);

	path += ":/Volumes/SystemRoot/bin"
		":/Volumes/SystemRoot/usr/bin";

	// Calling setenv doesn't change current process' environment variables,
	// it only modifies the local copy maintained by libc. This local copy
	// is then passed by libc to execve().
	// Since the execution of 'fusermount' happens in libfuse.so, we need
	// to add it into the environment on the ELF side.
	*((void**)(&elf_setenv)) = _elfcalls->dlsym_fatal(nullptr, "setenv");
	elf_setenv("PATH", path.c_str(), true);
}
