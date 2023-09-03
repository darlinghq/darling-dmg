#include "DMGDecompressor.h"
#include <zlib.h>
#include <bzlib.h>
#ifdef COMPILE_WITH_LZFSE
	#include <lzfse.h>
#endif
#include "adc.h"
#include <cstring>
#include <memory>
#include <algorithm>
#include <stdexcept>
#include <vector>
#include <iostream>
#include "exceptions.h"
#include <cassert>

class DMGDecompressor_Zlib : public DMGDecompressor
{
public:
	DMGDecompressor_Zlib(std::shared_ptr<Reader> reader);
	~DMGDecompressor_Zlib();
	virtual int32_t decompress(void* output, int32_t count, int64_t offset) override;
private:
	virtual int32_t decompress(void* output, int32_t count);
	z_stream m_strm;
};

class DMGDecompressor_Bzip2 : public DMGDecompressor
{
public:
	DMGDecompressor_Bzip2(std::shared_ptr<Reader> reader);
	~DMGDecompressor_Bzip2();
	virtual int32_t decompress(void* output, int32_t count, int64_t offset) override;
private:
	virtual int32_t decompress(void* output, int32_t count);
	bz_stream m_strm;
};

class DMGDecompressor_ADC : public DMGDecompressor
{
public:
	DMGDecompressor_ADC(std::shared_ptr<Reader> reader) : DMGDecompressor(reader) {}
	virtual int32_t decompress(void* output, int32_t count, int64_t offset) override;
};

class DMGDecompressor_LZFSE : public DMGDecompressor
{
public:
	DMGDecompressor_LZFSE(std::shared_ptr<Reader> reader) : DMGDecompressor(reader) {}
	virtual int32_t decompress(void* output, int32_t count, int64_t offset) override;
private:
	virtual int32_t decompress(void* output, int32_t outputBytes);
};

DMGDecompressor::DMGDecompressor(std::shared_ptr<Reader> reader)
	: m_reader(reader), m_pos(0)
{
}

DMGDecompressor* DMGDecompressor::create(RunType runType, std::shared_ptr<Reader> reader)
{
	switch (runType)
	{
		case RunType::Zlib:
			return new DMGDecompressor_Zlib(reader);
		case RunType::Bzip2:
			return new DMGDecompressor_Bzip2(reader);
		case RunType::ADC:
			return new DMGDecompressor_ADC(reader);
#ifdef COMPILE_WITH_LZFSE
		case RunType::LZFSE:
			return new DMGDecompressor_LZFSE(reader);
#endif
		default:
			return nullptr;
	}
}

int32_t DMGDecompressor::readSome(char** ptr)
{
	*ptr = m_buf;
	int32_t rd = m_reader->read(m_buf, sizeof(m_buf), m_pos);
	
	if (rd <= 0)
		throw io_error("DMGDecompressor cannot read from stream");
	
	return rd;
}

/*
 * bytes can be a negative number, but have to be >= m_pos
 */
void DMGDecompressor::processed(int32_t bytes)
{
	if ( bytes < 0 && m_pos < (uint32_t)(-bytes) )
		throw io_error("Position would become negative");
	m_pos += bytes;

#ifdef DEBUG
	//std::cout << "Processed: " << bytes << ", total: " << m_pos << std::endl;
#endif
}

DMGDecompressor_Zlib::DMGDecompressor_Zlib(std::shared_ptr<Reader> reader)
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

int32_t DMGDecompressor_Zlib::decompress(void* output, int32_t count)
{
	int status;
	char* input;
	int32_t bytesRead;
	
#ifdef DEBUG
	//std::cout << "zlib: Asked to provide " << outputBytes << " bytes\n";
#endif

	m_strm.next_out = (uint8_t*) output;
	m_strm.avail_out = count;
	do
	{
		if (m_strm.avail_in == 0)
		{
			bytesRead = readSome(&input);
			if (bytesRead <= 0)
				throw io_error("Error reading zlib stream");
			processed(bytesRead);
			m_strm.next_in = (uint8_t*)input;
			m_strm.avail_in = (uint32_t)bytesRead;
		}
	
		status = inflate(&m_strm, Z_SYNC_FLUSH);
		
		if (status == Z_STREAM_END)
			return count - m_strm.avail_out;
		else if (status < 0)
			return status;
	}
	while (m_strm.avail_out > 0);

	return count;
}

int32_t DMGDecompressor_Zlib::decompress(void* output, int32_t count, int64_t offset)
{
	int32_t done = 0;
	
#ifdef DEBUG
	std::cout << "zlib: Asked to provide " << count << " bytes\n";
#endif

	while (offset > 0)
	{
		char waste[4096];
		int32_t to_read = std::min(int64_t(sizeof(waste)), offset);
		int32_t bytesDecompressed = decompress(waste, to_read);
		if (bytesDecompressed <= 0)
			return bytesDecompressed;
		offset -= bytesDecompressed;
	}
	int32_t bytesDecompressed = decompress((uint8_t*)output+done, count);
	return bytesDecompressed;
}

DMGDecompressor_Bzip2::DMGDecompressor_Bzip2(std::shared_ptr<Reader> reader)
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

