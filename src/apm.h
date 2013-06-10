#ifndef APM_H
#define APM_H
#include <stdint.h>

#pragma pack(1)

static const uint16_t BLOCK0_SIGNATURE = 0x4552;
static const uint16_t DPME_SIGNATURE = 0x504D;

struct DDMap
{
    uint32_t  ddBlock;
    uint16_t  ddSize;
    uint16_t  ddType;
};

struct DPME
{
    uint16_t  dpme_signature;
    uint16_t  dpme_reserved_1;
    uint32_t  dpme_map_entries;
    uint32_t  dpme_pblock_start;
    uint32_t  dpme_pblocks;
    char      dpme_name[32];
    char      dpme_type[32];
    uint32_t  dpme_lblock_start;
    uint32_t  dpme_lblocks;
    uint32_t  dpme_flags;
    uint32_t  dpme_boot_block;
    uint32_t  dpme_boot_bytes;
    uint32_t  dpme_load_addr;
    uint32_t  dpme_load_addr_2;
    uint32_t  dpme_goto_addr;
    uint32_t  dpme_goto_addr_2;
    uint32_t  dpme_checksum;
    uint8_t   dpme_process_id[16];
    uint32_t  dpme_reserved_2[32];
    uint32_t  dpme_reserved_3[62];
};

struct Block0
{
    uint16_t  sbSig;
    uint16_t  sbBlkSize;
    uint32_t  sbBlkCount;
    uint16_t  sbDevType;
    uint16_t  sbDevId;
    uint32_t  sbDrvrData;
    uint16_t  sbDrvrCount;
    DDMap     sbDrvrMap[8];
    uint8_t   sbReserved[430];
};

#pragma pack()

#endif
