#ifndef DISKIMAGE_H
#define DISKIMAGE_H
#include <memory>
#include "Reader.h"
#include "PartitionedDisk.h"
#include "HFSVolume.h"

// An opened disk image: a UDIF image, a GPT or Apple partition map disk, or a bare HFS+ volume,
// and the first HFS+/HFSX volume in it.
struct DiskImage
{
	std::shared_ptr<Reader> file;
	std::unique_ptr<PartitionedDisk> partitions;
	std::shared_ptr<HFSVolume> volume;
};

// Throws file_not_found_error if the file cannot be opened, function_not_implemented_error if it is not a disk
// image this code reads or has no HFS+ volume, and the reader exceptions for damaged images.
void openDiskImage(const char* path, DiskImage& image);

// The partition scheme of the image's whole disk, named as macOS's hdiutil names it: "GUID_partition_scheme",
// "Apple_partition_scheme", or "" if the image has no partition map (its volume is the whole disk).
std::string partitionScheme(const DiskImage& image);

#endif
