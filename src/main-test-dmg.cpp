#include <iostream>
#include "FileReader.h"
#include "DMGDisk.h"

void readPartition(DMGDisk* dmg, int index);

int main(int argc, char** argv)
{
	Reader* fileReader = new FileReader(argv[1]);
	DMGDisk* dmg = new DMGDisk(fileReader);

	auto partitions = dmg->partitions();
	for (int i = 0; i < partitions.size(); i++)
	{
		auto& part = partitions[i];

		std::cout << part.name << ' ' << part.type << ", " << part.size << " bytes\n";
		if (part.type == "Apple_HFS")
		{
			readPartition(dmg, i);
		}
	}

	delete dmg;
	delete fileReader;
}

void readPartition(DMGDisk* dmg, int index)
{
	char buf[4096];
	Reader* reader = dmg->readerForPartition(index);

	reader->read(buf, sizeof(buf), 0);

	delete reader;
}

