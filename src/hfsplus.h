#ifndef HFSPLUS_H
#define HFSPLUS_H
#include <stdint.h>

#pragma pack(1)

typedef uint16_t unichar;
typedef uint32_t HFSCatalogNodeID;

#define HFSP_SIGNATURE 0x482b
#define HFSX_SIGNATURE 0x4858

#define HFSPLUS_S_IFMT   0170000	/* type of file mask */
#define HFSPLUS_S_IFIFO  0010000	/* named pipe (fifo) */
#define HFSPLUS_S_IFCHR  0020000	/* character special */
#define HFSPLUS_S_IFDIR  0040000	/* directory */
#define HFSPLUS_S_IFBLK  0060000	/* block special */
#define HFSPLUS_S_IFREG  0100000	/* regular */
#define HFSPLUS_S_IFLNK  0120000	/* symbolic link */
#define HFSPLUS_S_IFSOCK 0140000	/* socket */
#define HFSPLUS_S_IFWHT  0160000	/* whiteout */

#define HFS_PERM_OFLAG_COMPRESSED 0x20

struct HFSString
{
	uint16_t length;
	unichar string[255];
};

struct HFSPlusBSDInfo
{
	uint32_t ownerID;
	uint32_t groupID;
	uint8_t adminFlags;
	uint8_t ownerFlags;
	uint16_t fileMode;
	union
	{
		uint32_t iNodeNum;
		uint32_t linkCount;
		uint32_t rawDevice;
	} special;
};

struct HFSPlusExtentDescriptor
{
	uint32_t startBlock;
	uint32_t blockCount;
};

struct HFSPlusForkData
{
	uint64_t logicalSize;
	uint32_t clumpSize;
	uint32_t totalBlocks;
	HFSPlusExtentDescriptor extents[8];
};

struct HFSPlusVolumeHeader
{
	uint16_t signature;
	uint16_t version;
	uint32_t attributes;
	uint32_t lastMountedVersion;
	uint32_t journalInfoBlock;
 
	uint32_t createDate;
	uint32_t modifyDate;
	uint32_t backupDate;
	uint32_t checkedDate;
 
	uint32_t fileCount;
	uint32_t folderCount;
 
	uint32_t blockSize;
	uint32_t totalBlocks;
	uint32_t freeBlocks;
 
	uint32_t nextAllocation;
	uint32_t rsrcClumpSize;
	uint32_t dataClumpSize;
	uint32_t nextCatalogID;
 
	uint32_t writeCount;
	uint64_t encodingsBitmap;
 
	uint32_t finderInfo[8];
 
	HFSPlusForkData allocationFile;
	HFSPlusForkData extentsFile;
	HFSPlusForkData catalogFile;
	HFSPlusForkData attributesFile;
	HFSPlusForkData startupFile;
};

enum class NodeKind : int8_t
{
	kBTLeafNode       = -1,
	kBTIndexNode      =  0,
	kBTHeaderNode     =  1,
	kBTMapNode        =  2
};

struct BTNodeDescriptor
{
	uint32_t fLink;
	uint32_t bLink;
	NodeKind kind;
	uint8_t height;
	uint16_t numRecords;
	uint16_t reserved;
};

enum class KeyCompareType : uint8_t
{
	kHFSCaseFolding = 0xCF,
	kHFSBinaryCompare = 0xBC
};

struct BTHeaderRec
{
	uint16_t treeDepth;
	uint32_t rootNode;
	uint32_t leafRecords;
	uint32_t firstLeafNode;
	uint32_t lastLeafNode;
	uint16_t nodeSize;
	uint16_t maxKeyLength;
	uint32_t totalNodes;
	uint32_t freeNodes;
	uint16_t reserved1;
	uint32_t clumpSize;	  // misaligned
	uint8_t btreeType;
	KeyCompareType keyCompareType;
	uint32_t attributes;	 // long aligned again
	uint32_t reserved3[16];
};

enum
{
	kHFSNullID = 0,
	kHFSRootParentID = 1,
	kHFSRootFolderID = 2,
	kHFSExtentsFileID = 3,
	kHFSCatalogFileID = 4,
	kHFSBadBlockFileID = 5,
	kHFSAllocationFileID = 6,
	kHFSStartupFileID = 7,
	kHFSAttributesFileID = 8,
	kHFSRepairCatalogFileID = 14,
	kHFSBogusExtentFileID = 15,
	kHFSFirstUserCatalogNodeID = 16
};

