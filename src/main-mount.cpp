// darling-dmg-mount: runs the FUSE file system for one image attached through libdarling-dmg.
//
// Usage: darling-dmg-mount <image> <mount point> <ready fd>
//
// <ready fd> is normally 2: until the helper reports, its stderr (and so whatever FUSE or fusermount print) goes
// to the attaching process. Once the volume is mounted, "0\n" is written to it; if it cannot be mounted,
// "<errno>\n" is. Either way the descriptor is then pointed at /dev/null. The process that started the helper gets its exit status right away: the
// file system runs in a child in a session of its own, so it outlives whoever attached the image.
#include <cstdlib>
#include <cstring>
#include <string>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <elfcalls.h>

#define main main_fuse
#define BEFORE_MOUNT_EXTRA ops.init = mountReady
static void* mountReady(struct fuse_conn_info* conn);
#include "main-fuse.cpp"
#undef main

extern "C"
{
	extern const struct elf_calls* _elfcalls;
	int __darling_vchroot_expand(const char* path, char* out);
}

static int g_readyFd = -1;

static void reportMount(int err)
{
	if (g_readyFd < 0)
		return;
	std::string report = std::to_string(err) + "\n";
	write(g_readyFd, report.c_str(), report.size());

	int devNull = open("/dev/null", O_WRONLY);
	if (devNull >= 0)
	{
		dup2(devNull, g_readyFd);
		close(devNull);
	}
	else
		close(g_readyFd);
	g_readyFd = -1;
}

static void* mountReady(struct fuse_conn_info*)
{
	reportMount(0);
	return nullptr;
}

// libfuse runs fusermount itself, through the host's environment rather than ours.
static void addFusermountIntoPath()
{
	int (*elf_setenv)(const char* name, const char* value, int overwrite);
	const char* path = getenv("PATH");
	std::string newPath = path ? path : "/usr/bin:/bin";

	newPath += ":/Volumes/SystemRoot/bin:/Volumes/SystemRoot/usr/bin";
	*((void**)(&elf_setenv)) = _elfcalls->dlsym_fatal(nullptr, "setenv");
	elf_setenv("PATH", newPath.c_str(), true);
}

// FUSE splits -o values at commas; a backslash keeps one in a value.
static std::string escapeOption(const std::string& value)
{
	std::string escaped;
	for (char c : value)
	{
		if (c == ',' || c == '\\')
			escaped += '\\';
		escaped += c;
	}
	return escaped;
}

int main(int argc, char** argv)
{
	if (argc != 4)
		return 2;

	g_readyFd = atoi(argv[3]);
	for (int fd = getdtablesize() - 1; fd > 2; fd--)
	{
		if (fd != g_readyFd)
			close(fd);
	}

	pid_t pid = fork();
	if (pid < 0)
	{
		reportMount(errno);
		return 1;
	}
	if (pid > 0)
		_exit(0);
	setsid();

	addFusermountIntoPath();

	char hostMountPoint[4096];
	__darling_vchroot_expand(argv[2], hostMountPoint);

	std::string options = "subtype=darling-dmg,fsname=" + escapeOption(argv[1]) + ",nonempty";
	const char* args[] = { "darling-dmg-mount", argv[1], hostMountPoint, "-f", "-o", options.c_str(), nullptr };

	int result = main_fuse(6, args);
	reportMount(EIO); // only reaches the caller if the file system never came up
	return result;
}
