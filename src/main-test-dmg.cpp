#include <iostream>
#include "FileReader.h"
#include "DMGDisk.h"

int main(int argc, char** argv)
{
	Reader* fileReader = new FileReader(argv[1]);
	DMGDisk* dmg = new DMGDisk(fileReader);

	auto partitions = dmg->partitions();
	for (auto part : partitions)
	{
		std::cout << part.name << ' ' << part.type << ", " << part.size << " bytes\n";
	}

	delete dmg;
	delete fileReader;
}

