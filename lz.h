#ifndef _LZ_H_
#define _LZ_H_

#ifdef USE_LZEXE
    #include "lzexe.h"
#else
    #include "lzss.h"
#endif


#ifdef USE_LZEXE
    #define LZSTATE         lzexe_state_t
    #define LZ_BUF_SIZE     0x2000

    static inline void LzSetup(LZSTATE* state, uint8_t* input, uint8_t* output, uint32_t buf_size) {
        lzexe_setup(state, input, output, buf_size);
    }
    static inline int LzReadPartial(LZSTATE* state, uint16_t chunk_size) {
        return lzexe_read_partial(state, chunk_size);
    }
    static inline int LzReadAll(uint8_t* input, uint8_t* output) {
        return lzexe_read_all(input, output);
    }
    static inline void LzReset(LZSTATE* state) {
        lzexe_reset(state);
    }
#else
    #define LZSTATE         lzss_state_t
    #define LZ_BUF_SIZE     0x1000

    static inline void LzSetup(LZSTATE* state, uint8_t* base, uint8_t* buf, uint32_t buf_size) {
        lzss_setup(state, base, buf, buf_size);
    }
    static inline int LzReadPartial(LZSTATE* state, uint16_t chunk) {
        return lzss_read_partial(state, chunk);
    }
    static inline int LzReadAll(LZSTATE* state) {
        return lzss_read_all(state);
    }
    static inline void LzReset(LZSTATE* state) {
        lzss_reset(state);
    }
#endif

#endif // _LZ_H_