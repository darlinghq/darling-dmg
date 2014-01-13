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

#pragma pack()

#endif
