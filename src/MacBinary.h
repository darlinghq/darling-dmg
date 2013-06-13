#ifndef MACBINARY_H
#define MACBINARY_H
#include <stdint.h>
#include "Reader.h"

#pragma pack(1)
struct MacBinaryHeader
{
	uint8_t old_ver;
	uint8_t filename_len;
	char filename[63];
	uint32_t file_type, file_creator;
	uint8_t orig_finder_flags;
	uint8_t zero;
	uint16_t vertical_pos, horizontal_pos;
	uint16_t win_folder_id;
	uint8_t _protected, zero2;
	uint32_t data_len, resource_len;
	uint32_t created, last_modified;
	uint16_t getinfo_comment_len;
	uint8_t finder_flags;
	uint32_t signature; // 'mBIN'
	uint8_t script, ext_finder_flags, unused[8];
	uint32_t total_len; // unused
	uint16_t sec_header_len;
	uint8_t version_write, version_read;
	uint16_t crc;
	char padding[2]; // pad to 128 bytes
};
#pragma pack()

class MacBinary
{
public:
	MacBinary(Reader* reader);

	Reader* getDataFork();
	Reader* getResourceFork();

	static bool isMacBinary(Reader* reader);
private:
	Reader* m_reader;
	MacBinaryHeader m_header;
};



#endif
