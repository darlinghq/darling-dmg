#ifndef ADC_H
#define ADC_H
#include <stdint.h>

int adc_decompress(int in_size, uint8_t* input, int avail_size, uint8_t* output, int restartIndex, int* bytes_written);
int adc_chunk_type(char _byte);
int adc_chunk_size(char _byte);
int adc_chunk_offset(unsigned char *chunk_start);

#endif
