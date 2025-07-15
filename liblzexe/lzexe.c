#include <stdint.h>
#include <string.h>
#include "lzexe.h"

void lzexe_reset(lzexe_state_t* lzexe)
{
	lzexe->eof = 0;
	lzexe->input = lzexe->input_top;
	lzexe->output = lzexe->buf_top;
	lzexe->need_bitfield = 1;
	lzexe->copy_count = 0;
	lzexe->bitfield = 0;
	lzexe->reference_offset = 0;
	lzexe->bits_read = 0;
}

void lzexe_setup(lzexe_state_t* lzexe, uint8_t* input, uint8_t* output, uint32_t buf_size)
{
	lzexe->input_top = input;
	lzexe->buf_top = output;
	lzexe->buf_size = buf_size;
	lzexe_reset(lzexe);
}

#define UpdateBitfield()\
	bitfield >>= 1;\
	bits_read++;\
	if(bits_read == 16){\
		bitfield = *input++;\
		bitfield |= ((*input++) << 8);\
		bits_read = 0;\
	}

int lzexe_read_all(uint8_t* input, uint8_t* output)
{
	if (!input)
		return 0;

	uint8_t* output_start = output;

	int16_t copy_count;
	uint16_t bitfield = 0;
	int16_t reference_offset = 0;
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
				copy_count = (bitfield & 1) << 1;
				UpdateBitfield();
				copy_count |= (bitfield & 1);
				copy_count += 2;
				UpdateBitfield();
				reference_offset = 0xFF00 | *input++;
				break;
			case 1:
				// direct copy
				*output++ = *input++;
				copy_count = -1;
				break;
			case 2:
				// embedded/separate
				reference_offset = ((((input[1]) << 5) & 0x1F00) | *input) | 0xE000;
				copy_count = (input[1] & 7) + 2;
				input += 2;
				if (copy_count == 2) {
					// large copy
					copy_count = (*input++) + 1;
					if (copy_count == 1) {
						return output - output_start;
					}
				}
				break;
		}

		while (copy_count > 0) {
			*output = output[reference_offset];
			output++;
			copy_count--;
		}
	}

	return -1;
}

int lzexe_read_partial(lzexe_state_t* lzexe, uint16_t chunk_size)
{
	if (!lzexe->input)
		return 0;
	if (!chunk_size)
		return 0;
	if (lzexe->eof)
		return 0;

	int16_t copy_count = lzexe->copy_count;
	uint16_t bitfield = lzexe->bitfield;
	int16_t reference_offset = lzexe->reference_offset;
	uint8_t* input = lzexe->input;
	uint8_t* output = lzexe->output;
	uint16_t bits_read = lzexe->bits_read;

	uint16_t chunk_bytes_left = chunk_size;

	int16_t partial_copy_count;
	uint8_t compression_type;
	uint16_t bytes_written;

	if (output >= lzexe->buf_top + lzexe->buf_size) {
		output = lzexe->buf_top;
	}

	if (lzexe->need_bitfield) {
		bitfield = *input++;
		bitfield |= ((*input++) << 8);
		lzexe->need_bitfield = 0;
	}

	while (1)
	{
		if (copy_count > 0)
		{
		copy_bytes:
			if (copy_count > chunk_bytes_left) {
				partial_copy_count = chunk_bytes_left;
				copy_count -= chunk_bytes_left;
				chunk_bytes_left = 0;
			}
			else {
				partial_copy_count = copy_count;
				chunk_bytes_left -= copy_count;
				copy_count = 0;
			}

			//printf("referenced copy %d bytes\n", partial_copy_count);
			while (partial_copy_count > 0) {
				*output = output[reference_offset];
				output++;
				partial_copy_count--;
			}

			if (chunk_bytes_left == 0) {
				goto save_lzexe_state;
			}
		}

	read_bitfield:
		compression_type = bitfield & 1;
		UpdateBitfield();

		if (compression_type == 0) {
			compression_type = (bitfield & 1) << 1;
			UpdateBitfield();
		}

		switch (compression_type)
		{
			case 0:
				// inline
				copy_count = (bitfield & 1) << 1;
				UpdateBitfield();
				copy_count |= (bitfield & 1);
				copy_count += 2;
				UpdateBitfield();
				reference_offset = 0xFF00 | *input++;
				goto copy_bytes;
			case 1:
				// direct copy
				//printf("direct copy\n");
				*output++ = *input++;
				chunk_bytes_left--;
				if (chunk_bytes_left == 0) {
					goto save_lzexe_state;
				}
				goto read_bitfield;
			case 2:
				// embedded/separate
				reference_offset = ((((input[1]) << 5) & 0x1F00) | *input) | 0xE000;
				copy_count = (input[1] & 7) + 2;
				input += 2;
				if (copy_count == 2) {
					// large copy
					copy_count = (*input++) + 1;
					if (copy_count == 1) {
						copy_count = 0;
						goto lzexe_eof;
					}
					goto copy_bytes;
				}
		}
	}

lzexe_eof:
	lzexe->eof = 1;

save_lzexe_state:
	lzexe->copy_count = copy_count;
	lzexe->bitfield = bitfield;
	lzexe->reference_offset = reference_offset;
	lzexe->input = input;
	lzexe->output = output;
	lzexe->bits_read = bits_read;

	//printf("copy_count ............ %d\n", copy_count);
	//printf("bitfield .............. 0x%04X\n", bitfield);
	//printf("reference_offset ...... %d\n", reference_offset);
	//printf("input ................. 0x%08X\n", input);
	//printf("output ................ 0x%08X\n", output);
	//printf("bits_read ............. %d\n", bits_read);
	//printf("\nbytes_written = %d\n\n", output - lzexe->buf_top);

	return output - lzexe->buf_top;
}
