#include "DMGExtractor.h"

#include "AppleDisk.h"
#include "DMGDisk.h"
#include "FileReader.h"
#include "GPTDisk.h"
#include "HFSHighLevelVolume.h"
#include "HFSVolume.h"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <memory>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

namespace {

void fail(const std::string& action, const std::string& path) {
    throw std::runtime_error(action + " " + path + ": " + std::strerror(errno));
}

void requireEmptyDirectory(const std::string& path) {
    struct stat st;
    if (lstat(path.c_str(), &st) != 0) fail("stat", path);
    if (!S_ISDIR(st.st_mode)) throw std::runtime_error("destination is not a directory: " + path);
    DIR* dir = opendir(path.c_str());
    if (!dir) fail("open directory", path);
    bool empty = true;
    while (const auto* entry = readdir(dir)) {
        if (std::strcmp(entry->d_name, ".") != 0 && std::strcmp(entry->d_name, "..") != 0) {
            empty = false;
            break;
        }
    }
    closedir(dir);
    if (!empty) throw std::runtime_error("destination is not empty: " + path);
}

void copyFile(HFSHighLevelVolume& volume, const std::string& source,
              const std::string& destination, const struct stat& st) {
    auto reader = volume.openFile(source);
    if (st.st_size < 0) throw std::runtime_error("negative file size: " + source);

    if (S_ISLNK(st.st_mode)) {
        std::string target(static_cast<size_t>(st.st_size), '\0');
        if (!target.empty() && reader->read(&target[0], target.size(), 0) !=
                               static_cast<int32_t>(target.size()))
            throw std::runtime_error("short symbolic link: " + source);
        if (target.find('\0') != std::string::npos)
            throw std::runtime_error("invalid symbolic link: " + source);
        if (symlink(target.c_str(), destination.c_str()) != 0) fail("create symbolic link", destination);
        return;
    }

    int fd = open(destination.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0600);
    if (fd < 0) fail("create file", destination);
    char buffer[65536];
    uint64_t offset = 0;
    try {
        while (offset < static_cast<uint64_t>(st.st_size)) {
            size_t wanted = std::min(sizeof(buffer), static_cast<size_t>(st.st_size - offset));
            int32_t count = reader->read(buffer, wanted, offset);
            if (count <= 0) throw std::runtime_error("short read: " + source);
            size_t written = 0;
            while (written < static_cast<size_t>(count)) {
                ssize_t part = write(fd, buffer + written, count - written);
                if (part < 0 && errno == EINTR) continue;
                if (part <= 0) fail("write file", destination);
                written += part;
            }
            offset += count;
        }
        if (fchmod(fd, st.st_mode & 0777) != 0) fail("set permissions", destination);
    } catch (...) {
        close(fd);
        unlink(destination.c_str());
        throw;
    }
    if (close(fd) != 0) fail("close file", destination);
}

void copyDirectory(HFSHighLevelVolume& volume, const std::string& source,
                   const std::string& destination) {
    for (const auto& entry : volume.listDirectory(source)) {
        const std::string& name = entry.first;
        if (name.empty() || name == "." || name == ".." || name.find('/') != std::string::npos)
            throw std::runtime_error("invalid image entry name: " + name);
        std::string childSource = source == "/" ? "/" + name : source + "/" + name;
        std::string childDestination = destination + "/" + name;
        const struct stat& st = entry.second;
        if (S_ISDIR(st.st_mode)) {
            if (mkdir(childDestination.c_str(), 0700) != 0) fail("create directory", childDestination);
            copyDirectory(volume, childSource, childDestination);
            if (chmod(childDestination.c_str(), st.st_mode & 0777) != 0)
                fail("set permissions", childDestination);
        } else if (S_ISREG(st.st_mode) || S_ISLNK(st.st_mode)) {
            copyFile(volume, childSource, childDestination, st);
        } else {
            throw std::runtime_error("unsupported image entry: " + childSource);
        }
    }
}

} // namespace

void extractDMG(const std::string& imagePath, const std::string& destination) {
    requireEmptyDirectory(destination);
    auto reader = std::make_shared<FileReader>(imagePath.c_str());
    std::unique_ptr<PartitionedDisk> disk;
    std::shared_ptr<HFSVolume> hfs;
    if (DMGDisk::isDMG(reader)) disk.reset(new DMGDisk(reader));
    else if (GPTDisk::isGPTDisk(reader)) disk.reset(new GPTDisk(reader));
    else if (AppleDisk::isAppleDisk(reader)) disk.reset(new AppleDisk(reader));
    else if (HFSVolume::isHFSPlus(reader)) hfs.reset(new HFSVolume(reader));
    else throw std::runtime_error("unsupported disk image format: " + imagePath);

    if (disk) {
        const auto& partitions = disk->partitions();
        for (size_t i = 0; i < partitions.size(); ++i) {
            if (partitions[i].type == "Apple_HFS" || partitions[i].type == "Apple_HFSX") {
                hfs.reset(new HFSVolume(disk->readerForPartition(i)));
                break;
            }
        }
    }
    if (!hfs) throw std::runtime_error("no HFS+/HFSX partition: " + imagePath);
    HFSHighLevelVolume volume(hfs);
    copyDirectory(volume, "/", destination);
}
