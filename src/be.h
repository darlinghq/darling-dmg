#ifndef BENDIAN_H
#define BENDIAN_H
#include <stdint.h>
#ifdef __FreeBSD__
#include <sys/endian.h>
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
