#ifndef RSRC_H
#define RSRC_H
#include <stdint.h>

// Resource fork structure declarations
// Big Endian

#pragma pack(1)


struct HFSResourceForkHeader
{
	uint32_t dataOffset;
	uint32_t mapOffset; // offset to HFSResourceMapHeader
	uint32_t dataLength;
	uint32_t mapLength;
};

struct HFSResourceHeader
{
	uint32_t length;
	uint8_t data[];
};

struct HFSResourceMapHeader
{
	uint32_t dataOffset;
	uint32_t mapOffset;
	uint32_t dataLength;
	uint32_t mapLength;
	uint32_t reserved2;
	uint16_t reserved3;
	uint16_t attributes;
	uint16_t listOffset; // offset to HFSResourceList from the start of HFSResourceMapHeader
};

struct HFSResourceListItem
{
	uint32_t type; // fourcc
	uint16_t count; // contains count - 1
	uint16_t offset; // offset to HFSResourcePointer from this list item
};

struct HFSResourceList
{
	uint16_t count; // contains count - 1
	HFSResourceListItem items[];
};

struct HFSResourcePointer
{
	uint16_t resourceId; // 0xffff for cmpfs
	uint16_t offsetName;
	uint32_t dataOffset; // offset to HFSResourceHeader from added to HFSResourceForkHeader::dataOffset
	uint16_t reserved;
};

#pragma pack()

#endif
