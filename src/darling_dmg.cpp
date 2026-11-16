// libdarling-dmg: see include/darling-dmg/darling_dmg.h.
#include <darling-dmg/darling_dmg.h>

#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <spawn.h>
#include <sys/mount.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <unistd.h>

#include <strings.h>
#include <vector>

#include "DiskImage.h"
#include "DMGDisk.h"
#include "LicenseAgreement.h"
#include "MountPointName.h"
#include "exceptions.h"

extern "C" char** environ;

// Runs the FUSE file system for one attached image; see main-mount.cpp for its arguments and readiness report.
static const char kMountHelper[] = "/usr/libexec/darling-dmg-mount";

struct darling_dmg_image
{
	std::string path;
	std::string volumeName;
	std::string partitionScheme;
	DiskImage image;
	std::vector<LicenseAgreement> agreements;
	std::vector<darling_dmg_license> licenses; // views of agreements for the C API
	std::string attachMessages;                // what the mount helper said when the last attach failed
};

static void loadLicenses(darling_dmg_image* image)
{
	DMGDisk* dmg = dynamic_cast<DMGDisk*>(image->image.partitions.get());
	if (dmg == nullptr)
		return;

	image->agreements = loadLicenseAgreements([dmg](const std::string& type, int id, std::vector<uint8_t>& data) {
		return dmg->resource(type, id, data);
	});

	for (const LicenseAgreement& agreement : image->agreements)
	{
		darling_dmg_license license = {};
		license.language = agreement.language.c_str();
		license.is_default = agreement.isDefault;
		license.is_rtf = agreement.isRTF;
		license.text = agreement.text.c_str();
		if (agreement.hasStrings)
		{
			license.language_name = agreement.languageName.c_str();
			license.agree = agreement.agree.c_str();
			license.disagree = agreement.disagree.c_str();
			license.print = agreement.print.c_str();
			license.save = agreement.save.c_str();
			license.message = agreement.message.c_str();
		}
		image->licenses.push_back(license);
	}
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
		image->volumeName = image->image.volume->volumeName();
		image->partitionScheme = partitionScheme(image->image);
		loadLicenses(image);
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

	*out = image;
	return 0;
}

size_t darling_dmg_license_count(darling_dmg_image* image)
{
	return image->licenses.size();
}

const struct darling_dmg_license* darling_dmg_license_at(darling_dmg_image* image, size_t index)
{
	return index < image->licenses.size() ? &image->licenses[index] : nullptr;
}

// A BCP 47 tag with '_' read as '-', and its language subtag alone.
static std::string normalizedTag(const char* tag)
{
	std::string result = tag;
	for (char& c : result)
	{
		if (c == '_')
			c = '-';
	}
	return result;
}

static std::string languageSubtag(const std::string& tag)
{
	return tag.substr(0, tag.find('-'));
}

const struct darling_dmg_license* darling_dmg_license_for_languages(darling_dmg_image* image,
	const char* const* preferred, size_t count)
{
	for (size_t i = 0; preferred != nullptr && i < count; i++)
	{
		if (preferred[i] == nullptr)
			continue;
		std::string wanted = normalizedTag(preferred[i]);

		for (const darling_dmg_license& license : image->licenses)
		{
			if (strcasecmp(license.language, wanted.c_str()) == 0)
				return &license;
		}
		for (const darling_dmg_license& license : image->licenses)
		{
			if (strcasecmp(languageSubtag(license.language).c_str(), languageSubtag(wanted).c_str()) == 0)
				return &license;
		}
	}

	for (const darling_dmg_license& license : image->licenses)
	{
		if (license.is_default)
			return &license;
	}
	return nullptr;
}

void darling_dmg_close(darling_dmg_image* image)
{
	delete image;
}

const char* darling_dmg_attach_messages(darling_dmg_image* image)
{
	return image->attachMessages.empty() ? nullptr : image->attachMessages.c_str();
}

const char* darling_dmg_volume_name(darling_dmg_image* image)
{
	return image->volumeName.c_str();
}

const char* darling_dmg_partition_scheme(darling_dmg_image* image)
{
	return image->partitionScheme.c_str();
}

