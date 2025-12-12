#ifndef _LZEXE_H_
#define _LZEXE_H_

#include <stdint.h>

typedef struct
{
    uint8_t need_bitfield;
    int16_t copy_count;
    uint16_t bitfield;
    int16_t reference_offset;
    uint16_t bits_read;

    // current global state
    uint8_t eof;

    // incremented on each byte read
    uint8_t* input;
    // only set once during setup
    uint8_t* input_top;

    // the output ring buffer
    uint8_t* output;
    uint16_t output_pos;
    uint16_t output_mask;
} lzexe_state_t;

void lzexe_setup(lzexe_state_t* lzexe, uint8_t* input, uint8_t* output, uint32_t buf_size);
int lzexe_read_partial(lzexe_state_t* lzexe, uint16_t chunk_size);
int lzexe_read_all(uint8_t* input, uint8_t* output);
void lzexe_reset(lzexe_state_t* lzexe);

#endif // _LZEXE_H_
