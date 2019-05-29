#include <stdint.h>
#include <cstring>
#include "adc.h"

enum
{
	ADC_PLAIN = 1,
	ADC_2BYTE,
	ADC_3BYTE
};


/* Compression phrases
 * store phrase - 1 byte header + data, first byte 0x80-0xFF, max length 0x80 (7 bits + 1), no offset
 * short phrase - 2 byte header + data, first byte 0x00-0x3F, max length 0x12 (4 bits + 3), max offset 0x3FF (10 bits)
 * long phrase  - 3 byte header + data, first byte 0x40-0x7F, max length 0x43 (6 bits + 4), max offset 0xFFFF (16 bits)
 */

int adc_decompress(int in_size, uint8_t* input, int avail_size, uint8_t* output, int restartIndex, int* bytes_written)
{
	if (in_size == 0)
		return 0;
	
	bool output_full = false;
	bool input_short = false;
	unsigned char *inp = input;
	unsigned char *outp = output + restartIndex;
	int chunk_size;
	int offset;

	while (inp - input < in_size)
	{
		int chunk_type = adc_chunk_type(*inp);
		switch (chunk_type)
		{
		case ADC_PLAIN:
			chunk_size = adc_chunk_size(*inp);
			if ( inp - input > in_size - (chunk_size+1) ) {
				input_short = true;
				break;
			}
			if (outp + chunk_size - output > avail_size)
			{
				output_full = true;
				break;
			}
			memcpy(outp, inp + 1, chunk_size);
			inp += chunk_size + 1;
			outp += chunk_size;
			break;

		case ADC_2BYTE:
			if ( inp - input > in_size - 2 ) {
				input_short = true;
				break;
			}
			chunk_size = adc_chunk_size(*inp);
			offset = adc_chunk_offset(inp);
			if (outp + chunk_size - output > avail_size)
			{
				output_full = true;
				break;
			}
			if (offset == 0)
			{
				memset(outp, *(outp - offset - 1), chunk_size);
				outp += chunk_size;
				inp += 2;
			}
			else
			{
				for (int i = 0; i < chunk_size; i++)
				{
					memcpy(outp, outp - offset - 1, 1);
					outp++;
				}
				inp += 2;
			}
			break;

		case ADC_3BYTE:
			if ( inp - input > in_size - 3 ) {
				input_short = true;
				break;
			}
			chunk_size = adc_chunk_size(*inp);
			offset = adc_chunk_offset(inp);
			if (outp + chunk_size - output > avail_size)
			{
				output_full = true;
				break;
			}
			if (offset == 0)
			{
				memset(outp, *(outp - offset - 1), chunk_size);
				outp += chunk_size;
				inp += 3;
			}
			else
			{
				for (int i = 0; i < chunk_size; i++)
				{
					memcpy(outp, outp - offset - 1, 1);
					outp++;
				}
				inp += 3;
			}
			break;
		}
		if (output_full || input_short)
			break;
	}
	*bytes_written = int(outp - output); // safe cast, outp - output is < avail_size
	return int(inp - input); // safe cast, inp - input is < in_size
}

int adc_chunk_type(char _byte)
{
	if (_byte & 0x80)
		return ADC_PLAIN;
	else if (_byte & 0x40)
		return ADC_3BYTE;
	else
		return ADC_2BYTE;
}

int adc_chunk_size(char _byte)
{
	switch (adc_chunk_type(_byte))
	{
	case ADC_PLAIN:
		return (_byte & 0x7F) + 1;
	case ADC_2BYTE:
		return ((_byte & 0x3F) >> 2) + 3;
	case ADC_3BYTE:
		return (_byte & 0x3F) + 4;
	}
	return -1;
}

int adc_chunk_offset(unsigned char *chunk_start)
{
	switch (adc_chunk_type(*chunk_start))
	{
	case ADC_PLAIN:
		return 0;
	case ADC_2BYTE:
		return ((((unsigned char)*chunk_start & 0x03)) << 8) + (unsigned char)*(chunk_start + 1);
	case ADC_3BYTE:
		return (((unsigned char)*(chunk_start + 1)) << 8) + (unsigned char)*(chunk_start + 2);
	}
	return -1;
}
