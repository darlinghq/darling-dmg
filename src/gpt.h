#ifndef GPT_H
#define GPT_H
#include <stdint.h>
#include "be.h"

#pragma pack(1)

#define MPT_GPT_FAKE_TYPE 0xEE

struct MBRPartition
{
	uint8_t status;
	uint8_t chsFirst[3];
	uint8_t type;
	uint8_t chsLast[3];
	uint32_t lbaFirst;
	uint32_t numSectors;
};

#define MBR_SIGNATURE be<uint16_t>(0x55AA)

struct ProtectiveMBR
{
	uint8_t unused[446];
	MBRPartition partitions[4];
	uint16_t signature;
};

#define GPT_SIGNATURE "EFI PART"

struct GPTHeader
{
	char signature[8];
	// TODO
};

struct GUID
{
	uint32_t data1;
	uint16_t data2, data3;
	uint8_t data4[8];
};

struct GPTPartition
{
	GUID typeGUID;
	GUID partitionGUID;
	uint64_t firstLBA, lastLBA;
	uint64_t flags;
	uint16_t name[36];
};

#define GUID_EMPTY "00000000-0000-0000-0000-000000000000"
#define GUID_HFSPLUS "48465300-0000-11AA-AA11-00306543ECAC"

#pragma pack()

#endif
