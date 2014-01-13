#include "GPTDisk.h"
#include <stdexcept>
#include "gpt.h"

GPTDisk::GPTDisk(Reader* reader)
: m_reader(reader)
{
}

bool GPTDisk::isGPTDisk(Reader* reader)
{
	ProtectiveMBR mbr;
	reader->read(&mbr, sizeof(mbr), 0);
	
	if (mbr.signature != MBR_SIGNATURE)
		return false;
	if (mbr.partitions[0].type != MPT_GPT_FAKE_TYPE)
		return false;
	
	throw std::runtime_error("GPT based disk images not supported yet. See darling-dmg issue #1");
}