int32_t DMGDecompressor_Bzip2::decompress(void* output, int32_t count)
{
	int status;
	char* input;
	int32_t bytesRead;
	
#ifdef DEBUG
	//std::cout << "bz2: Asked to provide " << outputBytes << " bytes\n";
#endif

	m_strm.next_out = (char*) output;
	m_strm.avail_out = count;
	do
	{
		if (m_strm.avail_in == 0)
		{
			bytesRead = readSome(&input);
			if (bytesRead <= 0)
				throw io_error("Error reading bz2 stream");
			processed(bytesRead);
			m_strm.next_in = input;
			m_strm.avail_in = (uint32_t)bytesRead;
		}
	
		status = BZ2_bzDecompress(&m_strm);

		if (status == BZ_STREAM_END)
			break;
		else if (status < 0)
			return status;
	}
	while (m_strm.avail_out > 0);

	return count;
}

int32_t DMGDecompressor_Bzip2::decompress(void* output, int32_t count, int64_t offset)
{
#ifdef DEBUG
	//std::cout << "bz2: Asked to provide " << outputBytes << " bytes\n";
#endif

	while (offset > 0)
	{
		char waste[4096];
		int32_t to_read = std::min(int64_t(sizeof(waste)), offset);
		int32_t bytesDecompressed = decompress(waste, to_read);
		if (bytesDecompressed <= 0)
			return bytesDecompressed;
		offset -= bytesDecompressed;
	}
	int32_t bytesDecompressed = decompress((uint8_t*)output, count);
	return bytesDecompressed;
}

int32_t DMGDecompressor_ADC::decompress(void* output, int32_t count, int64_t offset)
{
	if (offset < 0)
		throw io_error("offset < 0");

	int32_t countLeft = count;
	int32_t nb_read;
	int32_t nb_input_char_used;
	char* inputBuffer;
	int restartIndex = 0;
	int bytes_written;

	uint8_t decrompressBuffer[0x20000 + 0x80]; // 2x maximum lookback + maximum size of a decompressed chunk

	while ( countLeft > 0 )
	{
		nb_read = readSome(&inputBuffer);

		nb_input_char_used = adc_decompress(nb_read, (uint8_t*)inputBuffer, sizeof(decrompressBuffer), (uint8_t* )&decrompressBuffer[0], restartIndex, &bytes_written);

		if (nb_input_char_used == 0)
			throw io_error("nb_input_char_used == 0");
		
		if ( bytes_written >= offset+countLeft) {
			memcpy(output, decrompressBuffer+offset, countLeft);
			countLeft = 0;
		}
		else if ( bytes_written >= 0x20000) {
			if (offset < 0x10000) {
				memcpy(output, decrompressBuffer+offset, 0x10000-offset);
				output = ((uint8_t*)output)+0x10000-offset;
				offset = 0;
				countLeft -= 0x10000-offset;
			}else{
				// to copy = 0
				offset -= 0x10000;
			}
			memmove(decrompressBuffer, decrompressBuffer+0x10000, bytes_written - 0x10000); // memory can overlap, so memmove in mandatory
			restartIndex = bytes_written - 0x10000;
		}else{
			restartIndex = bytes_written;
		}

		processed(nb_input_char_used);
	}
	return count;
}

#ifdef COMPILE_WITH_LZFSE

int32_t DMGDecompressor_LZFSE::decompress(void* output, int32_t outputBytes)
{
	// DMGDecompressor can only read by 8k while compressed length of a LZFSE block can be much bigger

	char* input = nullptr;
	char *inputBig = nullptr;
	
	int32_t inputBytes = readSome(&input);
	if (inputBytes < 0) // read error
		return inputBytes;

	const uint64_t readerTotalSize = readerLength();

	if ((uint32_t)inputBytes < readerTotalSize) // inputBytes is >=0 here.
	{
		inputBig = new char[readerTotalSize];
		memcpy(inputBig, input, inputBytes);

		processed(inputBytes);

		do
		{
			int32_t nextReadBytes = readSome(&input);
			if (nextReadBytes < 0) // read error
				return nextReadBytes;
			
			memcpy(inputBig + inputBytes, input, nextReadBytes);

			inputBytes += nextReadBytes; // nextReadBytes is >=0 here, so inputBytes is still >= 0.

			processed(nextReadBytes);
		} 
		while ((uint32_t)inputBytes < readerTotalSize);

		input = inputBig;
	}

	size_t out_size = lzfse_decode_buffer((uint8_t *)output, outputBytes, (const uint8_t *)input, inputBytes, nullptr);

	if (out_size == 0)
		throw io_error("DMGDecompressor_LZFSE failed");

	if (inputBig)
		delete[] inputBig;
	else
		processed(inputBytes);
	
	return out_size;
}

int32_t DMGDecompressor_LZFSE::decompress(void* output, int32_t count, int64_t offset)
{
	int32_t done = 0;

#ifdef DEBUG
	std::cout << "lzfse: Asked to provide " << count << " bytes\n";
#endif

	while (offset > 0)
	{
		char waste[4096];
		int32_t to_read = std::min(int64_t(sizeof(waste)), offset);
		int32_t bytesDecompressed = decompress(waste, to_read);
		// bytesDecompressed seems to be always equal to to_read
		assert(bytesDecompressed == to_read);
		offset -= bytesDecompressed;
	}
	done = decompress(output, count);
	return done;
}

#endif
