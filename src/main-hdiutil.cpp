#include <iostream>
#include <cstring>
#include <string>
#include <errno.h>
#include <unistd.h>
#include <cstdlib>
#include <elfcalls.h>
#include <sys/wait.h>
#include <spawn.h>

static void printMount();

#define main main_fuse
#define BEFORE_MOUNT_EXTRA printMount()
#include "main-fuse.cpp"
#undef main

static void printHelp();
static int doAttach(int argc, char** argv);
static int doDetach(int argc, char** argv);
static void addFusermountIntoPath();

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

	mount = "/Volumes/";

	dmg = argv[2];

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

	addFusermountIntoPath();

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

extern "C"
{
	extern const struct elf_calls* _elfcalls;
	extern char **environ;
}


static int doDetach(int argc, char** argv)
{
	pid_t pid;
	int (*elf_posix_spawn)(pid_t* pid, const char* path, const posix_spawn_file_actions_t *file_actions, const posix_spawnattr_t *attrp, char *const argv[], char *const envp[]);
	const char* pargv[] = { "fusermount", "-u", argv[2], nullptr };

	if (argc != 3)
		printHelp();

	addFusermountIntoPath();

	*((void**)(&elf_posix_spawn)) = _elfcalls->dlsym_fatal(nullptr, "posix_spawn");
	if (elf_posix_spawn(&pid, "fusermount", nullptr, nullptr, (char* const*) pargv, environ) != 0)
	{
		std::cerr << "Failed to execute fusermount!\n";
		return 1;
	}
	else
	{
		int status;
		waitpid(pid, &status, 0);
		rmdir(argv[2]);

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

