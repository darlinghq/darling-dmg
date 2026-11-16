#include "DiskImage.h"
#include "AppleDisk.h"
#include "GPTDisk.h"
#include "DMGDisk.h"
#include "FileReader.h"
#include "exceptions.h"

void openDiskImage(const char* path, DiskImage& image)
{
	image.file.reset(new FileReader(path));

	if (DMGDisk::isDMG(image.file))
		image.partitions.reset(new DMGDisk(image.file));
	else if (GPTDisk::isGPTDisk(image.file))
		image.partitions.reset(new GPTDisk(image.file));
	else if (AppleDisk::isAppleDisk(image.file))
		image.partitions.reset(new AppleDisk(image.file));
	else if (HFSVolume::isHFSPlus(image.file))
	{
		image.volume.reset(new HFSVolume(image.file));
		return;
	}
	else
		throw function_not_implemented_error("Unsupported file format");

	const std::vector<PartitionedDisk::Partition>& parts = image.partitions->partitions();
	for (size_t i = 0; i < parts.size(); i++)
	{
		if (parts[i].type == "Apple_HFS" || parts[i].type == "Apple_HFSX")
		{
			image.volume.reset(new HFSVolume(image.partitions->readerForPartition(i)));
			return;
		}
	}

	throw function_not_implemented_error("No suitable partition found in file");
}

std::string partitionScheme(const DiskImage& image)
{
	if (!image.partitions)
		return "";
	if (dynamic_cast<const GPTDisk*>(image.partitions.get()) != nullptr)
		return "GUID_partition_scheme";
	if (dynamic_cast<const AppleDisk*>(image.partitions.get()) != nullptr)
		return "Apple_partition_scheme";

	// a UDIF image lists the parts of its map in its own partition table
	for (const PartitionedDisk::Partition& part : image.partitions->partitions())
	{
		if (part.type == "Apple_partition_map")
			return "Apple_partition_scheme";
		if (part.type == "Primary GPT Header")
			return "GUID_partition_scheme";
	}
	return "";
}
