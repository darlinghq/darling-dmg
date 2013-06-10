#include <iostream>
#include "FileReader.h"
#include "AppleDisk.h"
#include "HFSVolume.h"
#include "HFSCatalogBTree.h"

int main(int argc, char** argv)
{
	FileReader* reader = new FileReader(argv[1]);
	AppleDisk* adisk = new AppleDisk(reader);
	Reader* partReader = nullptr;
	HFSVolume* volume;
	HFSCatalogBTree* rootTree;
	
	auto parts = adisk->partitions();
	
	for (int i = 0; i < parts.size(); i++)
	{
		if (parts[i].type == "Apple_HFS")
		{
			partReader = adisk->readerForPartition(i);
			break;
		}
	}
	
	if (!partReader)
	{
		std::cerr << "HFS partition not found\n";
		return 1;
	}
	
	volume = new HFSVolume(partReader);
	
	uint64_t total, free;
	volume->usage(total, free);
	std::cout << "Disk size: " << total << " bytes\n";
	std::cout << "Free size: " << free << " bytes\n";
	
	rootTree = volume->rootCatalogTree();
	
	delete rootTree;
	delete partReader;
	delete adisk;
	delete reader;
	
	return 0;
}
