#ifndef DECMPFS_H
#define DECMPFS_H
#include <stdint.h>

#define DECMPFS_MAGIC 0x636d7066 /* cmpf */
#define DECMPFS_ID 0xffff

#define DECMPFS_XATTR_NAME "com.apple.decmpfs"

enum class DecmpfsCompressionType : uint32_t
{
	UncompressedInline = 1, // inline = after the header in xattr
	CompressedInline = 3,
	CompressedResourceFork = 4
};

#pragma pack(1)
struct decmpfs_disk_header
{
	uint32_t compression_magic;
	uint32_t compression_type;
	uint64_t uncompressed_size;
	unsigned char attr_bytes[0];
};
#pragma pack()

#endif
