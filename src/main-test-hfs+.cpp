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
	
	const char* name = "/Squiggle.app/Contents";
	uint64_t total, free;
	Reader* fileReader;
	char* buf;

	volume->usage(total, free);
	std::cout << "Disk size: " << total << " bytes\n";
	std::cout << "Free size: " << free << " bytes\n";
	
	rootTree = volume->rootCatalogTree();

	std::map<std::string, HFSPlusCatalogFileOrFolder> root;
	rootTree->listDirectory(name, root);
	
	std::cout << name << " contains " << root.size() << " elems\n";
	for (auto it = root.begin(); it != root.end(); it++)
	{
		RecordType recType = it->second.file.recordType;
		std::cout << "* " << it->first << ' ';
		
		if (recType == RecordType::kHFSPlusFolderRecord)
			std::cout << "<DIR>\n";
		else
			std::cout << it->second.file.dataFork.logicalSize << std::endl;
	}

	rootTree->openFile("/Squiggle.app/Contents/Info.plist", &fileReader);
	buf = new char[874];

	fileReader->read(buf, 873, 0);
	buf[873] = 0;

	std::cout << buf;

	delete [] buf;
	delete fileReader;
	
	delete rootTree;
	delete partReader;
	delete adisk;
	delete reader;
	
	return 0;
}
