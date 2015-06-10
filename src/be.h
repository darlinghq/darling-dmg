#ifndef BENDIAN_H
#define BENDIAN_H
#include <stdint.h>
#ifdef __FreeBSD__
#include <sys/endian.h>
#elif defined(_WIN32)
	static uint16_t htobe16(uint16_t x) {
		union { uint16_t u16; uint8_t v[2]; } ret;
		ret.v[0] = (uint8_t)(x >> 8);
		ret.v[1] = (uint8_t)x;
		return ret.u16;
	}

	static uint32_t htobe32(uint32_t x) {
		union { uint32_t u32; uint8_t v[4]; } ret;
		ret.v[0] = (uint8_t)(x >> 24);
		ret.v[1] = (uint8_t)(x >> 16);
		ret.v[2] = (uint8_t)(x >> 8);
		ret.v[3] = (uint8_t)x;
		return ret.u32;
	}

	static uint64_t htobe64(uint64_t x) {
		union { uint64_t u64; uint8_t v[8]; } ret;
		ret.v[0] = (uint8_t)(x >> 56);
		ret.v[1] = (uint8_t)(x >> 48);
		ret.v[2] = (uint8_t)(x >> 40);
		ret.v[3] = (uint8_t)(x >> 32);
		ret.v[4] = (uint8_t)(x >> 24);
		ret.v[5] = (uint8_t)(x >> 16);
		ret.v[6] = (uint8_t)(x >> 8);
		ret.v[7] = (uint8_t)x;
		return ret.u64;
	}

	// windows can be only LE
	#define __BYTE_ORDER __LITTLE_ENDIAN // this define is required in HFSCatalogBTree.cpp

	#define be16toh(x)	htobe16(x)
	#define be32toh(x)	htobe32(x)
	#define be64toh(x)	htobe64(x)

	#define le16toh(x)	x
	#define le32toh(x)	x
	#define le64toh(x)	x

#else
#include <endian.h>
#endif

template <typename T> T be(T value);

template <> inline uint16_t be(uint16_t value)
{
	return be16toh(value);
}

template <> inline uint32_t be(uint32_t value)
{
	return be32toh(value);
}

template <> inline uint64_t be(uint64_t value)
{
	return be64toh(value);
}

template <> inline int16_t be(int16_t value)
{
	return be16toh(value);
}

template <> inline int32_t be(int32_t value)
{
	return be32toh(value);
}

template <> inline int64_t be(int64_t value)
{
	return be64toh(value);
}

template <typename T> T le(T value);

template <> inline uint16_t le(uint16_t value)
{
	return le16toh(value);
}

template <> inline uint32_t le(uint32_t value)
{
	return le32toh(value);
}

template <> inline uint64_t le(uint64_t value)
{
	return le64toh(value);
}

template <> inline int16_t le(int16_t value)
{
	return le16toh(value);
}

template <> inline int32_t le(int32_t value)
{
	return le32toh(value);
}

template <> inline int64_t le(int64_t value)
{
	return le64toh(value);
}

#endif
