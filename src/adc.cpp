#include <stdint.h>
#include <cstring>
#include "adc.h"

enum
{
	ADC_PLAIN = 1,
	ADC_2BYTE,
	ADC_3BYTE
};

int adc_decompress(int in_size, uint8_t* input, int avail_size, uint8_t* output, int* bytes_written)
{
	if (in_size == 0)
		return 0;
	
	bool output_full = false;
	unsigned char *inp = input;
	unsigned char *outp = output;
	int chunk_size;
	int offset;

	while (inp - input < in_size)
	{
		int chunk_type = adc_chunk_type(*inp);
		switch (chunk_type)
		{
		case ADC_PLAIN:
			chunk_size = adc_chunk_size(*inp);
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
		if (output_full)
			break;
	}
	*bytes_written = outp - output;
	return inp - input;
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
