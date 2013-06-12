#include "DMGDecompressor.h"
#include <zlib.h>
#include <bzlib.h>
#include "adc.h"
#include <cstring>
#include <memory>
#include <algorithm>
#include <stdexcept>
#include <vector>

class DMGDecompressor_Zlib : public DMGDecompressor
{
public:
	DMGDecompressor_Zlib();
	~DMGDecompressor_Zlib();
	virtual int32_t decompress(const void* input, int32_t inputBytes, void* output, int32_t outputBytes) override;
private:
	z_stream m_strm;
};

class DMGDecompressor_Bzip2 : public DMGDecompressor
{
public:
	DMGDecompressor_Bzip2();
	~DMGDecompressor_Bzip2();
	virtual int32_t decompress(const void* input, int32_t inputBytes, void* output, int32_t outputBytes) override;
private:
	bz_stream m_strm;
};

class DMGDecompressor_ADC : public DMGDecompressor
{
public:
	virtual int32_t decompress(const void* input, int32_t inputBytes, void* output, int32_t outputBytes) override;
private:
	std::vector<uint8_t> m_leftover;
};

DMGDecompressor* DMGDecompressor::create(RunType runType)
{
	switch (runType)
	{
		case RunType::Zlib:
			return new DMGDecompressor_Zlib;
		case RunType::Bzip2:
			return new DMGDecompressor_Bzip2;
		case RunType::ADC:
			return new DMGDecompressor_ADC;
		default:
			return nullptr;
	}
}

DMGDecompressor_Zlib::DMGDecompressor_Zlib()
{
	memset(&m_strm, 0, sizeof(m_strm));
	if (inflateInit(&m_strm) != Z_OK)
		throw std::bad_alloc();
}

DMGDecompressor_Zlib::~DMGDecompressor_Zlib()
{
	inflateEnd(&m_strm);
}

int32_t DMGDecompressor_Zlib::decompress(const void* input, int32_t inputBytes, void* output, int32_t outputBytes)
{
	m_strm.next_in = (Bytef*) input;
	m_strm.next_out = (Bytef*) output;
	m_strm.avail_in = inputBytes;
	m_strm.avail_out = outputBytes;
	
	return inflate(&m_strm, Z_NO_FLUSH);
}

DMGDecompressor_Bzip2::DMGDecompressor_Bzip2()
{
	memset(&m_strm, 0, sizeof(m_strm));
	if (BZ2_bzDecompressInit(&m_strm, 0, false) != Z_OK)
		throw std::bad_alloc();
}

DMGDecompressor_Bzip2::~DMGDecompressor_Bzip2()
{
	BZ2_bzDecompressEnd(&m_strm);
}

int32_t DMGDecompressor_Bzip2::decompress(const void* input, int32_t inputBytes, void* output, int32_t outputBytes)
{
	m_strm.next_in = (char*) input;
	m_strm.next_out = (char*) output;
	m_strm.avail_in = inputBytes;
	m_strm.avail_out = outputBytes;
	
	return BZ2_bzDecompress(&m_strm);
}

int32_t DMGDecompressor_ADC::decompress(const void* input, int32_t inputBytes, void* output, int32_t outputBytes)
{
	int32_t dec;
	int left;
	
	// FIXME: this is naive and possibly too slow
	if (!m_leftover.empty())
	{
		m_leftover.insert(m_leftover.end(), (uint8_t*) input, ((uint8_t*) input) + inputBytes);
		left = adc_decompress(m_leftover.size(), &m_leftover[0], outputBytes, (uint8_t*) output, &dec);
	}
	else
		left = adc_decompress(inputBytes, (uint8_t*) input, outputBytes, (uint8_t*) output, &dec);
	
	if (left > 0)
		m_leftover.assign(((uint8_t*) input) + inputBytes - left, ((uint8_t*) input) + inputBytes);
	
	return dec;
}

