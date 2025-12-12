#include <stdint.h>
#include <string.h>
#include "lzexe.h"

#define VGM_READAHEAD       0x200
#define VGM_MAX_READAHEAD   VGM_READAHEAD*8
#define VGM_LZEXE_BUF_SIZE   0x2000

static lzexe_state_t vgm_lzexe = { 0 };
static int vgm_preread_len = 0;
extern void *vgm_ptr;
extern int pcm_baseoffs;
extern int vgm_size;
extern uint16_t cd_ok;

__attribute__((aligned(4))) uint8_t vgm_lzexe_buf[VGM_LZEXE_BUF_SIZE];

void lzexe_setup(lzexe_state_t* lzexe, uint8_t* base, uint8_t *buf, uint32_t buf_size) __attribute__((section(".data"), aligned(16)));
int lzexe_read_partial(lzexe_state_t* lzexe, uint16_t chunk) __attribute__((section(".data"), aligned(16)));
void lzexe_reset(lzexe_state_t* lzexe) __attribute__((section(".data"), aligned(16)));

int vgm_setup(void* fm_ptr) __attribute__((section(".data"), aligned(16)));
int vgm_read(void) __attribute__((section(".data"), aligned(16)));
int vgm_read2(int cnt) __attribute__((section(".data"), aligned(16)));

int vgm_setup(void* fm_ptr)
{
    if (cd_ok && vgm_size < 0x20000) {
        // precache the whole VGM file in word ram to reduce bus
        // contention when reading from ROM during the gameplay
        uint8_t *scdWordRam = (uint8_t *)0x600000;
        memcpy(scdWordRam, fm_ptr, vgm_size);
        fm_ptr = scdWordRam;
    }

    lzexe_setup(&vgm_lzexe, fm_ptr, vgm_lzexe_buf, VGM_LZEXE_BUF_SIZE);

    vgm_ptr = vgm_lzexe_buf;
    vgm_preread_len = 0;

    return vgm_read();
}

void vgm_reset(void)
{
    lzexe_reset(&vgm_lzexe);
    vgm_preread_len = 0;
    vgm_ptr = vgm_lzexe_buf;
}

int vgm_read2(int length)
{
    int l, r;
    if (length > vgm_preread_len)
    {
        l = vgm_preread_len;
        r = lzexe_read_partial(&vgm_lzexe, length - l);
    }
    else
    {
        l = length;
        r = 0;
    }
    vgm_preread_len -= l;
    return l + r;
}

int vgm_read(void)
{
    return vgm_read2(VGM_READAHEAD);
}

int vgm_preread(int length)
{
    int r;
    if (vgm_preread_len + length > VGM_MAX_READAHEAD)
        length = VGM_MAX_READAHEAD - vgm_preread_len;
    r = lzexe_read_partial(&vgm_lzexe, length);
    vgm_preread_len += r;
    return r;
}
