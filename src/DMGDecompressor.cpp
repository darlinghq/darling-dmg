#include "DMGDecompressor.h"
#include <zlib.h>
#include <bzlib.h>
#include "adc.h"
#include <cstring>
#include <memory>
#include <algorithm>
#include <stdexcept>
#include <vector>
#include <iostream>

class DMGDecompressor_Zlib : public DMGDecompressor
{
public:
	DMGDecompressor_Zlib(Reader* reader);
	~DMGDecompressor_Zlib();
	virtual int32_t decompress(void* output, int32_t outputBytes) override;
private:
	z_stream m_strm;
};

class DMGDecompressor_Bzip2 : public DMGDecompressor
{
public:
	DMGDecompressor_Bzip2(Reader* reader);
	~DMGDecompressor_Bzip2();
	virtual int32_t decompress(void* output, int32_t outputBytes) override;
private:
	bz_stream m_strm;
};

class DMGDecompressor_ADC : public DMGDecompressor
{
public:
	DMGDecompressor_ADC(Reader* reader) : DMGDecompressor(reader) {}
	virtual int32_t decompress(void* output, int32_t outputBytes) override;
};

DMGDecompressor::DMGDecompressor(Reader* reader)
	: m_reader(reader), m_pos(0)
{
}

DMGDecompressor* DMGDecompressor::create(RunType runType, Reader* reader)
{
	switch (runType)
	{
		case RunType::Zlib:
			return new DMGDecompressor_Zlib(reader);
		case RunType::Bzip2:
			return new DMGDecompressor_Bzip2(reader);
		case RunType::ADC:
			return new DMGDecompressor_ADC(reader);
		default:
			return nullptr;
	}
}

int DMGDecompressor::readSome(char** ptr)
{
	*ptr = m_buf;
	int rd = m_reader->read(m_buf, sizeof(m_buf), m_pos);
	
	if (rd <= 0)
		throw std::runtime_error("DMGDecompressor cannot read from stream");
	
	return rd;
}

void DMGDecompressor::processed(int bytes)
{
	m_pos += bytes;
	std::cout << "Processed: " << bytes << ", total: " << m_pos << std::endl;
}

DMGDecompressor_Zlib::DMGDecompressor_Zlib(Reader* reader)
	: DMGDecompressor(reader)
{
	memset(&m_strm, 0, sizeof(m_strm));
	if (inflateInit(&m_strm) != Z_OK)
		throw std::bad_alloc();
}

DMGDecompressor_Zlib::~DMGDecompressor_Zlib()
{
	inflateEnd(&m_strm);
}

int32_t DMGDecompressor_Zlib::decompress(void* output, int32_t outputBytes)
{
	int status;
	char* input;
	int inputBytes;
	int32_t done = 0;

	do
	{
		inputBytes = readSome(&input);
	
		m_strm.next_in = (Bytef*) input;
		m_strm.next_out = (Bytef*) output + done;
		m_strm.avail_in = inputBytes;
		m_strm.avail_out = outputBytes - done;
	
		std::cout << "ZLIB decompressor supplying " << inputBytes << " bytes\n";
		std::cout << "Buffer is at " << (void*)m_strm.next_in << std::endl;
	
		status = inflate(&m_strm, Z_SYNC_FLUSH);

		processed(inputBytes - m_strm.avail_in);
		done += outputBytes - m_strm.avail_out;

		std::cout << m_strm.avail_in << " bytes left\n";
		std::cout << "status = " << status << std::endl;

		if (status == Z_STREAM_END)
			break;
		else if (status < 0)
			return status;
	}
	while (done == 0);

	return done;
}

DMGDecompressor_Bzip2::DMGDecompressor_Bzip2(Reader* reader)
	: DMGDecompressor(reader)
{
	memset(&m_strm, 0, sizeof(m_strm));
	if (BZ2_bzDecompressInit(&m_strm, 0, false) != Z_OK)
		throw std::bad_alloc();
}

DMGDecompressor_Bzip2::~DMGDecompressor_Bzip2()
{
	BZ2_bzDecompressEnd(&m_strm);
}

int32_t DMGDecompressor_Bzip2::decompress(void* output, int32_t outputBytes)
{
	int status;
	char* input;
	int inputBytes;
	int32_t done = 0;
	
	std::cout << "bz2: Asked to provide " << outputBytes << " bytes\n";

	do
	{
		inputBytes = readSome(&input);

		m_strm.next_in = (char*) input;
		m_strm.next_out = (char*) output + done;
		m_strm.avail_in = inputBytes;
		m_strm.avail_out = outputBytes - done;
	
		std::cout << "Bzip2 decompressor supplying " << inputBytes << " bytes\n";
		std::cout << "Bzip2 output buffer is " << outputBytes-done << " bytes long\n";
	
		status = BZ2_bzDecompress(&m_strm);

		processed(inputBytes - m_strm.avail_in);
		done += outputBytes - m_strm.avail_out;

		std::cout << m_strm.avail_in << " bytes left in input\n";
		std::cout << "bzip2: status = " << status << std::endl;
		std::cout << "bzip2: avail_out = " << m_strm.avail_out << std::endl;

		if (status == BZ_STREAM_END)
			break;
		else if (status < 0)
			return status;
	}
	while (done == 0);

	return done;
}

int32_t DMGDecompressor_ADC::decompress(void* output, int32_t outputBytes)
{
	int32_t dec;
	int left;
	char* input;
	int inputBytes;

	inputBytes = readSome(&input);
	
	left = adc_decompress(inputBytes, (uint8_t*) input, outputBytes, (uint8_t*) output, &dec);
	
	processed(inputBytes - left);
	return dec;
}

