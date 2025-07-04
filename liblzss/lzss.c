#include <stdint.h>
#include <string.h>
#include "lzss.h"

#define LENSHIFT 4		/* this must be log2(LOOKAHEAD_SIZE) */

#define UpdateBitfield()\
	bitfield >>= 1;\
	bits_read++;\
	if(bits_read == 16){\
		bitfield = *input++;\
		bitfield |= ((*input++) << 8);\
		bits_read = 0;\
	}

void lzss_reset(lzss_state_t* lzss)
{
	lzss->outpos = 0;
	lzss->eof = 0;
	lzss->run = 0;
	lzss->runlen = 0;
	lzss->runpos = 0;
	lzss->input = lzss->base;
	lzss->idbyte = 0;
	lzss->getidbyte = 0;
}

void lzss_setup(lzss_state_t* lzss, uint8_t* base, uint8_t* buf, uint32_t buf_size)
{
	lzss->base = base;
	lzss->buf = buf;
	lzss->buf_size = buf_size;
	lzss->buf_mask = buf_size - 1;
	lzss_reset(lzss);
}

int lzss_read(lzss_state_t* lzss, uint16_t chunk)
{
	uint16_t i = lzss->run;
	uint16_t len = lzss->runlen;
	uint32_t source = lzss->runpos;
	uint8_t* input = lzss->input;
	uint8_t* output = lzss->buf;
	uint32_t outpos = lzss->outpos;
	uint8_t idbyte = lzss->idbyte;
	uint8_t getidbyte = lzss->getidbyte;
	uint16_t left = chunk;
	uint32_t buf_size = lzss->buf_size;
	uint32_t buf_mask = lzss->buf_mask;

	if (!lzss->input)
		return 0;
	if (!left)
		return 0;
	if (lzss->eof)
		return 0;

	if (len)
		goto runcopy;
	if (getidbyte)
		goto crunch;

	i = 0;
	len = 0;

	while (left > 0)
	{
		/* get a new idbyte if necessary */
		if (!getidbyte) idbyte = *input++;

	crunch:
		if (idbyte & 1)
		{
			uint16_t j, limit;
			uint16_t pos;
			uint32_t outpos_masked, source_masked;

			/* decompress */
			if (buf_size <= 0x1000)
			{
				pos = *input++ << LENSHIFT;
				pos = pos | (*input >> LENSHIFT);
				len = (*input++ & 0xf) + 1;
			}
			else
			{
				pos = *input++ << 8;
				pos = pos | (*input++);
				len = (*input++ & 0xff) + 1;
			}
			source = outpos - pos - 1;

			if (len == 1) {
				/* end of stream */
				lzss->eof = 1;
				return chunk - left;
			}

		runcopy:
			limit = len - i;
			if (limit > left) limit = left;

			outpos_masked = outpos & buf_mask;
			source_masked = source & buf_mask;
			if ((outpos_masked < ((outpos + limit) & buf_mask)) &&
				(source_masked < ((source + limit) & buf_mask)))
			{
				for (j = 0; j < limit; j++)
					output[outpos_masked++] = output[source_masked++];
				outpos += limit;
				source += limit;
			}
			else
			{
				for (j = 0; j < limit; j++) {
					output[outpos & buf_mask] = output[source & buf_mask];
					outpos++;
					source++;
				}
			}
			left -= limit;
			i += limit;

			if (i == len) {
				i = len = 0;
				idbyte = idbyte >> 1;
				getidbyte = (getidbyte + 1) & 7;
			}
		}
		else {
			output[outpos & buf_mask] = *input++;
			outpos++;
			left--;
			idbyte = idbyte >> 1;
			getidbyte = (getidbyte + 1) & 7;
		}
	}

	lzss->getidbyte = getidbyte;
	lzss->idbyte = idbyte;
	lzss->run = i;
	lzss->runlen = len;
	lzss->runpos = source;
	lzss->input = input;
	lzss->outpos = outpos;

	return chunk - left;
}

int lzss_read_all(lzss_state_t* lzss)
{
	if (!lzss->input)
		return 0;

	int16_t write_count;
	uint16_t bitfield = 0;
	int16_t reference_offset = 0;
	uint8_t* input = lzss->input;
	uint8_t* output = lzss->buf;
	uint16_t bits_read = 0;

	bitfield = *input++;
	bitfield |= ((*input++) << 8);

	while (1)
	{
		uint8_t compression_type = bitfield & 1;
		UpdateBitfield();

		if (compression_type == 0) {
			compression_type = (bitfield & 1) << 1;
			UpdateBitfield();
		}

		switch (compression_type)
		{
			case 0:
				// inline
				write_count = (bitfield & 1) << 1;
				UpdateBitfield();
				write_count |= (bitfield & 1);
				write_count++;
				UpdateBitfield();
				reference_offset = 0xFF00 | *input++;
				break;
			case 1:
				// direct copy
				*output++ = *input++;
				write_count = -1;
				break;
			case 2:
				// embedded/separate
				reference_offset = ((((input[1]) << 5) & 0x1F00) | *input) | 0xE000;
				write_count = (input[1] & 7) + 1;
				input += 2;
				if(write_count == 1){
					write_count = *input++;
					if(write_count == 0)
						return output - lzss->buf;
					else if(write_count == 1)
						write_count = -1;
				}
				break;
		}
		
		while(write_count >= 0){
			*output = output[reference_offset];
			output++;
			write_count--;
		}
	}

	return output - lzss->buf;
}
