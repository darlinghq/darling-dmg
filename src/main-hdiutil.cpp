#include <iostream>
#include <cstring>
#include <string>
#include <errno.h>
#include <unistd.h>
#include <cstdlib>

static void printMount();

#define main main_fuse
#define BEFORE_MOUNT_EXTRA printMount()
#include "main-fuse.cpp"
#undef main

static void printHelp();
static int doAttach(int argc, char** argv);
static int doDetach(int argc, char** argv);
static std::string getPrefix();

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
		"\tattach <file>\n"
		"\t\tMounts a .dmg file <file> and prints the mount locaton\n"
		"\tdetach <mount-path>\n"
		"\t\tUnmounts a .dmg file mounted at <mount-path>\n";

	exit(1);
}

static std::string getPrefix()
{
	std::string prefix;

	if (const char* p = getenv("DPREFIX"))
	{
		prefix = p;

		if (!prefix.empty() && prefix[prefix.length()-1] != '/')
			prefix += '/';
	}
	
	if (prefix.empty())
	{
		const char* home = getenv("HOME");
		if (home == nullptr)
			home = "/";

		prefix = home;
		prefix += ".darling/";
	}

	return prefix;
}

static std::string g_mountDir;

static int doAttach(int argc, char** argv)
{
	if (argc != 3)
		printHelp();

	std::string prefix, mount, dmg;
	std::string mountname;
	const char *p, *p2;
	int fd;
	char output[] = "/tmp/hdiutilXXXXXX";
	const char* args[4];

	prefix = getPrefix();
	mount = prefix;
	mount += "Volumes/";

	if (argv[2][0] == '/')
		dmg = prefix;
	dmg += argv[2];

	if (access(dmg.c_str(), R_OK) != 0)
	{
		std::cerr << "Cannot access " << argv[2] << std::endl;
		return 1;
	}

	p = strrchr(argv[2], '/');
	if (p == nullptr)
		p = argv[2];
	else
		p++;

	p2 = strrchr(argv[2], '.');
	if (p2 == nullptr)
		p2 = argv[2] + strlen(argv[2]);

	mountname = std::string(p, p2-p);
	mount += mountname;
	g_mountDir = "/Volumes/";
	g_mountDir += mountname;

	if (mkdir(mount.c_str(), 0777) == -1 && errno != EEXIST)
	{
		std::cerr << "Cannot mkdir " << mount << std::endl;
		return 1;
	}

	fd = mkstemp(output);

	// redirect stderr into temp file
	dup2(2, 255);
	close(2);

	dup2(fd, 2);

	args[0] = "darling-dmg";
	args[1] = dmg.c_str();
	args[2] = mount.c_str();
	args[3] = nullptr;

	if (main_fuse(3, args) != 0)
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
	std::cout << g_mountDir << std::endl;
}

static int doDetach(int argc, char** argv)
{
	if (argc != 3)
		printHelp();

	std::string path;

	path = getPrefix();
	path += argv[2];

	execlp("fusermount", "fusermount", "-u", path.c_str(), (char*) nullptr);
	std::cerr << "Failed to execute fusermount!\n";

	return 1;
}