static bool isMountPoint(const char* path)
{
	struct statfs sfs;
	return statfs(path, &sfs) == 0 && strcmp(sfs.f_mntonname, path) == 0;
}

// Starts the helper and waits until it reports that the volume is mounted, or why it is not. The helper's stderr
// is the report pipe: whatever it (FUSE, fusermount) says before the final "<errno>" line is returned in messages.
static int runMountHelper(const std::string& imagePath, const std::string& mountPoint, std::string& messages)
{
	int ready[2];
	if (pipe(ready) != 0)
		return errno;
	// Neither end may leak into processes other threads of the caller start; the helper gets its own copy as stderr.
	fcntl(ready[0], F_SETFD, FD_CLOEXEC);
	fcntl(ready[1], F_SETFD, FD_CLOEXEC);

	const char* argv[] = { kMountHelper, imagePath.c_str(), mountPoint.c_str(), "2", nullptr };

	// The helper must not hold on to the caller's standard streams: a caller reading our output through a pipe
	// would otherwise wait for as long as the volume stays mounted.
	posix_spawn_file_actions_t actions;
	posix_spawn_file_actions_init(&actions);
	posix_spawn_file_actions_addopen(&actions, 0, "/dev/null", O_RDONLY, 0);
	posix_spawn_file_actions_addopen(&actions, 1, "/dev/null", O_WRONLY, 0);
	posix_spawn_file_actions_adddup2(&actions, ready[1], 2);

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

	// The child stops writing to the pipe once it has reported, so this reads until it is done.
	std::string report;
	char buffer[512];
	for (;;)
	{
		ssize_t got = read(ready[0], buffer, sizeof(buffer));
		if (got > 0)
			report.append(buffer, got);
		else if (got == 0 || errno != EINTR)
			break;
	}
	close(ready[0]);

	while (!report.empty() && report.back() == '\n')
		report.pop_back();
	size_t lastLine = report.rfind('\n');
	std::string statusLine = report.substr(lastLine == std::string::npos ? 0 : lastLine + 1);
	std::string before = lastLine == std::string::npos ? std::string() : report.substr(0, lastLine);

	if (statusLine.empty() || statusLine.find_first_not_of("0123456789") != std::string::npos)
	{
		messages = report; // the helper died without saying how it ended
		return EIO;
	}
	int result = atoi(statusLine.c_str());
	if (result != 0)
		messages = before;
	return result;
}

// Keeps other attaches from choosing a mount point name in the same directory until this one's volume is mounted
// (see makeMountPoint). Advisory; without it (lock unavailable) attaching still works.
class MountRootLock
{
public:
	explicit MountRootLock(const std::string& mountRoot)
	{
		m_fd = open(mountRoot.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
		if (m_fd >= 0)
		{
			while (flock(m_fd, LOCK_EX) != 0 && errno == EINTR)
				;
		}
	}
	~MountRootLock()
	{
		if (m_fd >= 0)
			close(m_fd);
	}
private:
	int m_fd;
};

int darling_dmg_attach(darling_dmg_image* image, const struct darling_dmg_attach_options* options,
	char* mount_point, size_t mount_point_size)
{
	if (image == nullptr || mount_point == nullptr)
		return EINVAL;

	image->attachMessages.clear();
	std::unique_ptr<MountRootLock> lock;
	std::string mountPoint;
	bool created;
	if (options != nullptr && options->mount_point != nullptr)
	{
		mountPoint = options->mount_point;
		created = mkdir(mountPoint.c_str(), 0777) == 0;
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
	}
	else
	{
		std::string mountRoot = (options != nullptr && options->mount_root != nullptr) ? options->mount_root : "/Volumes";
		while (mountRoot.size() > 1 && mountRoot.back() == '/')
			mountRoot.pop_back();
		lock.reset(new MountRootLock(mountRoot));
		int err = makeMountPoint(mountRoot, mountPointName(image->volumeName), mountPoint);
		if (err != 0)
			return err;
		created = true;
	}

	if (mountPoint.size() + 1 > mount_point_size)
	{
		if (created)
			rmdir(mountPoint.c_str());
		return ENAMETOOLONG;
	}

	int err = runMountHelper(image->path, mountPoint, image->attachMessages);
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
