#ifndef DMG_H
#define DMG_H

#pragma pack(1)
#define UDIF_SIGNATURE 0x6B6F6C79

enum
{
	kUDIFFlagsFlattened = 1
};

enum
{
	kUDIFDeviceImageType = 1,
	kUDIFPartitionImageType = 2
};

struct UDIFChecksum
{
	uint32_t type;
	uint32_t size;
	uint32_t data[0x20];
};

struct UDIFID
{
	uint32_t data1; /* smallest */
	uint32_t data2;
	uint32_t data3;
	uint32_t data4; /* largest */
};

struct UDIFResourceFile
{
	uint32_t fUDIFSignature;
	uint32_t fUDIFVersion;
	uint32_t fUDIFHeaderSize;
	uint32_t fUDIFFlags;
	
	uint64_t fUDIFRunningDataForkOffset;
	uint64_t fUDIFDataForkOffset;
	uint64_t fUDIFDataForkLength;
	uint64_t fUDIFRsrcForkOffset;
	uint64_t fUDIFRsrcForkLength;
	
	uint32_t fUDIFSegmentNumber;
	uint32_t fUDIFSegmentCount;
	UDIFID fUDIFSegmentID;  /* a 128-bit number like a GUID, but does not seem to be a OSF GUID, since it doesn't have the proper versioning byte */
	
	UDIFChecksum fUDIFDataForkChecksum;
	
	uint64_t fUDIFXMLOffset;
	uint64_t fUDIFXMLLength;
	
	uint8_t reserved1[0x78]; /* this is actually the perfect amount of space to store every thing in this struct until the checksum */
	
	UDIFChecksum fUDIFMasterChecksum;
	
	uint32_t fUDIFImageVariant;
	uint64_t fUDIFSectorCount;
	
	uint32_t reserved2;
	uint32_t reserved3;
	uint32_t reserved4;
	
};

struct BLKXRun
{
	uint32_t type;
	uint32_t reserved;
	uint64_t sectorStart;
	uint64_t sectorCount;
	uint64_t compOffset;
	uint64_t compLength;
};

enum class RunType : uint32_t
{
	ZeroFill = 0,
	Raw = 1,
	Unknown = 2,
	ADC = 0x80000004,
	Zlib = 0x80000005,
	Bzip2 = 0x80000006,
	LZFSE = 0x80000007,
	Comment = 0x7ffffffe,
	Terminator = 0xffffffff
};

struct SizeResource
{
	uint16_t version; /* set to 5 */
	uint32_t isHFS; /* first dword of v53(ImageInfoRec): Set to 1 if it's a HFS or HFS+ partition -- duh. */
	uint32_t unknown1; /* second dword of v53: seems to be garbage if it's HFS+, stuff related to HFS embedded if it's that*/
	uint8_t dataLen; /* length of data that proceeds, comes right before the data in ImageInfoRec. Always set to 0 for HFS, HFS+ */
	uint8_t data[255]; /* other data from v53, dataLen + 1 bytes, the rest NULL filled... a string? Not set for HFS, HFS+ */
	uint32_t unknown2; /* 8 bytes before volumeModified in v53, seems to be always set to 0 for HFS, HFS+  */
	uint32_t unknown3; /* 4 bytes before volumeModified in v53, seems to be always set to 0 for HFS, HFS+ */
	uint32_t volumeModified; /* offset 272 in v53 */
	uint32_t unknown4; /* always seems to be 0 for UDIF */
	uint16_t volumeSignature; /* HX in our case */
	uint16_t sizePresent; /* always set to 1 */
};

struct CSumResource
{
	uint16_t version; /* set to 1 */
	uint32_t type; /* set to 0x2 for MKBlockChecksum */
	uint32_t checksum;
};

#define DDM_DESCRIPTOR 0xFFFFFFFF
#define ENTIRE_DEVICE_DESCRIPTOR 0xFFFFFFFE

struct BLKXTable
{
	uint32_t fUDIFBlocksSignature;
	uint32_t infoVersion;
	uint64_t firstSectorNumber;
	uint64_t sectorCount;
	
	uint64_t dataStart;
	uint32_t decompressBufferRequested;
	uint32_t blocksDescriptor;
	
	uint32_t reserved1;
	uint32_t reserved2;
	uint32_t reserved3;
	uint32_t reserved4;
	uint32_t reserved5;
	uint32_t reserved6;
	
	UDIFChecksum checksum;
	
	uint32_t blocksRunCount;
	BLKXRun runs[0];
};

#pragma pack()

#endif