struct HFSPlusCatalogKey
{
	uint16_t keyLength;
	HFSCatalogNodeID parentID;
	HFSString nodeName;
};

enum class RecordType : uint16_t
{
	kHFSPlusFolderRecord        = 0x0001,
	kHFSPlusFileRecord          = 0x0002,
	kHFSPlusFolderThreadRecord  = 0x0003,
	kHFSPlusFileThreadRecord    = 0x0004
};

struct Point
{
	int16_t v, h;
};

struct Rect
{
	int16_t top, left, bottom, right;
};

struct FileInfo
{
	uint32_t fileType;
	uint32_t fileCreator;
	uint16_t finderFlags;
	Point location;
	uint16_t reservedField;
};

//struct ExtendedFileInfo
//{
//    int16_t reserved1[4];
//    uint16_t extendedFinderFlags;
//    int16_t reserved2;
//    int32_t putAwayFolderID;
//};
// looking in Apple Source, this is the modern definition of ExtendedFileInfo
struct ExtendedFileInfo
{
	uint32_t document_id;
	uint32_t date_added;
	uint16_t extended_flags;
	uint16_t reserved2;
	uint32_t write_gen_counter;
} __attribute__((aligned(2), packed));

struct FolderInfo
{
	Rect windowBounds;
	uint16_t finderFlags;
	Point location;
	uint16_t reservedField;
};

//struct ExtendedFolderInfo
//{
//    Point scrollPosition;
//    int32_t reserved1;
//    uint16_t extendedFinderFlags;
//    int16_t reserved2;
//    int32_t putAwayFolderID;
//};
// looking in Apple Source, this is the modern definition of ExtendedFolderInfo
struct ExtendedFolderInfo
{
	uint32_t document_id;
	uint32_t date_added;
	uint16_t extended_flags;
	uint16_t reserved3;
	uint32_t write_gen_counter;
} __attribute__((aligned(2), packed));

struct HFSPlusCatalogFolder
{
	RecordType recordType;
	uint16_t flags;
	uint32_t valence;
	HFSCatalogNodeID folderID;
	uint32_t createDate;
	uint32_t contentModDate;
	uint32_t attributeModDate;
	uint32_t accessDate;
	uint32_t backupDate;
	HFSPlusBSDInfo permissions;
	FolderInfo userInfo;
	ExtendedFolderInfo finderInfo;
	uint32_t textEncoding;
	uint32_t reserved;
};

struct HFSPlusCatalogFile
{
	RecordType recordType;
	uint16_t flags;
	uint32_t reserved1;
	HFSCatalogNodeID fileID;
	uint32_t createDate;
	uint32_t contentModDate;
	uint32_t attributeModDate;
	uint32_t accessDate;
	uint32_t backupDate;
	HFSPlusBSDInfo permissions;
	FileInfo userInfo;
	ExtendedFileInfo finderInfo;
	uint32_t textEncoding;
	uint32_t reserved2;
 
	HFSPlusForkData dataFork;
	HFSPlusForkData resourceFork;
};

struct HFSPlusCatalogFileOrFolder
{
	union
	{
		HFSPlusCatalogFile file;
		HFSPlusCatalogFolder folder;
	};
};

struct HFSPlusCatalogThread
{
	RecordType recordType;
	int16_t reserved;
	HFSCatalogNodeID parentID;
	HFSString nodeName;
};

struct HFSPlusExtentKey
{
	uint16_t keyLength;
	uint8_t forkType;
	uint8_t pad;
	HFSCatalogNodeID fileID;
	uint32_t startBlock;
};

struct HFSPlusAttributeKey
{
	uint16_t keyLength;
	uint16_t padding;
	HFSCatalogNodeID fileID;
	uint32_t startBlock; // first allocation block number for extents
	uint16_t attrNameLength;
	uint16_t attrName[127];
};

enum
{
	kHFSPlusAttrInlineData  = 0x10,
	kHFSPlusAttrForkData    = 0x20,
	kHFSPlusAttrExtents     = 0x30
};

struct HFSPlusAttributeDataInline
{
	uint32_t recordType; // kHFSPlusAttrInlineData
	uint64_t reserved;
	uint32_t attrSize;
	uint8_t attrData[];
};

#pragma pack()

// File type and creator for hard link
enum {
	kHardLinkFileType = 0x686C6E6B,  /* 'hlnk' */
	kHFSPlusCreator   = 0x6866732B   /* 'hfs+' */
};

#endif

