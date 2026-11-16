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
