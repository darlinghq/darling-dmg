#ifndef HFS_H
#define HFS_H
#include <stdint.h>

#define HFS_SIGNATURE 0x4244

#pragma pack(1)

struct HFSExtentDescriptor
{
	uint16_t startBlock;
	uint16_t blockCount;
};
typedef HFSExtentDescriptor HFSExtentRecord[3];

struct HFSMasterDirectoryBlock
{
	uint16_t drSigWord;
	uint32_t drCrDate;
	uint32_t drLsMod;
	uint16_t drAtrb;
	uint16_t drNmFls;
	uint16_t drVBMSt;
	uint16_t drAllocPtr;
	uint16_t drNmAlBlks;
	uint32_t drAlBlkSiz;
	uint32_t drClpSiz;
	uint16_t drAlBlSt;
	uint32_t drNxtCNID;
	uint16_t drFreeBks;
	uint8_t  drVN[28];
	uint32_t drVolBkUp;
	uint16_t drVSeqNum;
	uint32_t drWrCnt;
	uint32_t drXTClpSiz;
	uint32_t drCTClpSiz;
	uint16_t drNmRtDirs;
	uint32_t drFilCnt;
	uint32_t drDirCnt;
	uint32_t drFndrInfo[8];
	uint16_t drEmbedSigWord;
	HFSExtentDescriptor	drEmbedExtent;
	uint32_t drXTFlSize;
	HFSExtentRecord	drXTExtRec;
	uint32_t drCTFlSize;
	HFSExtentRecord	drCTExtRec;
};

#pragma pack()

#endif

