#include "GPTDisk.h"
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <cstring>
#include "gpt.h"
#include "SubReader.h"
#include "exceptions.h"

GPTDisk::GPTDisk(std::shared_ptr<Reader> reader)
: m_reader(reader)
{
	loadPartitions(nullptr);
}

GPTDisk::GPTDisk(std::shared_ptr<Reader> protectiveMBR, std::shared_ptr<Reader> partitionTable)
	: m_reader(nullptr)
{
	if (!isGPTDisk(protectiveMBR))
		throw io_error("Not a GPT disk!");
	loadPartitions(partitionTable);
}

bool GPTDisk::isGPTDisk(std::shared_ptr<Reader> reader)
{
	ProtectiveMBR mbr;
	if (reader->read(&mbr, sizeof(mbr), 0) != sizeof(mbr))
		return false;
	
	if (mbr.signature != MBR_SIGNATURE)
		return false;
	if (mbr.partitions[0].type != MPT_GPT_FAKE_TYPE)
		return false;

	return true;
}

std::string GPTDisk::makeGUID(const GPT_GUID& guid)
{
	std::stringstream ss;

	ss << std::hex << std::uppercase;
	ss << std::setw(8) << std::setfill('0') << guid.data1;

	ss << '-';
	ss << std::setw(4) << std::setfill('0') << guid.data2 << '-' << guid.data3 << '-';

	for (int i = 0; i < 8; i++)
	{
		ss << std::setw(2) << std::setfill('0') << uint32_t(guid.data4[i]);
		if (i == 1)
			ss << '-';
	}

	return ss.str();
}

void GPTDisk::loadPartitions(std::shared_ptr<Reader> table)
{
	uint64_t offset;
	int32_t rd;
	GPTPartition part[128];

	if (table)
		offset = 0;
	else
	{
		offset = 2*512;
		table = m_reader;
	}

	rd = table->read(part, sizeof(part), offset);

	for (int i = 0; i < rd / sizeof(GPTPartition); i++)
	{
		Partition p;
		char name[37];
		std::string typeGUID = makeGUID(part[i].typeGUID);

		memset(name, 0, sizeof(name));
		for (int j = 0; j < 36; j++)
			name[j] = char(part[i].name[j]);

		p.name = name;
		p.size = (part[i].lastLBA - part[i].firstLBA + 1) * 512;
		p.offset = part[i].firstLBA * 512;

		if (typeGUID == GUID_EMPTY)
			p.type = "Apple_Free";
		else if (typeGUID == GUID_HFSPLUS)
			p.type = "Apple_HFS";
		else
			p.type = typeGUID;

		m_partitions.push_back(p);
	}
}

std::shared_ptr<Reader> GPTDisk::readerForPartition(int index)
{
	const Partition& part = m_partitions.at(index);
	return std::shared_ptr<Reader>(new SubReader(m_reader, part.offset, part.size));
}

