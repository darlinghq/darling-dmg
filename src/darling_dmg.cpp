// libdarling-dmg: see include/darling-dmg/darling_dmg.h.
#include <darling-dmg/darling_dmg.h>

#include <cstdlib>
#include <cstring>
#include <string>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <spawn.h>
#include <sys/mount.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "DiskImage.h"
#include "exceptions.h"

extern "C" char** environ;

// Runs the FUSE file system for one attached image; see main-mount.cpp for its arguments and readiness report.
static const char kMountHelper[] = "/usr/libexec/darling-dmg-mount";

struct darling_dmg_image
{
	std::string path;
	std::string volumeName;
	DiskImage image;
};

// The file name without its directory and its last extension.
static std::string nameFromPath(const std::string& path)
{
	size_t slash = path.rfind('/');
	std::string name = path.substr(slash == std::string::npos ? 0 : slash + 1);
	size_t dot = name.rfind('.');
	if (dot != std::string::npos && dot != 0)
		name.erase(dot);
	return name;
}

int darling_dmg_open(const char* path, darling_dmg_image** out)
{
	if (out == nullptr)
		return EINVAL;
	*out = nullptr;
	if (path == nullptr)
		return EINVAL;

	char absolute[PATH_MAX];
	if (realpath(path, absolute) == nullptr)
		return errno;

	// The readers report every open failure the same way, so find out why the file cannot be read first.
	int fd = open(absolute, O_RDONLY);
	if (fd < 0)
		return errno;
	struct stat st;
	int statResult = fstat(fd, &st);
	close(fd);
	if (statResult != 0)
		return errno;
	if (!S_ISREG(st.st_mode) && !S_ISBLK(st.st_mode))
		return EFTYPE;

	darling_dmg_image* image = new darling_dmg_image;
	image->path = absolute;
	try
	{
		openDiskImage(absolute, image->image);
	}
	catch (const io_error&)
	{
		delete image;
		return EIO;
	}
	catch (const std::exception&)
	{
		delete image;
		return EFTYPE;
	}

	image->volumeName = nameFromPath(image->path);
	*out = image;
	return 0;
}

void darling_dmg_close(darling_dmg_image* image)
{
	delete image;
}

const char* darling_dmg_volume_name(darling_dmg_image* image)
{
	return image->volumeName.c_str();
}

static bool isMountPoint(const char* path)
{
	struct statfs sfs;
	return statfs(path, &sfs) == 0 && strcmp(sfs.f_mntonname, path) == 0;
}

// Starts the helper and waits until it reports that the volume is mounted, or why it is not.
static int runMountHelper(const std::string& imagePath, const std::string& mountPoint)
{
	int ready[2];
	if (pipe(ready) != 0)
		return errno;
	fcntl(ready[0], F_SETFD, FD_CLOEXEC);

	std::string readyFd = std::to_string(ready[1]);
	const char* argv[] = { kMountHelper, imagePath.c_str(), mountPoint.c_str(), readyFd.c_str(), nullptr };

	// The helper must not hold on to the caller's standard streams: a caller reading our output through a pipe
	// would otherwise wait for as long as the volume stays mounted.
	posix_spawn_file_actions_t actions;
	posix_spawn_file_actions_init(&actions);
	posix_spawn_file_actions_addopen(&actions, 0, "/dev/null", O_RDONLY, 0);
	posix_spawn_file_actions_addopen(&actions, 1, "/dev/null", O_WRONLY, 0);
	posix_spawn_file_actions_addopen(&actions, 2, "/dev/null", O_WRONLY, 0);

	pid_t pid;
	int err = posix_spawn(&pid, kMountHelper, &actions, nullptr, const_cast<char* const*>(argv), environ);
	posix_spawn_file_actions_destroy(&actions);
	close(ready[1]);
	if (err != 0)
	{
		close(ready[0]);
		return err;
	}

	// The helper hands the file system over to a detached child of its own and exits, so this does not wait
	// for the volume to be unmounted.
	int status;
	while (waitpid(pid, &status, 0) < 0 && errno == EINTR)
		;

	char report[64];
	size_t length = 0;
	ssize_t got;
	while (length < sizeof(report) - 1 && ((got = read(ready[0], report + length, sizeof(report) - 1 - length)) > 0
		|| (got < 0 && errno == EINTR)))
	{
		if (got > 0)
			length += got;
	}
	close(ready[0]);
	report[length] = 0;

	if (length == 0 || report[0] < '0' || report[0] > '9')
		return EIO; // the helper died without saying why
	return atoi(report);
}

int darling_dmg_attach(darling_dmg_image* image, const struct darling_dmg_attach_options* options,
	char* mount_point, size_t mount_point_size)
{
	if (image == nullptr || mount_point == nullptr)
		return EINVAL;

	std::string mountPoint;
	if (options != nullptr && options->mount_point != nullptr)
		mountPoint = options->mount_point;
	else
	{
		mountPoint = (options != nullptr && options->mount_root != nullptr) ? options->mount_root : "/Volumes";
		while (mountPoint.size() > 1 && mountPoint.back() == '/')
			mountPoint.pop_back();
		if (mountPoint != "/")
			mountPoint += '/';
		mountPoint += image->volumeName;
	}

	if (mountPoint.size() + 1 > mount_point_size)
		return ENAMETOOLONG;

	bool created = mkdir(mountPoint.c_str(), 0777) == 0;
	if (!created)
	{
		if (errno != EEXIST)
			return errno;
		struct stat st;
		if (stat(mountPoint.c_str(), &st) != 0)
			return errno;
		if (!S_ISDIR(st.st_mode))
			return ENOTDIR;
		if (isMountPoint(mountPoint.c_str()))
			return EBUSY;
	}

	int err = runMountHelper(image->path, mountPoint);
	if (err != 0)
	{
		if (created)
			rmdir(mountPoint.c_str());
		return err;
	}

	strcpy(mount_point, mountPoint.c_str());
	return 0;
}

int darling_dmg_detach(const char* mount_point, int force)
{
	if (mount_point == nullptr)
		return EINVAL;

	char path[PATH_MAX];
	if (realpath(mount_point, path) == nullptr || !isMountPoint(path))
		return ENOENT;

	if (unmount(path, force ? MNT_FORCE : 0) != 0)
		return errno;

	rmdir(path);
	return 0;
}
