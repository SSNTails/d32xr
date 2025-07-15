#ifndef __LZEXE_H__
#define __LZEXE_H__

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
    uint8_t* buf_top;
    uint32_t buf_size;
} lzexe_state_t;

void lzexe_reset(lzexe_state_t* lzexe);
void lzexe_setup(lzexe_state_t* lzexe, uint8_t* input, uint8_t* output, uint32_t buf_size);
int lzexe_read_all(uint8_t* input, uint8_t* output);
int lzexe_read_partial(lzexe_state_t* lzexe, uint16_t chunk_size);

#endif
