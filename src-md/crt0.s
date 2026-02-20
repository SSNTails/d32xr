| SEGA 32X support code for the 68000
| by Chilly Willy

        .text

        .equ DEFAULT_LINK_TIMEOUT, 0x3FFF

        /* Z80 local mem variable offsets */
        .equ FM_IDX,    0x40        /* music track index */
        .equ FM_CSM,    0x41        /* music CSM mode */
        .equ FM_SMPL,   0x42        /* music delay in samples (low) */
        .equ FM_SMPH,   0x43        /* music delay in samples (high) */
        .equ FM_TICL,   0x44        /* music timer ticks (low) */
        .equ FM_TICH,   0x45        /* music timer ticks (high) */
        .equ FM_TTTL,   0x46        /* music time to tick (low) */
        .equ FM_TTTH,   0x47        /* music time to tick (high) */
        .equ FM_BUFCSM, 0x48        /* music buffer reads requested */
        .equ FM_BUFGEN, 0x49        /* music buffer reads available */
        .equ FM_RQPND,  0x4A        /* 68000 request pending */
        .equ FM_RQARG,  0x4B        /* 68000 request arg */
        .equ FM_START,  0x4C        /* start offset in sram (for starting/looping vgm) */
        .equ CRITICAL,  0x4F        /* Z80 critical section flag */

        .equ REQ_ATTR,   0xFFEE
        .equ REQ_ACT,    0xFFEF     /* Request 68000 Action */
        .equ STRM_KON,   0xFFF0
        .equ STRM_ID,    0xFFF1
        .equ STRM_CHIP,  0xFFF2
        .equ STRM_LMODE, 0xFFF3
        .equ STRM_SSIZE, 0xFFF4
        .equ STRM_SBASE, 0xFFF5
        .equ STRM_FREQH, 0xFFF6
        .equ STRM_FREQL, 0xFFF7
        .equ STRM_OFFHH, 0xFFF8
        .equ STRM_OFFHL, 0xFFF9
        .equ STRM_OFFLH, 0xFFFA
        .equ STRM_OFFLL, 0xFFFB
        .equ STRM_LENHH, 0xFFFC
        .equ STRM_LENHL, 0xFFFD
        .equ STRM_LENLH, 0xFFFE
        .equ STRM_LENLL, 0xFFFF

        .equ STRM_DRUMI, 0xFFF9     /* Drum sample ID */
        .equ STRM_DRUMV, 0xFFFA     /* Drum sample volume */
        .equ STRM_DRUMP, 0xFFFB     /* Drum sample panning */

        .macro  z80rd adr, dst
        move.b  0xA00000+\adr,\dst
        .endm

        .macro  z80wr adr, src
        move.b  \src,0xA00000+\adr
        .endm

        .macro  sh2_wait
        move.w  #0x0003,0xA15102    /* assert CMD INT to both SH2s */
        move.w  #0xA55A,d2
99:
        cmp.w   0xA15120,d2         /* wait on handshake in COMM0 */
        bne.b   99b
98:
        cmp.w   0xA15124,d2         /* wait on handshake in COMM4 */
        bne.b   98b
        .endm

        .macro  sh2_cont
        move.w  #0xFFFE,d0          /* command = exit irq */
        move.w  d0,0xA15120
        move.w  d0,0xA15124
99:
        cmp.w   0xA15120,d0         /* wait on exit */
        beq.b   99b
98:
        cmp.w   0xA15124,d0         /* wait on exit */
        beq.b   98b
        .endm

        .macro  set_rv
        bset    #0,0xA15107         /* set RV */
        .endm

        .macro  clr_rv
        bclr    #0,0xA15107         /* clear RV */
        .endm

| 0x880800 - entry point for reset/cold-start

        .global _start
_start:

| Clear Work RAM
        moveq   #0,d0
        move.w  #0x3FFF,d1
        suba.l  a1,a1
1:
        move.l  d0,-(a1)
        dbra    d1,1b

| Copy initialized variables from ROM to Work RAM
        lea     __text_end,a0
        move.w  #__data_size,d0
        lsr.w   #1,d0
        subq.w  #1,d0
2:
        move.w  (a0)+,(a1)+
        dbra    d0,2b

|        lea     __stack,sp              /* set stack pointer to top of Work RAM */
        lea     0x00FFFFE0,sp           /* top of Work RAM - space for Z80 comm */

        bsr     init_hardware           /* initialize the console hardware */

        jsr     main                    /* call program main() */
3:
        stop    #0x2700
        bra.b   3b

        .align  64

| 0x880840 - 68000 General exception handler

        move.l  d0,-(sp)
        move.l  4(sp),d0            /* jump table return address */
        sub.w   #0x206,d0           /* 0 = BusError, 6 = AddrError, etc */

        /* call exception handler here */

        move.l  (sp)+,d0
        addq.l  #4,sp               /* pop jump table return address */
        rte

        .align  64

| 0x880880 - 68000 Level 4 interrupt handler - HBlank IRQ

        jmp     horizontal_blank    /* This jump is only used by Gens */

        .align  64

| 0x8808C0 - 68000 Level 6 interrupt handler - VBlank IRQ

        move.l  d0,-(sp)
        move.l  vblank,d0
        beq.b   1f
        move.l  a0,-(sp)
        movea.l d0,a0

        jmp     (a0)
1:
        move.l  (sp)+,d0
        rte

        .align  64

| 0x880900 - 68000 Level 2 interrupt handler - EXT IRQ

        move.l  d0,-(sp)
        move.l  extint,d0
        beq.b   1f
        move.l  a0,-(sp)
        movea.l d0,a0
        jmp     (a0)
1:
        move.l  (sp)+,d0
        rte


        .align  64

| Initialize the MD side to a known state for the game

init_hardware:
        move.w  #0x8104,(0xC00004)      /* display off, vblank disabled */
        move.w  (0xC00004),d0           /* read VDP Status reg */

| init joyports
        move.b  #0x40,0xA10009
        move.b  #0x40,0xA1000B
        move.b  #0x40,0xA10003
        move.b  #0x40,0xA10005

| init MD VDP
        lea     0xC00004,a0
        lea     0xC00000,a1
        move.w  #0x8004,(a0) /* reg 0 = /IE1 (no HBL INT), /M3 (enable read H/V cnt) */
        move.w  #0x8114,(a0) /* reg 1 = /DISP (display off), /IE0 (no VBL INT), M1 (DMA enabled), /M2 (V28 mode) */
        move.w  #0x8230,(a0) /* reg 2 = Name Tbl A = 0xC000 */
        move.w  #0x8328,(a0) /* reg 3 = Name Tbl W = 0xA000 */
        move.w  #0x8407,(a0) /* reg 4 = Name Tbl B = 0xE000 */
        move.w  #0x8504,(a0) /* reg 5 = Sprite Attr Tbl = 0x0800 */
        move.w  #0x8600,(a0) /* reg 6 = always 0 */
        move.w  #0x8700,(a0) /* reg 7 = BG color */
        move.w  #0x8800,(a0) /* reg 8 = always 0 */
        move.w  #0x8900,(a0) /* reg 9 = always 0 */
        move.w  #0x8A00,(a0) /* reg 10 = HINT = 0 */
        move.w  #0x8B00,(a0) /* reg 11 = /IE2 (no EXT INT), full scroll */
        |move.w  #0x8B03,(a0) /* reg 11 = /IE2 (no EXT INT), line scroll */
        move.w  #0x8C81,(a0) /* reg 12 = H40 mode, no lace, no shadow/hilite */
        move.w  #0x8D00,(a0) /* reg 13 = HScroll Tbl = 0x0000 */
        move.w  #0x8E00,(a0) /* reg 14 = always 0 */
        move.w  #0x8F01,(a0) /* reg 15 = data INC = 1 */
        move.w  #0x9010,(a0) /* reg 16 = Scroll Size = 32x64 */
        move.w  #0x9100,(a0) /* reg 17 = W Pos H = left */
        move.w  #0x9200,(a0) /* reg 18 = W Pos V = top */

| clear VRAM
        move.w  #0x8F02,(a0)            /* set INC to 2 */
        move.l  #0x40000000,(a0)        /* write VRAM address 0 */
        moveq   #0,d0
        move.w  #0x7FFF,d1              /* 32K - 1 words */
0:
        move.w  d0,(a1)                 /* clear VRAM */
        dbra    d1,0b

| The VDP state at this point is: Display disabled, ints disabled, Name Tbl A at 0xC000,
| Name Tbl B at 0xE000, Sprite Attr Tbl at 0x0800, HScroll Tbl at 0x0000, H40 V28 mode,
| and Scroll size is 64x32.

| Clear CRAM
        move.l  #0x81048F01,(a0)        /* set reg 1 and reg 15 */
        move.l  #0xC0000000,(a0)        /* write CRAM address 0 */
        moveq   #31,d3
1:
        move.l  d0,(a1)
        dbra    d3,1b

| Clear VSRAM
        move.l  #0x40000010,(a0)         /* write VSRAM address 0 */
        moveq   #19,d4
2:
        move.l  d0,(a1)
        dbra    d4,2b

        || jsr     load_font /* DLG */

| set the default palette for text
        move.l  #0xC0000000,(a0)        /* write CRAM address 0 */
        move.l  #0x00000000,(a1)        /* set background color to black */

| init controllers
        |jsr     chk_ports

| setup Z80 for FM music
        move.w  #0x0100,0xA11100        /* Z80 assert bus request */
        move.w  #0x0100,0xA11200        /* Z80 deassert reset */
3:
        move.w  0xA11100,d0
        andi.w  #0x0100,d0
        bne.b   3b                      /* wait for bus acquired */

        jsr     rst_ym2612              /* reset FM chip */

        moveq   #0,d0
        lea     0x00FFFFE0,a0           /* top of stack */
        moveq   #7,d1
4:
        move.l  d0,(a0)+                /* clear work ram used by FM driver */
        dbra    d1,4b

        lea     0xA00000,a0
        move.w  #0x1FFF,d1
5:
        move.b  d0,(a0)+                /* clear Z80 sram */
        dbra    d1,5b

        lea     z80_vgm_start,a0
        lea     z80_vgm_end,a1
        lea     0xA00000,a2
6:
        move.b  (a0)+,(a2)+             /* copy Z80 driver */
        cmpa.l  a0,a1
        bne.b   6b

        move.w  #0x0000,0xA11200        /* Z80 assert reset */

        move.w  #0x0000,0xA11100        /* Z80 deassert bus request */
7:
|        move.w  0xA11100,d0
|        andi.w  #0x0100,d0
|        beq.b   7b                      /* wait for bus released */

        move.w  #0x0100,0xA11200        /* Z80 deassert reset - run driver */

| wait on Mars side
        move.w  #0,0xA15104             /* set cart bank select */
        move.b  #0,0xA15107             /* clear RV - allow SH2 to access ROM */
8:
        cmp.l   #0x4D5F4F4B,0xA15120    /* M_OK */
        bne.b   8b                      /* wait for master ok */
9:
        cmp.l   #0x535F4F4B,0xA15124    /* S_OK */
        bne.b   9b                      /* wait for slave ok */

        move.l  #vert_blank,vblank      /* set vertical blank interrupt handler */
        move.w  #0x8174,0xC00004        /* display on, vblank enabled */
        move.w  #0x2000,sr              /* enable interrupts */
        rts


        .data

| Put remaining code in data section to lower bus contention for the rom.

| void write_byte(void *dst, unsigned char val)
        .global write_byte
write_byte:
        movea.l 4(sp),a0
        move.l  8(sp),d0
        move.b  d0,(a0)
        rts

| void write_word(void *dst, unsigned short val)
        .global write_word
write_word:
        movea.l 4(sp),a0
        move.l  8(sp),d0
        move.w  d0,(a0)
        rts

| void write_long(void *dst, unsigned int val)
        .global write_long
write_long:
        movea.l 4(sp),a0
        move.l  8(sp),d0
        move.l  d0,(a0)
        rts

| unsigned char read_byte(void *src)
        .global read_byte
read_byte:
        movea.l 4(sp),a0
        move.b  (a0),d0
        rts

| unsigned short read_word(void *src)
        .global read_word
read_word:
        movea.l 4(sp),a0
        move.w  (a0),d0
        rts

| unsigned int read_long(void *src)
        .global read_long
read_long:
        movea.l 4(sp),a0
        move.l  (a0),d0
        rts

        .global do_main
do_main:
| make sure save ram is disabled
        move.w  #0x2700,sr          /* disable ints */
        clr_rv

.ifdef CHECKSUM
calculate_checksum:
        move.w  0x88018E,d5
        |cmpi.w  #0,d5               /* should we skip the checksum routine? */
        |beq.w   checksum_pass

        lea     0x880200,a1         /* skip the ROM header */
        move.w  #0xFFBF,d1          /* read 512 bytes less for the first bank */

        moveq   #0,d0               /* initialize the word accumulator */

        move.w  0x8801A4,d2         /* get the upper word of the ROM size */
        lsr.w   #3,d2               /* last bank */
        move.w  d2,d3
        cmpi.w  #8,d2
        blo.s   0f
        move.b  #7,d2

| Handle the first page (512KB) of ROM with RV=0 to avoid hardware bugs with ROM reads.
0:
        add.w   (a1)+,d0
        add.w   (a1)+,d0
        add.w   (a1)+,d0
        add.w   (a1)+,d0
        dbra    d1,0b

        move.w  #0xFFFF,d1          /* prepare to read another 512 KB */
        subq    #1,d2
        set_rv
        lea     0x80000,a1          /* start at the second page with remapped memory */

| Handle the next seven pages (3.5MB) of ROM with RV=1
1:
        add.w   (a1)+,d0
        add.w   (a1)+,d0
        add.w   (a1)+,d0
        add.w   (a1)+,d0
        dbra    d1,1b

        move.w  #0xFFFF,d1          /* prepare to read another 512 KB */
        dbra    d2,1b

        cmpi.b  #8,d3               /* are there fewer than 8 banks total? */
        blo.b   checksum_validation

| Handle bytes beyond the first 4MB of ROM
        move.w  d3,d2               /* bank counter */
        sub.b   #8,d2               /* skip the first 8 banks */

        lea     0xA130FD,a0         /* switch banks on offset 0x300000 */
2:
        lea     0x300000,a1         /* go back to the start of the bank */
        move.b  d3,d4
        sub.b   d2,d4
        move.b  d4,(a0)             /* point to the next bank */
3:
        add.w   (a1)+,d0
        add.w   (a1)+,d0
        add.w   (a1)+,d0
        add.w   (a1)+,d0
        dbra    d1,3b

        move.w  #0xFFFF,d1          /* prepare to read another 512 KB */
        dbra    d2,2b

        move.b  #6,(a0)             /* reset bank */

checksum_validation:
        cmp.w   d0,d5               /* is the checksum correct? */
        beq.s   checksum_pass

checksum_fail:
        lea     0xC00004,a0
        lea     0xC00000,a1
        move.l  #0xC0000000,(a0)    /* write CRAM address 0 */
        move.w  #0x000E,(a1)        /* set background color to red */
checksum_fail_lock:
        bra.s   checksum_fail_lock  /* forever loop */

checksum_pass:

.else
        set_rv
.endif

setup_horizontal_interrupt:
        move.l  #horizontal_blank,0x70  /* Stay within RAM */

| check flash cart status
        cmpi.w  #2,megasd_ok
        beq.b   1f
        tst.w   everdrive_ok
        bne.b   2f
| assume standard mapper save ram
        move.b  #2,0xA130F1         /* SRAM disabled, write protected */
        bra.b   3f
1:
        move.b  bnk7_save,0xA130FF  /* MegaSD => restore bank 7 */
        move.w  #0x0002,0xA130F0    /* standard mapping, sram disabled */
        bra.b   3f
2:
        move.w  #0x8000,0xA130F0    /* MED/MSD => map bank 0 to page 0, write disabled */
3:
        clr_rv
        move.w  #0x2000,sr          /* enable ints */

check_emulator:
        lea     0xC00004,a0
        lea     0xC00000,a1

        | Clear the first word in VRAM
        move.w  #0x8174,(a0)
        move.l  #0x40200000,(a0)        /* Write VRAM address 0 */
        move.w  #0x0000,(a1)
0:
        move.w  (a0),d0                 /* Wait for VBlank */
        btst    #3,d0
        beq.s   0b
1:
        move.w  (a0),d0                 /* Wait for VBlank to end */
        btst    #3,d0
        bne.s   1b

        | Write a word to VRAM with 128KB mode
        move.w  #0x81F4,(a0)
        move.l  #0x40200000,(a0)        /* Write VRAM address 0x0020 */
        move.w  #0x0102,(a1)
2:
        move.w  (a0),d0                 /* Wait for VBlank */
        btst    #3,d0
        beq.s   2b
3:
        move.w  (a0),d0                 /* Wait for VBlank to end */
        btst    #3,d0
        bne.s   3b

        | Disable 128KB VRAM mode
        move.w  #0x8174,(a0)
        move.l  #0x00200000,(a0)        /* Read VRAM address 0x0020 */
        move.w  (a1),d0

        | Determine if this is an older emulator
        move.b  #0,legacy_emulator
        cmpi.w  #0x0002,d0
        beq.s   main_loop_start

        | 128KB VRAM Mode is unsupported by this emulator
        move.b  #2,legacy_emulator      /* Assume Kega Fusion, but could be Gens */

        | Check what value is returned from the debug register
        lea     0xC0001C,a0
        move    (a0),d0
        cmpi.w  #0xFFFF,d0
        beq.s   main_loop_start

        | The debug register did not return -1
        move.b  #3,legacy_emulator      /* Emulator determined to be Gens */

main_loop_start:
        move.b  legacy_emulator,0xA1512F    /* Allow the SH-2s to see this */
        move.w  0xA15100,d0
        or.w    #0x8000,d0
        move.w  d0,0xA15100         /* set FM - allow SH2 access to MARS hw */
        move.l  #0,0xA15120         /* let Master SH2 run */

main_loop:
        tst.b   need_ctrl_int
        beq.b   main_loop_bump_fm
        move.b  #0,need_ctrl_int
        /* send controller values to primary sh2 */
        nop                         /* prevent soft lock in Gens (why does this work?) */
        nop                         /* prevent soft lock in Gens (why does this work?) */
        nop                         /* prevent soft lock in Gens (why does this work?) */
        nop                         /* prevent soft lock in Gens (why does this work?) */
        move.w  #0x0001,0xA15102    /* assert CMD INT to primary SH2 */
10:
        move.w  0xA15120,d0         /* wait on handshake in COMM0 */
        cmpi.w  #0xA55A,d0
        bne.b   10b
        move.w  ctrl1_val,0xA15122  /* controller 1 value in COMM2 */
        move.w  #0xFF00,0xA15120
11:
        move.w  0xA15120,d0         /* wait on handshake in COMM0 */
        cmpi.w  #0xFF01,d0
        bne.b   11b
        move.w  ctrl2_val,0xA15122  /* controller 2 value in COMM2 */
        move.w  #0xFF02,0xA15120
20:
        move.w  0xA15120,d0         /* wait on handshake in COMM0 */
        cmpi.w  #0xA55A,d0
        bne.b   20b
        /* done */
        move.w  #0x5AA5,0xA15120
30:
        cmpi.w  #0x5AA5,0xA15120
        beq.b   30b

main_loop_bump_fm:
        tst.b   need_bump_fm
        beq.b   main_loop_handle_req
        move.b  #0,need_bump_fm
        bsr     bump_fm

main_loop_handle_req:
        move.w  0xA15120,d0         /* get COMM0 */
        bne.b   handle_req

        move.w  0xA15124,d0         /* get COMM4 */
        bne.w   handle_sec_req

        tst.w   fm_idx
        beq.b   00f
        moveq.l #32,d0
        move.l  d0,-(sp)
        jsr     vgm_preread
        addq.l  #4,sp
00:

        /* check hot-plug count */
        tst.b   hotplug_cnt
        bne.w   main_loop
        move.b  #60,hotplug_cnt

        move.w  ctrl1_val,d0
        cmpi.w  #0xF000,d0
        beq.b   1f                  /* no pad in port 1, do hot-plug check */
0:
        tst.b   net_type
        bne.w   main_loop           /* networking enabled, ignore port 2 */
        move.w  ctrl2_val,d0
        cmpi.w  #0xF000,d0
        bne.w   main_loop           /* pad in port 2, exit */
1:
        |bsr     chk_ports
        bra.w   main_loop

| process request from Master SH2
handle_req:
        moveq   #0,d1
        move.w  d0,-(sp)
        move.b  (sp)+,d1
        add.w   d1,d1
        move.w  prireqtbl(pc,d1.w),d1
        jmp     prireqtbl(pc,d1.w)
| unknown command
no_cmd:
        move.w  #0,0xA15120         /* done */
        bra     main_loop

        .align  2
 prireqtbl:
        dc.w    no_cmd - prireqtbl                /* 0x00 */
        dc.w    read_sram - prireqtbl             /* 0x01 */
        dc.w    write_sram - prireqtbl            /* 0x02 */
        dc.w    start_music - prireqtbl           /* 0x03 */
        dc.w    stop_music - prireqtbl            /* 0x04 */
        dc.w    crossfade_md_palette - prireqtbl  /* 0x05 */
        dc.w    read_cdstate - prireqtbl          /* 0x06 */
        dc.w    set_usecd - prireqtbl             /* 0x07 */
        dc.w    set_crsr - prireqtbl              /* 0x08 */
        dc.w    get_crsr - prireqtbl              /* 0x09 */
        dc.w    set_color - prireqtbl             /* 0x0A */
        dc.w    get_color - prireqtbl             /* 0x0B */
        dc.w    set_dbpal - prireqtbl             /* 0x0C */
        dc.w    put_chr - prireqtbl               /* 0x0D */
        dc.w    clear_a - prireqtbl               /* 0x0E */
        dc.w    load_md_sky - prireqtbl           /* 0x0F */
        dc.w    fade_md_palette - prireqtbl       /* 0x10 */
        dc.w    scroll_md_sky - prireqtbl         /* 0x11 */
        dc.w    net_getbyte - prireqtbl           /* 0x12 */
        dc.w    net_putbyte - prireqtbl           /* 0x13 */
        dc.w    net_setup - prireqtbl             /* 0x14 */
        dc.w    net_cleanup - prireqtbl           /* 0x15 */
        dc.w    set_bank_page - prireqtbl         /* 0x16 */
        dc.w    net_set_link_timeout - prireqtbl  /* 0x17 */
        dc.w    set_music_volume - prireqtbl      /* 0x18 */
        dc.w    set_gamemode - prireqtbl          /* 0x19 */
        dc.w    set_scroll_positions - prireqtbl  /* 0x1A */
        dc.w    load_md_palettes - prireqtbl      /* 0x1B */
        dc.w    queue_register_write - prireqtbl  /* 0x1C */
        dc.w    load_sfx - prireqtbl              /* 0x1D */
        dc.w    play_sfx - prireqtbl              /* 0x1E */
        dc.w    get_sfx_status - prireqtbl        /* 0x1F */
        dc.w    sfx_clear - prireqtbl             /* 0x20 */
        dc.w    update_sfx - prireqtbl            /* 0x21 */
        dc.w    stop_sfx - prireqtbl              /* 0x22 */
        dc.w    flush_sfx - prireqtbl             /* 0x23 */
        dc.w    load_voice - prireqtbl            /* 0x24 */
        dc.w    play_sequence - prireqtbl         /* 0x25 */
        dc.w    load_sequence - prireqtbl         /* 0x26 */

| process request from Secondary SH2
handle_sec_req:
        cmpi.w  #0x1600,d0
        bls     main_loop
        cmpi.w  #0x16FF,d0
        bls     set_bank_page_sec
| unknown command
        move.w  #0,0xA15124         /* done */
        bra     main_loop

read_sram:
        move.w  #0x2700,sr          /* disable ints */
        moveq   #0,d0
        move.w  0xA15122,d0         /* COMM2 holds offset */
        moveq   #0,d1
|        cmpi.w  #2,megasd_ok
|        beq.b   rd_msd_sram
        tst.w   everdrive_ok
        bne.w   rd_med_sram
| assume standard mapper save ram
        add.l   d0,d0
        lea     0x200000,a0         /* use standard mapper save ram base */
        sh2_wait
        set_rv
        move.b  #3,0xA130F1         /* SRAM enabled, write protected */
        move.b  1(a0,d0.l),d1       /* read SRAM from ODD bytes */
        move.b  #2,0xA130F1         /* SRAM disabled, write protected */
        bra.w   exit_rd_sram

rd_msd_sram:
        add.l   d0,d0
        lea     0x380000,a0         /* use the lower 32K of bank 7 */
        sh2_wait
        set_rv
        move.b  #0x80,0xA130FF      /* bank 7 is save ram */
        move.b  1(a0,d0.l),d1       /* read SRAM from ODD bytes */
        move.b  bnk7_save,0xA130FF  /* restore bank 7 */
        bra.w   exit_rd_sram

rd_med_sram:
        add.l   d0,d0
        tst.b   everdrive_ok
        bne.b   1f                  /* v1 or v2a MED */
        lea     0x040000,a0         /* use the upper 256K of page 31 */
        sh2_wait
        set_rv
        move.w  #0x801F,0xA130F0    /* map bank 0 to page 31, disable write */
        move.b  1(a0,d0.l),d1       /* read SRAM */
        move.w  #0x8000,0xA130F0    /* map bank 0 to page 0, disable write */
        bra.b   exit_rd_sram
1:
        lea     0x000000,a0         /* use the lower 32K of page 28 */
        sh2_wait
        set_rv
        move.w  #0x801C,0xA130F0    /* map bank 0 to page 28, disable write */
        move.b  1(a0,d0.l),d1       /* read SRAM */
        move.w  #0x8000,0xA130F0    /* map bank 0 to page 0, disable write */

exit_rd_sram:
        clr_rv
        sh2_cont
        move.w  d1,0xA15122         /* COMM2 holds return byte */
        move.w  #0,0xA15120         /* done */
        move.w  #0x2000,sr          /* enable ints */
        bra     main_loop

write_sram:
        move.w  #0x2700,sr          /* disable ints */
        moveq   #0,d1
        move.w  0xA15122,d1         /* COMM2 holds offset */
|        cmpi.w  #2,megasd_ok
|        beq.b   wr_msd_sram
        tst.w   everdrive_ok
        bne.w   wr_med_sram
| assume standard mapper save ram
        add.l   d1,d1
        lea     0x200000,a0         /* use standard mapper save ram base */
        sh2_wait
        set_rv
        move.b  #1,0xA130F1         /* SRAM enabled, write enabled */
        move.b  d0,1(a0,d1.l)       /* write SRAM to ODD bytes */
        move.b  #2,0xA130F1         /* SRAM disabled, write protected */
        bra.w   exit_wr_sram

wr_msd_sram:
        add.l   d1,d1
        lea     0x380000,a0         /* use the lower 32K of bank 7 */
        sh2_wait
        set_rv
        move.w  #0xA000,0xA130F0    /* enable write */
        move.b  #0x80,0xA130FF      /* bank 7 is save ram */
        move.b  d0,1(a0,d1.l)       /* write SRAM to ODD bytes */
        move.b  bnk7_save,0xA130FF  /* restore bank 7 */
        move.w  #0x8000,0xA130F0    /* disable write */
        bra.w   exit_wr_sram

wr_med_sram:
        add.l   d1,d1
        tst.b   everdrive_ok
        bne.b   1f                  /* v1 or v2a MED */
        lea     0x040000,a0         /* use the upper 256K of page 31 */
        sh2_wait
        set_rv
        move.w  #0xA01F,0xA130F0    /* map bank 0 to page 31, enable write */
        move.b  d0,1(a0,d1.l)       /* write SRAM */
        move.w  #0x8000,0xA130F0    /* map bank 0 to page 0, disable write */
        bra.b   exit_wr_sram
1:
        lea     0x000000,a0         /* use the lower 32K of page 28 */
        sh2_wait
        set_rv
        move.w  #0xA01C,0xA130F0    /* map bank 0 to page 28, enable write */
        move.b  d0,1(a0,d1.l)       /* write SRAM */
        move.w  #0x8000,0xA130F0    /* map bank 0 to page 0, disable write */

exit_wr_sram:
        clr_rv
        sh2_cont
        move.w  #0,0xA15120         /* done */
        move.w  #0x2000,sr          /* enable ints */
        bra     main_loop

set_rom_bank:
        move.l  a0,d3
        swap    d3
        lsr.w   #4,d3
        andi.w  #3,d3
        move.w  d3,0xA15104         /* set cart bank select */
        move.l  a0,d3
        andi.l  #0x0FFFFF,d3
        ori.l   #0x900000,d3
        movea.l d3,a1
        rts

start_music:
        tst.w   use_cd
        bne     start_cd
        
        /* start VGM */
        clr.l   vgm_ptr
        cmpi.w  #0x0300,d0
        beq.b   0f

        /* fetch VGM length */
        lea     vgm_size,a0
        move.w  0xA15122,0(a0)
        move.w  #0,0xA15120         /* done with upper word */
20:
        move.w  0xA15120,d0         /* wait on handshake in COMM0 */
        cmpi.w  #0x0302,d0
        bne.b   20b
        move.w  0xA15122,2(a0)
        move.w  #0,0xA15120         /* done with lower word */

        /* fetch VGM offset */
        lea     vgm_ptr,a0
21:
        move.w  0xA15120,d0         /* wait on handshake in COMM0 */
        cmpi.w  #0x0303,d0
        bne.b   21b
        move.w  0xA15122,0(a0)
        move.w  #0,0xA15120         /* done with upper word */
22:
        move.w  0xA15120,d0         /* wait on handshake in COMM0 */
        cmpi.w  #0x0304,d0
        bne.b   22b
        move.w  0xA15122,2(a0)
        move.w  #0,0xA15120         /* done with lower word */

23:
        move.w  0xA15120,d0         /* wait on handshake in COMM0 */
        cmpi.w  #0x0300,d0
        bne.b   23b

0:
        /* set VGM pointer and init VGM player */
        move.w  0xA15122,d0         /* COMM2 = index | repeat flag */
        move.w  #0x8000,d1
        and.w   d0,d1               /* repeat flag */
        eor.w   d1,d0               /* clear flag from index */
        move.w  d1,fm_rep           /* repeat flag */
        move.w  d0,fm_idx           /* index 1 to N */
        move.w  #0,0xA15104         /* set cart bank select */
        move.l  #0,a0
        move.l  vgm_ptr,d0          /* set VGM offset */
        beq     9f

        move.l  d0,a0
        bsr     set_rom_bank

        move.l  a1,-(sp)            /* MD lump ptr */
        jsr     vgm_setup           /* setup lzexe, set pcm_baseoffs, set vgm_ptr, read first block */
        lea     4(sp),sp

        /* start VGM on Z80 */
        move.w  #0x0100,0xA11100    /* Z80 assert bus request */
        move.w  #0x0100,0xA11200    /* Z80 deassert reset */
1:
        move.w  0xA11100,d0
        andi.w  #0x0100,d0
        bne.b   1b                  /* wait for bus acquired */

        jsr     rst_ym2612

        moveq   #0,d0
        lea     0x00FFFFE0,a0       /* top of stack */
        moveq   #7,d1
2:
        move.l  d0,(a0)+            /* clear work ram used by FM driver */
        dbra    d1,2b

        lea     z80_vgm_start,a0
        lea     z80_vgm_end,a1
        lea     0xA00000,a2
3:
        move.b  (a0)+,(a2)+         /* copy Z80 driver */
        cmpa.l  a0,a1
        bne.b   3b

| FM setup
        movea.l vgm_ptr,a6          /* lzexe buffer */
        lea     0x1C(a6),a6         /* loop offset */
| get vgm loop offset
        move.b  (a6)+,d0
        move.b  (a6)+,d1
        move.b  (a6)+,d2
        move.b  (a6)+,d3
        move.b  d3,-(sp)
        move.w  (sp)+,d3            /* shift left 8 */
        move.b  d2,d3
        swap    d3
        move.b  d1,-(sp)
        move.w  (sp)+,d3            /* shift left 8 */
        move.b  d0,d3
        tst.l   d3
        beq.b   0f                  /* no loop offset, use default song start */
        addi.l  #0x1C,d3
        bra.b   1f
0:
        moveq   #0x40,d3
1:
        move.l  d3,fm_loop          /* loop offset */

        movea.l vgm_ptr,a6
        lea     8(a6),a6            /* version */
        move.b  (a6)+,d0
        move.b  (a6)+,d1
        move.b  (a6)+,d2
        move.b  (a6)+,d3
        move.b  d3,-(sp)
        move.w  (sp)+,d3            /* shift left 8 */
        move.b  d2,d3
        swap    d3
        move.b  d1,-(sp)
        move.w  (sp)+,d3            /* shift left 8 */
        move.b  d0,d3
        cmpi.l  #0x150,d3           /* >= v1.50? */
        bcs     2f                  /* no, default to song start of offset 0x40 */

        movea.l vgm_ptr,a6
        lea     0x34(a6),a6         /* VGM data offset */
        move.b  (a6)+,d0
        move.b  (a6)+,d1
        move.b  (a6)+,d2
        move.b  (a6)+,d3
        move.b  d3,-(sp)
        move.w  (sp)+,d3            /* shift left 8 */
        move.b  d2,d3
        swap    d3
        move.b  d1,-(sp)
        move.w  (sp)+,d3            /* shift left 8 */
        move.b  d0,d3
        tst.l   d3
        beq.b   2f                  /* not set - use 0x40 */
        addi.l  #0x34,d3
        bra.b   3f
2:
        moveq   #0x40,d3
3:
        move.l  d3,fm_start         /* start of song data */
        z80wr   FM_START,d3         /* start song => FX_START = start offset */
        lsr.w   #8,d3
        z80wr   FM_START+1,d3

        move.w  fm_idx,d0
        z80wr   FM_IDX,d0           /* set FM_IDX */

        jsr     vgm_read
        move.w  #6, preread_cnt     /* preread blocks to fill buffer */

        z80wr   FM_BUFCSM,#0
        z80wr   FM_BUFGEN,#2
        z80wr   FM_RQPND,#0

        movea.l vgm_ptr,a0
        lea     0xA01000,a1
        move.w  #1023,d1
4:
        move.b  (a0)+,(a1)+         /* copy vgm data from decompressor buffer to sram */
        dbra    d1,4b

        move.w  #0x0400,offs68k
        move.w  #0x0400,offsz80

        move.w  #0x0000,0xA11200    /* Z80 assert reset */
        move.w  #0x0000,0xA11100    /* Z80 deassert bus request */
        move.w  #0x0100,0xA11200    /* Z80 deassert reset - run driver */

        move.w  #0,0xA15120         /* done */
        bra     main_loop
9:
        clr.w   fm_idx              /* not playing VGM */

        move.w  #0,0xA15120         /* done */
        bra     main_loop

start_cd:
        tst.w   megasd_num_cdtracks
        beq     9f                   /* no MD+ tracks */

        move.w  0xA15122,d0          /* COMM2 = index | repeat flag */
        move.w  #0x8000,d1
        and.w   d0,d1                /* repeat flag */
        eor.w   d1,d0                /* clear flag from index */
        lsr.w   #7,d1
        lsr.w   #8,d1

        move.l  d1,-(sp)            /* push the looping flag */
        move.l  d0,-(sp)            /* push the cdtrack */
        jsr     MegaSD_PlayCDTrack
        lea     8(sp),sp            /* clear the stack */
        move.w  #0,0xA15120         /* done */
        bra     main_loop

9:
        tst.w   cd_ok
        beq     2f                  /* couldn't init cd */
        tst.b   cd_ok
        bne.b   0f                  /* disc found - try to play track */
        clr.w   number_tracks
        /* check for CD */
10:
        move.b  0xA1200F,d1
        bne.b   10b                 /* wait until Sub-CPU is ready to receive command */
        move.b  #'D,0xA1200E        /* set main comm port to GetDiskInfo command */
11:
        move.b  0xA1200F,d0
        beq.b   11b                 /* wait for acknowledge byte in sub comm port */
        move.b  #0x00,0xA1200E      /* acknowledge receipt of command result */

        cmpi.b  #'D,d0
        bne.b   2f                  /* couldn't get disk info */
        move.w  0xA12020,d0         /* BIOS status */
        cmpi.w  #0x1000,d0
        bhs.b   2f                  /* open, busy, or no disc */
        move.b  #1,cd_ok            /* we have a disc - try to play track */

        move.w  0xA12022,d0         /* first and last track */
        moveq   #0,d1
        move.b  d0,d1
        move.w  d1,last_track
        lsr.w   #8,d0
        move.w  d0,first_track
        sub.w   d0,d1
        addq.w  #1,d1
        move.w  d1,number_tracks
0:
        move.b  0xA1200F,d1
        bne.b   0b                  /* wait until Sub-CPU is ready to receive command */

        move.w  0xA15122,d0         /* COMM2 = index | repeat flag */
        move.w  #0x8000,d1
        and.w   d0,d1               /* repeat flag */
        eor.w   d1,d0               /* clear flag from index */
        lsr.w   #7,d1
        lsr.w   #8,d1

        move.b  d1,0xA12012         /* repeat flag */
        move.w  d0,0xA12010         /* track number */
        move.b  #'P,0xA1200E        /* set main comm port to PlayTrack command */
1:
        move.b  0xA1200F,d0
        beq.b   1b                  /* wait for acknowledge byte in sub comm port */
        move.b  #0x00,0xA1200E      /* acknowledge receipt of command result */
2:
        move.w  #0,0xA15120         /* done */
        bra     main_loop

stop_music:
        tst.w    use_cd
        bne.b    stop_cd

        /* stop VGM */
        move.w  #0x0100,0xA11100    /* Z80 assert bus request */
        move.w  #0x0100,0xA11200    /* Z80 deassert reset */
0:
        move.w  0xA11100,d0
        andi.w  #0x0100,d0
        bne.b   0b                  /* wait for bus acquired */

        jsr     rst_ym2612          /* reset FM chip */

        moveq   #0,d0
        lea     0x00FFFFE0,a0       /* top of stack */
        moveq   #7,d1
1:
        move.l  d0,(a0)+            /* clear work ram used by FM driver */
        dbra    d1,1b

|        lea     z80_vgm_start,a0
|        lea     z80_vgm_end,a1
|        lea     0xA00000,a2
2:
|        move.b  (a0)+,(a2)+         /* copy Z80 driver */
|        cmpa.l  a0,a1
|        bne.b   2b

        z80wr   FM_IDX,#0           /* no music playing */
        z80wr   FM_RQPND,#0         /* no requests pending */
        z80wr   CRITICAL,#0         /* not in a critical section */

        move.w  #0x0000,0xA11200    /* Z80 assert reset */

        move.w  #0x0000,0xA11100    /* Z80 deassert bus request */
3:
|        move.w  0xA11100,d0
|        andi.w  #0x0100,d0
|        beq.b   3b                  /* wait for bus released */

        move.w  #0x0100,0xA11200    /* Z80 deassert reset - run driver */

        clr.w   fm_idx              /* stop music */
        clr.w   fm_rep

        move.w  #0,0xA15120         /* done */
        bra     main_loop

stop_cd:
        tst.w   megasd_num_cdtracks
        beq     3f                  /* no MD+ */

        jsr     MegaSD_PauseCD
        move.w  #0,0xA15120         /* done */
        bra     main_loop

3:
        tst.w   cd_ok
        beq.b   2f
0:
        move.b  0xA1200F,d1
        bne.b   0b                  /* wait until Sub-CPU is ready to receive command */

        move.b  #'S,0xA1200E        /* set main comm port to StopPlayback command */
1:
        move.b  0xA1200F,d0
        beq.b   1b                  /* wait for acknowledge byte in sub comm port */
        move.b  #0x00,0xA1200E      /* acknowledge receipt of command result */
2:
        move.w  #0,0xA15120         /* done */
        bra     main_loop



read_cdstate:
        tst.w   megasd_num_cdtracks
        beq     9f                  /* no MD+ tracks */

        move.w  megasd_num_cdtracks,d0
        lsl.l   #2,d0

        move.w  megasd_ok,d1
        or.w    cd_ok,d1
        andi.w  #3,d1
        or.w    d1,d0

        move.w  d0,0xA15122
        move.w  #0,0xA15120         /* done */
        bra     main_loop

9:
        tst.w   cd_ok
        beq     0f                  /* couldn't init cd */
        tst.b   cd_ok
        bne.b   0f                  /* disc found - return state */
        clr.w   number_tracks
        /* check for disc */
10:
        move.b  0xA1200F,d1
        bne.b   10b                 /* wait until Sub-CPU is ready to receive command */
        move.b  #'D,0xA1200E        /* set main comm port to GetDiskInfo command */
11:
        move.b  0xA1200F,d0
        beq.b   11b                 /* wait for acknowledge byte in sub comm port */
        move.b  #0x00,0xA1200E      /* acknowledge receipt of command result */

        cmpi.b  #'D,d0
        bne.b   0f                  /* couldn't get disk info */
        move.w  0xA12020,d0         /* BIOS status */
        cmpi.w  #0x1000,d0
        bhs.b   0f                  /* open, busy, or no disc */
        move.b  #1,cd_ok            /* we have a disc - get state info */

        move.w  0xA12022,d0         /* first and last track */
        moveq   #0,d1
        move.b  d0,d1
        move.w  d1,last_track
        lsr.w   #8,d0
        move.w  d0,first_track
        sub.w   d0,d1
        addq.w  #1,d1
        move.w  d1,number_tracks
0:
        move.w  number_tracks,d0
        lsl.l   #2,d0
        or.w    cd_ok,d0
        move.w  d0,0xA15122
        move.w  #0,0xA15120         /* done */
        bra     main_loop

set_usecd:
        move.w  0xA15122,use_cd     /* COMM2 holds the new value */
        move.w  #0,0xA15120         /* done */
        bra     main_loop


| network support functions

ext_serial:
        move.w  #0x2700,sr          /* disable ints */
        move.l  d1,-(sp)
        lea     net_rdbuf,a0
        move.w  net_wbix,d1

        btst    #2,0xA10019         /* check RERR on serial control register of joypad port 2 */
        bne.b   1f                  /* error */        
        btst    #1,0xA10019         /* check RRDY on serial control register of joypad port 2 */
        beq.b   3f                  /* no byte available, ignore int */
        moveq   #0,d0               /* clear serial status byte */
        move.b  0xA10017,d0         /* received byte */
        bra.b   2f
1:
        move.w  #0xFF04,d0          /* serial status and received byte = 0xFF04 (serial read error) */
2:
        move.w  d0,0(a0,d1.w)       /* write status:data to buffer */
        addq.w  #2,d1
        andi.w  #30,d1
        move.w  d1,net_wbix         /* update buffer write index */
3:
        move.l  (sp)+,d1
        movea.l (sp)+,a0
        move.l  (sp)+,d0
        rte

ext_link:
        move.w  #0x2700,sr          /* disable ints */
        btst    #6,0xA10005         /* check TH asserted */
        bne.b   2f                  /* no, extraneous ext int */

        move.l  d1,-(sp)
        lea     net_rdbuf,a0
        move.w  net_wbix,d1

        move.b  0xA10005,d0         /* read nibble from other console */
        move.b  #0x00,0xA10005      /* assert handshake (TR low) */

        move.b  d0,0(a0,d1.w)       /* write port byte to buffer - one nibble of data */
        addq.w  #1,d1
        andi.w  #31,d1
        move.w  d1,net_wbix         /* update buffer write index */
        move.w  net_link_timeout,d1
0:
        nop
        nop
        btst    #6,0xA10005         /* check TH deasserted */
        bne.b   1f                  /* handshake */
        dbra    d2,0b
        bra.b   3f                  /* timeout err */
1:
        move.b  #0x20,0xA10005      /* deassert handshake (TR high) */
        move.l  (sp)+,d1
2:
        movea.l (sp)+,a0
        move.l  (sp)+,d0
        rte
3:
        /* timeout during handshake - shut down link net */
        clr.b   net_type
        clr.l   extint
        move.w  #0x8B00,0xC00004    /* reg 11 = /IE2 (no EXT INT), full scroll */
        |move.w  #0x8B03,0xC00004    /* reg 11 = /IE2 (no EXT INT), line scroll */
        move.b  #0x40,0xA1000B      /* port 2 to neutral setting */
        nop
        nop
        move.b  #0x40,0xA10005
        move.l  (sp)+,d1
        movea.l (sp)+,a0
        move.l  (sp)+,d0
        rte

init_serial:
        move.w  #0xF000,ctrl2_val    /* port 1 ID = SEGA_CTRL_NONE */
        move.b  #0x10,0xA1000B      /* all pins inputs except TL (Pin 6) */
        nop
        nop
        move.b  #0x38,0xA10019      /* 4800 Baud 8-N-1, RDRDY INT allowed */
        clr.w   net_rbix
        clr.w   net_wbix
        move.l  #ext_serial,extint  /* serial read data ready handler */
        move.w  #0x8B08,0xC00004    /* reg 11 = IE2 (enable EXT INT), full scroll */
        |move.w  #0x8B0B,0xC00004    /* reg 11 = IE2 (enable EXT INT), line scroll */
        move.w  #0,0xA15120         /* done */
        bra     main_loop

init_link:
        move.w  #0xF000,ctrl2_val    /* port 1 ID = SEGA_CTRL_NONE */
        move.b  #0x00,0xA10019      /* no serial */
        nop
        nop
        move.b  #0xA0,0xA1000B      /* all pins inputs except TR, TH INT allowed */
        nop
        nop
        move.b  #0x20,0xA10005      /* TR set */
        clr.w   net_rbix
        clr.w   net_wbix
        move.l  #ext_link,extint    /* TH INT handler */
        move.w  #0x8B08,0xC00004    /* reg 11 = IE2 (enable EXT INT), full scroll */
        |move.w  #0x8B0B,0xC00004    /* reg 11 = IE2 (enable EXT INT), line scroll */
        move.w  #0,0xA15120         /* done */
        bra     main_loop

read_serial:
        move.w  #0x2700,sr          /* disable ints */
        lea     net_rdbuf,a0
        move.w  net_rbix,d1
        cmp.w   net_wbix,d1
        beq.b   1f                  /* no data in buffer */

        move.w  0(a0,d1.w),d0       /* received status:data */
        addq.w  #2,d1
        andi.w  #30,d1
        move.w  d1,net_rbix         /* update buffer read index */
        bra.b   2f
1:
        move.w  #0xFF00,d0          /* set status:data to 0xFF00 (no data available) */
2:
        move.w  d0,0xA15122         /* return received status:data in COMM2 */
        move.w  #0,0xA15120         /* done */
        move.w  #0x2000,sr          /* enable ints */
        bra     main_loop

read_link:
        move.w  #0x2700,sr          /* disable ints */
        btst    #0,net_wbix+1       /* check for even index */
        bne.b   1f                  /* in middle of collecting nibbles */

        lea     net_rdbuf,a0
        move.w  net_rbix,d1
        cmp.w   net_wbix,d1
        beq.b   1f                  /* no data in buffer */

        move.w  0(a0,d1.w),d0       /* data nibbles */
        andi.w  #0x0F0F,d0
        lsl.b   #4,d0
        lsr.w   #4,d0
        addq.w  #2,d1
        andi.w  #30,d1
        move.w  d1,net_rbix         /* update buffer read index */
        bra.b   2f
1:
        move.w  #0xFF00,d0          /* set status:data to 0xFF00 (no data available) */
2:
        move.w  d0,0xA15122         /* return received byte and status in COMM2 */
        move.w  #0,0xA15120         /* done */
        move.w  #0x2000,sr          /* enable ints */
        bra     main_loop

write_serial:
        move.w  #0x2700,sr          /* disable ints */
        move.w  net_link_timeout,d1
0:
        bsr     bump_fm
        btst    #0,0xA10019         /* ok to transmit? */
        beq.b   1f                  /* yes */
        dbra    d1,0b               /* wait until ok to transmit */
        bra.b   2f                  /* timeout err */
1:
        move.b  d0,0xA10015         /* Send byte */
        move.w  #0,0xA15122         /* okay */
        move.w  #0,0xA15120         /* done */
        move.w  #0x2000,sr          /* enable ints */
        bra     main_loop
2:
        move.w  net_link_timeout,0xA15122    /* timeout */
        move.w  #0,0xA15120         /* done */
        move.w  #0x2000,sr          /* enable ints */
        bra     main_loop

write_link:
        move.w  #0x2700,sr          /* disable ints */
        move.b  #0x2F,0xA1000B      /* only TL and TH in, TH INT not allowed */

        move.b  d0,d1               /* save lsn */
        lsr.b   #4,d0               /* msn */
        ori.b   #0x20,d0            /* set TR line */
        move.b  d0,0xA10005         /* set nibble out */
        nop
        nop
        andi.b  #0x0F,d0            /* clr TR line */
        move.b  d0,0xA10005         /* assert TH of other console */

        /* wait on handshake */
        move.w  net_link_timeout,d2
0:
        bsr     bump_fm
        btst    #6,0xA10005         /* check for TH low (handshake) */
        beq.b   1f                  /* handshake */
        dbra    d2,0b
        bra.w   9f                  /* timeout err */
1:
        ori.b   #0x20,d0            /* set TR line */
        move.b  d0,0xA10005         /* deassert TH of other console */
        move.w  net_link_timeout,d2
2:
        nop
        nop
        btst    #6,0xA10005         /* wait for TH high (handshake done) */
        bne.b   3f                  /* handshake */
        dbra    d2,2b
        bra.w   9f                  /* timeout err */
3:
        moveq   #0x0F,d0
        and.b   d1,d0               /* lsn */
        ori.b   #0x20,d0            /* set TR line */
        move.b  d0,0xA10005         /* set nibble out */
        nop
        nop
        andi.b  #0x0F,d0            /* clr TR line */
        move.b  d0,0xA10005         /* assert TH of other console */

        /* wait on handshake */
4:
        bsr     bump_fm
        btst    #6,0xA10005         /* check for TH low (handshake) */
        bne.b   4b

        ori.b   #0x20,d0            /* set TR line */
        move.b  d0,0xA10005         /* deassert TH of other console */
5:
        nop
        nop
        btst    #6,0xA10005         /* wait for TH high (handshake done) */
        beq.b   5b

        move.b  #0x20,0xA10005      /* TR set */
        nop
        nop
        move.b  #0xA0,0xA1000B      /* all pins inputs except TR, TH INT allowed */
        move.w  #0,0xA15122         /* okay */
        move.w  #0,0xA15120         /* done */
        move.w  #0x2000,sr          /* enable ints */
        bra     main_loop
9:
        move.b  #0x20,0xA10005      /* TR set */
        nop
        nop
        move.b  #0xA0,0xA1000B      /* all pins inputs except TR, TH INT allowed */
        move.w  #0xFFFF,0xA15122    /* timeout */
        move.w  #0,0xA15120         /* done */
        move.w  #0x2000,sr          /* enable ints */
        bra     main_loop


net_getbyte:
        tst.b   net_type
        bmi.w   read_serial
        bne.w   read_link
        move.w  #0xFF00,0xA15122    /* nothing */
        move.w  #0,0xA15120         /* done */
        bra     main_loop

net_putbyte:
        tst.b   net_type
        bmi.w   write_serial
        bne.w   write_link
        move.w  #0xFFFF,0xA15122    /* timeout */
        move.w  #0,0xA15120         /* done */
        bra     main_loop

net_setup:
        move.b  d0,net_type
        tst.b   net_type
        bmi.w   init_serial
        bne.w   init_link
        move.w  #0,0xA15120         /* done */
        bra     main_loop

net_cleanup:
        clr.b   net_type
        clr.l   extint
        move.w  #0x8B00,0xC00004    /* reg 11 = /IE2 (no EXT INT), full scroll */
        |move.w  #0x8B03,0xC00004    /* reg 11 = /IE2 (no EXT INT), line scroll */
        move.b  #0x00,0xA10019      /* no serial */
        nop
        nop
        move.b  #0x40,0xA1000B      /* port 2 to neutral setting */
        nop
        nop
        move.b  #0x40,0xA10005
        move.w  #0,0xA15120         /* done */
        bra     main_loop


| video debug functions

set_crsr:
        move.w  0xA15122,d0         /* cursor y<<6 | x */
        move.w  d0,d1
        andi.w  #0x1F,d0
        move.w  d0,crsr_y
        lsr.l   #6,d1
        move.w  d1,crsr_x
        move.w  #0,0xA15120         /* done */
        bra     main_loop

get_crsr:
        move.w  crsr_y,d0           /* y coord */
        lsl.w   #6,d0
        or.w    crsr_x,d0           /* cursor y<<6 | x */
        move.w  d0,0xA15122
        move.w  #0,0xA15120         /* done */
        bra     main_loop

set_color:
        /* the foreground color is in the LS nibble of COMM0 */
        move.w  0xA15120,d0
        andi.l  #0x000F,d0
        move    d0,d1
        lsl.w   #4,d0
        or      d1,d0
        move.w  d0,d1
        lsl.w   #8,d0
        or      d1,d0
        move.w  d0,d1
        swap    d1
        move.w  d0,d1
        move.l  d1,fg_color

        /* the background color is in the second LS nibble of COMM0 */
        move.w  0xA15120,d0
        lsr.w   #4,d0
        andi.l  #0x000F,d0
        move    d0,d1
        lsl.w   #4,d0
        or      d1,d0
        move.w  d0,d1
        lsl.w   #8,d0
        or      d1,d0
        move.w  d0,d1
        swap    d1
        move.w  d0,d1
        move.l  d1,bg_color

        || bsr     load_font /* DLG */
        move.w  #0,0xA15120         /* done */
        bra     main_loop

get_color:
        move.w  fg_color,d0
        andi.w  #0x000F,d0
        move    d0,d1
        move.w  bg_color,d0
        andi.w  #0x000F,d0
        lsl     #4,d0
        or      d1,d0
        move.w  d0,0xA15122
        move.w  #0,0xA15120         /* done */
        bra     main_loop

set_dbpal:
        andi.w  #0x0003,d0
        moveq   #13,d1
        lsl.w   d1,d0
        move.w  d0,dbug_color       /* palette select = N * 0x2000 */
        move.w  #0,0xA15120         /* done */
        bra     main_loop

put_chr:
        andi.w  #0x00FF,d0          /* character */
        subi.b  #0x20,d0            /* font starts at space */
        or.w    dbug_color,d0       /* OR with color palette */
        lea     0xC00000,a0
        move.w  #0x8F02,4(a0)       /* set INC to 2 */
        moveq   #0,d1
        move.w  crsr_y,d1           /* y coord */
        lsl.w   #6,d1
        or.w    crsr_x,d1           /* cursor y<<6 | x */
        add.w   d1,d1               /* pattern names are words */
        swap    d1
        ori.l   #0x40000003,d1      /* OR cursor with VDP write VRAM at 0xC000 (scroll plane A) */
        move.l  d1,4(a0)            /* write VRAM at location of cursor in plane A */
        move.w  d0,(a0)             /* set pattern name for character */
        addq.w  #1,crsr_x           /* increment x cursor coord */
        move.w  #0,0xA15120         /* done */
        bra     main_loop

clear_a:
        moveq   #0,d0
        lea     0xC00000,a0
        move.w  #0x8F02,4(a0)       /* set INC to 2 */
        move.l  #0x40000003,d1      /* VDP write VRAM at 0xC000 (scroll plane A) */
        move.l  d1,4(a0)            /* write VRAM at plane A start */
        move.w  #64*32-1,d1
0:
        move.w  d0,(a0)             /* clear name pattern */
        dbra    d1,0b
        move.w  #0,0xA15120         /* done */
        bra     main_loop

set_bank_page:
        move.w  d0,d1
        andi.l  #0x07,d0            /* bank number */
        lsr.w   #3,d1               /* page number */
        andi.l  #0x1f,d1
        cmpi.b  #7,d0
        bne.b   1f
        move.b  d1,bnk7_save
1:
        lea     0xA130F0,a0
        add.l   d0,d0
        lea     1(a0,d0.l),a0
        move.w  #0x2700,sr          /* disable ints */
        sh2_wait
        set_rv
        move.b  d1,(a0)
        clr_rv
        sh2_cont
        move.w  #0x2000,sr          /* enable ints */
        move.w  #0,0xA15120         /* release SH2 now */
        bra     main_loop

set_bank_page_sec:
        move.w  d0,d1
        andi.l  #0x07,d0            /* bank number */
        lsr.w   #3,d1               /* page number */
        andi.l  #0x1f,d1
        cmpi.b  #7,d0
        bne.b   1f
        move.b  d1,bnk7_save
1:
        lea     0xA130F0,a0
        add.l   d0,d0
        lea     1(a0,d0.l),a0
        move.w  #0x2700,sr          /* disable ints */
        sh2_wait
        set_rv
        move.b  d1,(a0)
        clr_rv
        sh2_cont
        move.w  #0x2000,sr          /* enable ints */
        move.w  #0x1000,0xA15124    /* release SH2 now */
        bra     main_loop

net_set_link_timeout:
        move.w  0xA15122,net_link_timeout
        move.w  #0,0xA15120         /* release SH2 now */
        bra     main_loop

set_music_volume:
        tst.w   megasd_num_cdtracks
        beq     1f                  /* no MD+ */

        move    #0xFF,d1
        and.w   d1,d0

        move.l  d0,-(sp)            /* push the volume */
        jsr     MegaSD_SetCDVolume
        lea     4(sp),sp            /* clear the stack */
1:
        tst.w   cd_ok
        beq.b   4f                  /* couldn't init cd */
        tst.b   cd_ok
        beq.b   4f                  /* disc not found */

        move.w  #0xFF,d1
        and.w   d0,d1
        addq.w  #1,d1               /* volume (1 to 256) */
        add.w   d1,d1
        add.w   d1,d1               /* volume * 4 (4 to 1024) */
2:
        move.b  0xA1200F,d0
        bne     4b                  /* wait until Sub-CPU is ready to receive command */

        move.w  d1,0xA12010         /* cd volume */
        move.b  #'V,0xA1200E        /* set main comm port to SetVolume command */
3:
        move.b  0xA1200F,d0
        beq.b   3b                  /* wait for acknowledge byte in sub comm port */
        move.b  #0x00,0xA1200E      /* acknowledge receipt of command result */
4:
        move.w  #0,0xA15120         /* done */
        bra     main_loop



set_gamemode:
        move.b  0xA15122,gamemode
        move.w  #0,0xA15120         /* done */
        bra     main_loop



get_lump_source_and_size:
        /* fetch lump length */
        lea     lump_size,a0
        move.w  0xA15122,0(a0)
        move.w  #0,0xA15120         /* done with upper word */
21:
        move.w  0xA15120,d0         /* wait on handshake in COMM0 */
        cmpi.b  #0x02,d0
        bne.b   21b
        move.w  0xA15122,2(a0)
        move.w  #0,0xA15120         /* done with lower word */

        /* fetch lump offset */
        lea     lump_ptr,a0
22:
        move.w  0xA15120,d0         /* wait on handshake in COMM0 */
        cmpi.b  #0x03,d0
        bne.b   22b
        move.w  0xA15122,0(a0)
        move.w  #0,0xA15120         /* done with upper word */
23:
        move.w  0xA15120,d0         /* wait on handshake in COMM0 */
        cmpi.b  #0x04,d0
        bne.b   23b
        move.w  0xA15122,2(a0)
        move.w  #0,0xA15120         /* done with lower word */

        rts



decompress_lump:
        bsr     get_lump_source_and_size
        tst.l   lump_size
        beq.s   decompress_lump_done
        move.l  lump_ptr,d0
        andi.l  #0x7FFFF,d0
        addi.l  #0x880000,d0
        movea.l d0,a0
        moveq   #0,d0
        move.w  (a0)+,d0
        move.l  d0,lump_size
        lea     decomp_buffer,a1
        jmp     Kos_Decomp_Main
decompress_lump_done:
        rts     /* Kos_Decomp_Main will do an 'rts' for us. */



queue_register_write:
        move.w  0xA15122,register_write_queue

        move.w  #0,0xA15120         /* done */

        bra     main_loop



load_md_palettes:
        move.l  a0,-(sp)
        move.l  a1,-(sp)
        move.l  a2,-(sp)
        move.l  a3,-(sp)
        move.l  d0,-(sp)
        move.l  d1,-(sp)
        move.l  d2,-(sp)
        move.l  d3,-(sp)

        bsr     get_lump_source_and_size
1:
        move.w  0xA15120,d0         /* wait on handshake in COMM0 */
        cmpi.b  #0x05,d0
        bne.b   1b
        move.b  0xA15122,d2
        move.b  0xA15123,d3
        move.w  #0,0xA15120         /* done with lower word */

        btst    #0,d3
        bne.s   2f
        lea     bank1_palette_1,a3
        bra.s   3f
2:
        lea     bank2_palette_1,a3
3:

        lea     0xC00004,a0
        lea     0xC00000,a1
        move.w  #0x8F02,(a0)
        move.l  #0xC0000000,(a0)        /* Write CRAM address 0 */
        move.l  lump_ptr,d0
        andi.l  #0x7FFFF,d0
        addi.l  #0x880000,d0
        move.l  d0,a2
        move.l  lump_size,d1
        lsr.l   #1,d1
        sub.l   #1,d1

        btst    #0,d2                   /* Set CRAM to black? */
        beq.s   5f
        move.w  d1,d0
4:
        move.w  #0,(a1)                 /* Set color to blank in CRAM */
        dbra    d0,4b
5:
        move.w  (a2)+,(a3)+             /* Copy color from ROM to DRAM */
        dbra    d1,5b

        move.l  (sp)+,d2
        move.l  (sp)+,d1
        move.l  (sp)+,d0
        move.l  (sp)+,a3
        move.l  (sp)+,a2
        move.l  (sp)+,a1
        move.l  (sp)+,a0

        |move.w  #0,0xA15120         /* done */

        bra     main_loop



load_md_sky:
        |move.l  #224,test_end      /* TESTING */
        move.w  #0x2700,sr          /* disable ints */

        move.l  a0,-(sp)
        move.l  a1,-(sp)
        move.l  a2,-(sp)
        move.l  a3,-(sp)
        move.l  a4,-(sp)
        move.l  d0,-(sp)
        move.l  d1,-(sp)
        move.l  d2,-(sp)

        lea     0xC00004,a0
        lea     0xC00000,a1

        move.w  #0x8016,(a0) /* reg 0 = /IE1 (enable HBL INT), /M3 (enable read H/V cnt) */
        move.w  #0x8174,(a0) /* reg 1 = /DISP (display off), /IE0 (enable VBL INT), M1 (DMA enabled), /M2 (V28 mode) */
        ||move.w  #0x8230,(a0) /* reg 2 = Name Tbl A = 0xC000 */
        ||move.w  #0x8328,(a0) /* reg 3 = Name Tbl W = 0xA000 */
        ||move.w  #0x8407,(a0) /* reg 4 = Name Tbl B = 0xE000 */
        ||move.w  #0x8504,(a0) /* reg 5 = Sprite Attr Tbl = 0x0800 */
        ||move.w  #0x8C81,(a0) /* reg 12 = H40 mode, no lace, no shadow/hilite */
        ||move.w  #0x8D00,(a0) /* reg 13 = HScroll Tbl = 0x0000 */


        /* Load metadata */
        |bset.l  #0,(test_values+20)      /* TESTING */
        bsr     get_lump_source_and_size
        lea     0xC00004,a0
        move.l  lump_ptr,d0
        andi.l  #0x7FFFF,d0
        addi.l  #0x880002,d0    /* Plus 2 to skip the 32X thru color and dummy bytes */
        move.l  d0,a2

        move.w  #0x8C00,d0
        or.b    (a2)+,d0
        cmpi.b  #3,legacy_emulator      /* Check for Gens */
        bne.s   0f
        or.b    #0x0081,d0              /* Force Gens to use H40 */
0:
        cmpi.b  #1,legacy_emulator      /* Check for a legacy emulator (not Ares) */
        bgt.s   4f
        move.w  #0x8F02,(a0)
        lea     0xC00004,a0
        lea     0xC00000,a1
        moveq   #0,d2
        btst.b  #0,d0                   /* Which screen resolution will be used? */
        bne.s   2f

        move.l  #0x11111111,d2          /* Left-border sprite pixels */
        move.l  #0x48000000,(a0)        /* Write VRAM address 0x0800 */
        lea     h32_left_edge_sprites,a3
        move.w  #13,d1
1:
        move.l  (a3)+,(a1)              /* Copy eight pixels from the source */
        dbra    d1,1b
2:
        move.l  #0x4A000000,(a0)        /* Write VRAM address 0x0A00 */
        move.w  #31,d1
3:
        move.l  d2,(a1)                 /* Create left-border tiles, or erase them */
        dbra    d1,3b
4:
        |move.w  d0,(a0) /* reg 12 */
        move.w  d0,register_write_queue     /* Set register 12 during VBlank */

        move.w  #0x9000, d0
        or.b    (a2)+,d0
        move.w  d0,(a0) /* reg 16 */

        move.w  (a2)+,d0
        move.w  d0,scroll_b_vert_offset_top

        move.w  (a2)+,d0
        move.w  d0,scroll_b_vert_offset_bottom

        move.w  (a2)+,d0
        move.w  d0,scroll_a_vert_offset_top

        move.w  (a2)+,d0
        move.w  d0,scroll_a_vert_offset_bottom

        move.w  (a2)+,d0
        move.w  d0,scroll_b_vert_break

        move.w  (a2)+,d0
        move.w  d0,scroll_a_vert_break

        move.b  (a2)+,d1
        move.b  #16,d0
        sub.b   d1,d0
        move.b  d0,scroll_b_vert_rate_top

        move.b  (a2)+,d1
        move.b  #16,d0
        sub.b   d1,d0
        move.b  d0,scroll_b_vert_rate_bottom

        move.b  (a2)+,d1
        move.b  #16,d0
        sub.b   d1,d0
        move.b  d0,scroll_a_vert_rate_top

        move.b  (a2)+,d1
        move.b  #16,d0
        sub.b   d1,d0
        move.b  d0,scroll_a_vert_rate_bottom



        /* Load palettes */
        |bset.l  #0,(test_values+8)      /* TESTING */
        bsr     get_lump_source_and_size
        lea     0xC00004,a0
        lea     0xC00000,a1
        move.w  #0x8F02,(a0)
        move.l  #0xC0000000,(a0)        /* Write CRAM address 0 */
        move.l  lump_ptr,d0
        andi.l  #0x7FFFF,d0
        addi.l  #0x880000,d0
        move.l  d0,a2
        lea     bank1_palette_1,a3
        move.w  #47,d1
        cmpi.b  #1,legacy_emulator      /* Check for legacy emulator (not Ares) */
        bgt.s   1f

        moveq   #0,d0
        move.w  d0,(a3)+                /* Use black for the background (hardware H32-safe) */
        addq    #2,a2
        subq    #1,d1
        bra.s   2f
1:
        move.w  (a2),d0                 /* Use background color from palette lump (best for legacy emulators) */
2:
        move.w  (a2)+,(a3)+             /* Save color to DRAM */
        move.w  #0,(a1)                 /* Set color to black in CRAM */
        |add.l   #0x40000,a1            /* Advance the destination cursor */
        dbra    d1,2b

        move.w  d0,(a3)+                /* Set color 48 to match color 0 */
        move.w  d0,(a3)+                /* Set color 49 to match color 0 */

        /* Load patterns */
        |bset.l  #0,(test_values+12)      /* TESTING */
        bsr     decompress_lump
        lea     0xC00004,a0
        lea     0xC00000,a1
        move.w  #0x8F02,(a0)
        move.l  #0x4A800000,(a0)        /* Write VRAM address 0x0A80 */
        lea     decomp_buffer,a2
        move.l  lump_size,d1
        lsr.l   #1,d1
        sub.l   #1,d1
3:
        move.w  (a2)+,(a1)              /* Copy eight pixels from the source */
        dbra    d1,3b



        /* Load pattern name table B1 */
        |move.l  #0,(test_values+0)
        |bset.l  #0,(test_values+0)      /* TESTING */
        bsr     decompress_lump
        lea     0xC00004,a0
        lea     0xC00000,a1
        lea     decomp_buffer,a2
        move.w  #0x8F02,(a0)
        move.l  lump_size,d1
        cmpi.w  #0x2000,d1              /* Greater than 8KB? */
        bgt.s   1f

        | Scroll B is either 256 or 512 pixels wide
        |bset.l  #1,(test_values+0)      /* TESTING */
        move.l  #0x60000003,(a0)        /* Set destination offset to pattern name table B1 at position 0x0 */
        move.b  #0x30,scroll_a_address_register_values
        move.l  #0x07070707,scroll_b_address_register_values
        move.l  #0x60000003,write_scroll_b_table_1
        move.l  #0x60000003,write_scroll_b_table_2
        move.l  #0x60000003,write_scroll_b_table_3
        move.l  #0x60000003,write_scroll_b_table_4
        lsr.l   #1,d1
        sub.l   #1,d1
0:
        move.w  (a2)+,(a1)              /* Write 0 to pattern name table B1 at position 0x0 */
        dbra    d1,0b
        bra.w   9f
1:
        | Scroll B is 1024 pixels wide
        |bset.l  #2,(test_values+0)      /* TESTING */
        move.l  #0x40000002,(a0)
        move.l  #0x18181818,scroll_a_address_register_values
        move.l  #0x07060504,scroll_b_address_register_values
        move.l  #0x40000002,write_scroll_b_table_1
        move.l  #0x60000002,write_scroll_b_table_2
        move.l  #0x40000003,write_scroll_b_table_3
        move.l  #0x60000003,write_scroll_b_table_4

        move.l  a5,-(sp)
        lea     decomp_buffer+0x1000,a3
        lea     decomp_buffer+0x2000,a4
        lea     decomp_buffer+0x3000,a5

        | Table B1 (1/2 panel configuration)
        |bset.l  #3,(test_values+0)      /* TESTING */
        move.w  #63,d0
1:
        move.w  #31,d1
        move.w  d1,d2
0:
        move.w  (a2)+,(a1)              /* Write 0 to pattern name table B1 at position 0x0 */
        dbra    d1,0b
0:
        move.w  (a3)+,(a1)              /* Write 0 to pattern name table B1 at position 0x0 */
        dbra    d2,0b
        dbra    d0,1b

        suba    #0x1000,a2
        suba    #0x1000,a3

        | Table B2 (3/2 panel configuration)
        |bset.l  #4,(test_values+0)      /* TESTING */
        move.w  #63,d0
1:
        move.w  #31,d1
        move.w  d1,d2
0:
        move.w  (a4)+,(a1)              /* Write 0 to pattern name table B1 at position 0x0 */
        dbra    d1,0b
0:
        move.w  (a3)+,(a1)              /* Write 0 to pattern name table B1 at position 0x0 */
        dbra    d2,0b
        dbra    d0,1b

        suba    #0x1000,a3
        suba    #0x1000,a4

        | Table B3 (3/4 panel configuration)
        |bset.l  #5,(test_values+0)      /* TESTING */
        move.w  #63,d0
1:
        move.w  #31,d1
        move.w  d1,d2
0:
        move.w  (a4)+,(a1)              /* Write 0 to pattern name table B1 at position 0x0 */
        dbra    d1,0b
0:
        move.w  (a5)+,(a1)              /* Write 0 to pattern name table B1 at position 0x0 */
        dbra    d2,0b
        dbra    d0,1b

        suba    #0x1000,a4
        suba    #0x1000,a5

        | Table B4 (1/4 panel configuration)
        |bset.l  #6,(test_values+0)      /* TESTING */
        move.w  #63,d0
1:
        move.w  #31,d1
        move.w  d1,d2
0:
        move.w  (a2)+,(a1)              /* Write 0 to pattern name table B1 at position 0x0 */
        dbra    d1,0b
0:
        move.w  (a5)+,(a1)              /* Write 0 to pattern name table B1 at position 0x0 */
        dbra    d2,0b
        dbra    d0,1b

        move.l  (sp)+,a5
9:


        /* Load pattern name table A1 */
        |move.l  #0,(test_values+4)
        |bset.l  #0,(test_values+4)      /* TESTING */
        bsr     decompress_lump
        lea     0xC00004,a0
        lea     0xC00000,a1
        lea     decomp_buffer,a2
        move.w  #0x8F02,(a0)
        move.l  lump_size,d1
        cmpi.w  #0x2000,d1              /* Greater than 8KB? */
        bgt.w   1f
        cmpi.b  #0x30,scroll_a_address_register_values
        beq.s   0f

        | Scroll A is either 256 or 512 pixels wide
        | Scroll B is either 256 or 512 pixels wide
        |bset.l  #1,(test_values+4)      /* TESTING */
        move.l  #0x40000003,(a0)        /* Set destination offset to pattern name table A1 at position 0x0 */
        move.l  #0x30303030,scroll_a_address_register_values
        move.l  #0x40000003,write_scroll_a_table_1
        move.l  #0x40000003,write_scroll_a_table_2
        move.l  #0x40000003,write_scroll_a_table_3
        move.l  #0x40000003,write_scroll_a_table_4
        bra.w   2f
0:
        | Scroll A is either 256 or 512 pixels wide
        | Scroll B is 1024 pixels wide
        |bset.l  #2,(test_values+4)      /* TESTING */
        move.l  #0x60000001,(a0)
        move.l  #0x18181818,scroll_a_address_register_values
        move.l  #0x60000001,write_scroll_a_table_1
        move.l  #0x60000001,write_scroll_a_table_2
        move.l  #0x60000001,write_scroll_a_table_3
        move.l  #0x60000001,write_scroll_a_table_4
        bra.w   2f
1:
        | Scroll A is 1024 pixels wide
        |bset.l  #3,(test_values+4)      /* TESTING */
        move.l  #0x60000001,(a0)
        move.l  #0x30282018,scroll_a_address_register_values
        move.l  #0x60000001,write_scroll_a_table_1
        move.l  #0x40000002,write_scroll_a_table_2
        move.l  #0x60000002,write_scroll_a_table_3
        move.l  #0x40000003,write_scroll_a_table_4

        move.l  a5,-(sp)
        lea     decomp_buffer+0x1000,a3
        lea     decomp_buffer+0x2000,a4
        lea     decomp_buffer+0x3000,a5

        | Table A1 (1/2 panel configuration)
        |bset.l  #4,(test_values+4)      /* TESTING */
        move.w  #63,d0
1:
        move.w  #31,d1
        move.w  d1,d2
0:
        move.w  (a2)+,(a1)              /* Write 0 to pattern name table B1 at position 0x0 */
        dbra    d1,0b
0:
        move.w  (a3)+,(a1)              /* Write 0 to pattern name table B1 at position 0x0 */
        dbra    d2,0b
        dbra    d0,1b

        suba    #0x1000,a2
        suba    #0x1000,a3

        | Table A2 (3/2 panel configuration)
        |bset.l  #5,(test_values+4)      /* TESTING */
        move.w  #63,d0
1:
        move.w  #31,d1
        move.w  d1,d2
0:
        move.w  (a4)+,(a1)              /* Write 0 to pattern name table B1 at position 0x0 */
        dbra    d1,0b
0:
        move.w  (a3)+,(a1)              /* Write 0 to pattern name table B1 at position 0x0 */
        dbra    d2,0b
        dbra    d0,1b

        suba    #0x1000,a3
        suba    #0x1000,a4

        | Table A3 (3/4 panel configuration)
        |bset.l  #6,(test_values+4)      /* TESTING */
        move.w  #63,d0
1:
        move.w  #31,d1
        move.w  d1,d2
0:
        move.w  (a4)+,(a1)              /* Write 0 to pattern name table B1 at position 0x0 */
        dbra    d1,0b
0:
        move.w  (a5)+,(a1)              /* Write 0 to pattern name table B1 at position 0x0 */
        dbra    d2,0b
        dbra    d0,1b

        suba    #0x1000,a4
        suba    #0x1000,a5

        | Table A4 (1/4 panel configuration)
        |bset.l  #7,(test_values+4)      /* TESTING */
        move.w  #63,d0
1:
        move.w  #31,d1
        move.w  d1,d2
0:
        move.w  (a2)+,(a1)              /* Write 0 to pattern name table B1 at position 0x0 */
        dbra    d1,0b
0:
        move.w  (a5)+,(a1)              /* Write 0 to pattern name table B1 at position 0x0 */
        dbra    d2,0b
        dbra    d0,1b

        suba    #0x1000,a2
        suba    #0x1000,a5

        move.l  (sp)+,a5
        bra.s   9f
2:
        lsr.l   #1,d1
        sub.l   #1,d1
0:
        move.w  (a2)+,(a1)              /* Write 0 to pattern name table A1 at position 0x0 */
        dbra    d1,0b
9:
        lea     0xC00004,a0
        lea     0xC00000,a1

        move.w  #0x8200,d0
        move.b  scroll_a_address_register_values_1,d0
        move.w  d0,(a0) /* reg 2 = Name Tbl A */

        move.w  #0x8400,d0
        move.b  scroll_b_address_register_values_1,d0
        move.w  d0,(a0) /* reg 4 = Name Tbl B */



        move.l  (sp)+,d2
        move.l  (sp)+,d1
        move.l  (sp)+,d0
        move.l  (sp)+,a4
        move.l  (sp)+,a3
        move.l  (sp)+,a2
        move.l  (sp)+,a1
        move.l  (sp)+,a0

        move.w  #0,0xA15120         /* done */

        move.w  #0x2000,sr          /* enable ints */

        bra     main_loop


set_scroll_positions:
        move.l  d0,-(sp)

        move.w  0xA15122,current_scroll_b_top_y

        move.w  #0,0xA15120         /* request more data */
1:
        move.b  0xA15121,d0         /* wait on handshake in COMM0 */
        cmpi.b  #0x02,d0
        bne.b   1b

        move.w  0xA15122,current_scroll_b_bottom_y

        move.w  #0,0xA15120         /* request more data */
2:
        move.b  0xA15121,d0         /* wait on handshake in COMM0 */
        cmpi.b  #0x03,d0
        bne.b   2b

        move.w  0xA15122,current_scroll_a_top_y

        move.w  #0,0xA15120         /* request more data */
3:
        move.b  0xA15121,d0         /* wait on handshake in COMM0 */
        cmpi.b  #0x04,d0
        bne.b   3b

        move.w  0xA15122,current_scroll_a_bottom_y

        move.l  (sp)+,d0

        move.w  #0,0xA15120         /* done */

        bra     main_loop


scroll_md_sky:
        move.l  a0,-(sp)
        move.l  a1,-(sp)
        move.l  d0,-(sp)
        move.l  d1,-(sp)
        move.l  d2,-(sp)
        move.l  d3,-(sp)
        move.l  d4,-(sp)
        move.l  d5,-(sp)
        move.l  d6,-(sp)
        move.l  d7,-(sp)

        lea     0xC00004,a0
        lea     0xC00000,a1

        /* Vertical */
        move.l  #0x40000010,(a0)
        moveq   #0,d0
        moveq   #0,d1
        moveq   #0,d2
        moveq   #0,d3
        moveq   #0,d4
        moveq   #0,d5
        moveq   #0,d6
        moveq   #0,d7

        move.w  0xA15122,d3         /* scroll_y_base */

        move.w  #0,0xA15120         /* request more data */
2:
        move.b  0xA15121,d0         /* wait on handshake in COMM0 */
        cmpi.b  #0x02,d0
        bne.b   2b
        move.w  0xA15122,d4         /* scroll_y_offset */
        
        move.w  #0,0xA15120         /* request more data */
3:
        move.b  0xA15121,d0         /* wait on handshake in COMM0 */
        cmpi.b  #0x03,d0
        bne.b   3b
        move.w  0xA15122,d2         /* scroll_y_pan */

        moveq   #0,d1
        move.b  scroll_b_vert_rate_top,d1
        move.w  d4,d5               /* scroll_y_offset */
        lsr.w   d1,d5
        move.w  scroll_b_vert_offset_top,d1
        sub.w   d3,d1               /* scroll_y_base */
        sub.w   d2,d1               /* scroll_y_pan */
        sub.w   d5,d1               /* position = (scroll_y_base - d5 - scroll_y_pan) & 0x3FF */
        |andi.w  #0x3FF,d1
        move.w  d1,current_scroll_b_top_y

        moveq   #0,d1
        move.b  scroll_b_vert_rate_bottom,d1
        move.w  d4,d5               /* scroll_y_offset */
        lsr.w   d1,d5
        move.w  scroll_b_vert_offset_bottom,d1
        sub.w   d3,d1               /* scroll_y_base */
        sub.w   d2,d1               /* scroll_y_pan */
        sub.w   d5,d1               /* position = (scroll_y_base - d5 - scroll_y_pan) & 0x3FF */
        |andi.w  #0x3FF,d1
        move.w  d1,current_scroll_b_bottom_y

        moveq   #0,d1
        move.b  scroll_a_vert_rate_top,d1
        move.w  d4,d5               /* scroll_y_offset */
        lsr.w   d1,d5
        move.w  scroll_a_vert_offset_top,d1
        sub.w   d3,d1               /* scroll_y_base */
        sub.w   d2,d1               /* scroll_y_pan */
        sub.w   d5,d1               /* position = (scroll_y_base - d5 - scroll_y_pan) & 0x3FF */
        |andi.w  #0x3FF,d1
        move.w  d1,current_scroll_a_top_y

        moveq   #0,d1
        move.b  scroll_a_vert_rate_bottom,d1
        move.w  d4,d5               /* scroll_y_offset */
        lsr.w   d1,d5
        move.w  scroll_a_vert_offset_bottom,d1
        sub.w   d3,d1               /* scroll_y_base */
        sub.w   d2,d1               /* scroll_y_pan */
        sub.w   d5,d1               /* position = (scroll_y_base - d5 - scroll_y_pan) & 0x3FF */
        |andi.w  #0x3FF,d1
        move.w  d1,current_scroll_a_bottom_y

        move.w  #0,0xA15120         /* done with vertical scroll */
4:
        move.b  0xA15121,d0         /* wait on handshake in COMM0 */
        cmpi.b  #0x04,d0
        bne.b   4b

        /* Horizontal */
        move.l  write_horizontal_scroll_table,d3
        moveq   #0,d0
        move.w  0xA15122,d0         /* scroll_x */
        move.w  d0,current_scroll_b_top_x
        move.w  d0,current_scroll_b_bottom_x
        move.w  d0,current_scroll_a_top_x
        move.w  d0,current_scroll_a_bottom_x
        |move.w  d0,d1
        |swap    d0
        |or.w    d1,d0

        move.l  #448,d2             /* Update each line, alternating between Scroll A and Scroll B */
0:
        move.l  d3,(a0)
        |move.l  d0,(a1)
        move.w  d0,(a1)
        add.l   #0x20000,d3
        subq    #1,d2
        cmp.w   #0,d2
        bne.b   0b

        lsr.w   #8,d0
        andi.w  #3,d0

        move.w  #0x8200,d1
        lea     scroll_a_address_register_values,a1
        adda.w  d0,a1
        move.b  (a1),d1
        move.w  d1,(a0) /* reg 2 = Name Tbl A */

        move.w  #0x8400,d1
        lea     scroll_b_address_register_values,a1
        adda.w  d0,a1
        move.b  (a1),d1
        move.w  d1,(a0) /* reg 4 = Name Tbl B */

        move.l  (sp)+,d7
        move.l  (sp)+,d6
        move.l  (sp)+,d5
        move.l  (sp)+,d4
        move.l  (sp)+,d3
        move.l  (sp)+,d2
        move.l  (sp)+,d1
        move.l  (sp)+,d0
        move.l  (sp)+,a1
        move.l  (sp)+,a0

        move.w  #0,0xA15120         /* done */

        bra     main_loop


crossfade_md_palette:
        move.l  a0,-(sp)
        move.l  a1,-(sp)
        move.l  a3,-(sp)
        move.l  a4,-(sp)
        move.l  a5,-(sp)
        move.l  d0,-(sp)
        move.l  d1,-(sp)
        move.l  d2,-(sp)
        move.l  d3,-(sp)
        move.l  d4,-(sp)
        move.l  d5,-(sp)
        move.l  d6,-(sp)
        move.l  d7,-(sp)

        /* Fade palette */
        lea     0xC00004,a0
        lea     0xC00000,a1
        move.w  #0x8F02,(a0)
        move.l  #0xC0000000,(a0)        /* Write CRAM address 0 */

        lea     bank1_palette_1,a3      /* Point A3 to source palette */
        lea     bank2_palette_1,a4      /* Point A4 to target palette */
        move.w  0xA15122,d2             /* Get fade degree */

        btst.b  #4,d2
        beq.s   2f

        /* Copy bank2 to bank1 and load entire bank into CRAM */
        moveq   #0x20-1,d0
0:
        move.l  (a4)+,(a3)+             /* Copy palette bank2 to bank1 */
        dbra    d0,0b

        moveq   #0x40-1,d0
1:
        move.w  (a3)+,(a1)              /* Load entire bank into CRAM */
        dbra    d0,1b

        bra.w   crossfade_done

        /* Perform cross fade between bank1 and bank2 */
2:
        lea     crossfade_lookup,a5     /* Point A5 to crossfade lookup table */
        moveq   #0,d4
        moveq   #0,d5
        moveq   #0,d6

        move.w  #0x40-1,d7              /* Prepare to copy all 64 colors */


        | D0 = CRAM value (source)
        | D1 = CRAM value (target)

        | D2 = Fade value

        | D3 = CRAM color channel (source)
        | D4 = CRAM color channel (target) / Color delta

        | D5 = Resulting color

        | A1 = CRAM palette
        | A3 = DRAM palette (source)
        | A4 = DRAM palette (target)
        | A5 = Crossfade lookup

crossfade_color:
        move.w  (a3)+,d0        /* Copy color from source palette to D0 */
        move.w  (a4)+,d1        /* Copy color from target palette to D1 */

crossfade_red:
        /* RED */
        move.b  d0,d3           /* Get source red */
        andi.w  #0xE,d3         /* Remove green from this copy */
        move.b  d1,d4           /* Get target red */
        andi.w  #0xE,d4         /* Remove green from this copy */

        /* Get total delta */
        sub.b   d3,d4
        cmp.b   #0,d4
        bge.s   3f
        neg.b   d4

        /* Find interval delta */
        | D2 = Phase (x)
        | D4 = Delta (y)
        lsl.b   #3,d4
        add.b   d2,d4
        move.b  (a5,d4.w),d6    /* Store new red in D6 */
        bra.s   crossfade_green
3:
        /* Find interval delta */
        | D2 = Phase (x)
        | D4 = Delta (y)
        lsl.b   #3,d4
        add.b   d2,d4
        move.b  (a5,d4.w),d5    /* Store new red in D5 */

crossfade_green:
        /* GREEN */
        move.w  d0,d3           /* Get source green */
        andi.w  #0xE0,d3        /* Remove green from this copy */
        move.w  d1,d4           /* Get target green */
        andi.w  #0xE0,d4        /* Remove green from this copy */

        /* Get total delta */
        sub.w   d3,d4
        cmp.w   #0,d4
        bge.s   4f
        neg.w   d4

        /* Find interval delta*/
        | D2 = Phase (x)
        | D4 = Delta (y)
        lsr.w   #1,d4
        add.b   d2,d4
        move.b  (a5,d4.w),d4
        lsl.w   #4,d4
        or.w    d4,d6           /* Store new green in D6 */
        bra.s   crossfade_blue
4:
        /* Find interval delta*/
        | D2 = Phase (x)
        | D4 = Delta (y)
        lsr.w   #1,d4
        add.b   d2,d4
        move.b  (a5,d4.w),d4
        lsl.w   #4,d4
        or.w    d4,d5           /* Store new green in D5 */

crossfade_blue:
        /* BLUE */
        move.w  d0,d3           /* Get source blue */
        andi.w  #0xE00,d3       /* Remove blue from this copy */
        move.w  d1,d4           /* Get target blue */
        andi.w  #0xE00,d4       /* Remove blue from this copy */

        /* Get total delta */
        sub.w   d3,d4
        cmp.w   #0,d4
        bge.s   5f
        neg.w   d4

        /* Find interval delta */
        | D2 = Phase (x)
        | D4 = Delta (y)
        lsr.w   #5,d4
        add.b   d2,d4
        move.b  (a5,d4.w),d4
        lsl.w   #8,d4
        or.w    d4,d6           /* Store new blue in D6 */
        bra.s   crossfade_update_color
5:
        /* Find interval delta */
        | D2 = Phase (x)
        | D4 = Delta (y)
        lsr.w   #5,d4
        add.b   d2,d4
        move.b  (a5,d4.w),d4
        lsl.w   #8,d4
        or.w    d4,d5           /* Store new blue in D5 */

crossfade_update_color:
        add.w   d5,d0
        sub.w   d6,d0
        move.w  d0,(a1)         /* Copy new color in D0 to CRAM */
        dbra    d7,crossfade_color   /* Continue the loop if more colors need to be updated. */

crossfade_done:
        move.l  (sp)+,d7
        move.l  (sp)+,d6
        move.l  (sp)+,d5
        move.l  (sp)+,d4
        move.l  (sp)+,d3
        move.l  (sp)+,d2
        move.l  (sp)+,d1
        move.l  (sp)+,d0
        move.l  (sp)+,a5
        move.l  (sp)+,a4
        move.l  (sp)+,a3
        move.l  (sp)+,a1
        move.l  (sp)+,a0

        move.w  #0,0xA15120         /* done */

        bra     main_loop


fade_md_palette:
        move.l  a0,-(sp)
        move.l  a1,-(sp)
        move.l  a3,-(sp)
        move.l  d0,-(sp)
        move.l  d1,-(sp)
        move.l  d2,-(sp)
        move.l  d3,-(sp)
        move.l  d4,-(sp)

        /* Fade palette */
        lea     0xC00004,a0
        lea     0xC00000,a1
        move.w  #0x8F02,(a0)
        move.l  #0xC0000000,(a0)        /* Write CRAM address 0 */

        lea     bank1_palette_1,a3      /* Point A3 to DRAM palette */
        move.w  0xA15122,d1             /* Get fade degree */

        move.w  #0x40-1,d4              /* Prepare to copy all 64 colors */


        | D0 = CRAM value
        | D1 = Fade value

        | D2 = CRAM color channel
        | D3 = Fade color channel

        | A1 = CRAM palette
        | A3 = DRAM palette

fade_color:
        move.w  (a3)+,d0        /* Copy color from DRAM to D0 */

fade_red:
        /* RED */
        move.b  d0,d2           /* Get CRAM red */
        andi.b  #0xE,d2         /* Remove CRAM green from this copy */
        move.b  d1,d3           /* Get fade red */
        andi.b  #0xE,d3         /* Remove fade green from this copy */

        cmp.b   d3,d2           /* Compare the two reds */
        bls     fade_green      /* Branch if fade red is less than or equal to CRAM red */
        andi.b  #0xE0,d0        /* Remove the current red */
        or.b    d3,d0           /* Replace with the new red */

fade_green:
        /* GREEN */
        move.b  d0,d2           /* Get CRAM green */
        andi.b  #0xE0,d2        /* Remove CRAM red from this copy */
        move.b  d1,d3           /* Get fade green */
        andi.b  #0xE0,d3        /* Remove fade red from this copy */

        cmp.b   d3,d2           /* Compare the two greens */
        bls     fade_blue       /* Branch if fade green is less than or equal to CRAM green */
        andi.b  #0x0E,d0        /* Remove the current green */
        or.b    d3,d0           /* Replace with the new green */

fade_blue:
        /* BLUE */
        move.w  d0,d2           /* Get CRAM blue */
        andi.w  #0xE00,d2       /* Remove CRAM red and green from this copy */
        move.w  d1,d3           /* Get fade blue */
        andi.w  #0xE00,d3       /* Remove fade red and green from this copy */

        cmp.w   d3,d2           /* Compare the two blues */
        bls     update_color    /* Branch if fade blue is less than or equal to CRAM blue */
        andi.w  #0x0EE,d0       /* Remove the current blue */
        or.w    d3,d0           /* Replace with the new blue */

update_color:
        move.w  d0,(a1)         /* Copy new color in D0 to CRAM */
        dbra    d4,fade_color   /* Continue the loop if more colors need to be updated. */

        move.l  (sp)+,d4
        move.l  (sp)+,d3
        move.l  (sp)+,d2
        move.l  (sp)+,d1
        move.l  (sp)+,d0
        move.l  (sp)+,a3
        move.l  (sp)+,a1
        move.l  (sp)+,a0

        move.w  #0,0xA15120         /* done */

        bra     main_loop



load_sfx:
        /* fetch sample length */
        move.w  0xA15122,d2
        swap.w  d2
        move.w  #0,0xA15120         /* done with upper word */
20:
        move.w  0xA15120,d0         /* wait on handshake in COMM0 */
        cmpi.w  #0x1D02,d0
        bne.b   20b
        move.w  0xA15122,d2
        move.w  #0,0xA15120         /* done with lower word */
21:
        move.w  0xA15120,d0         /* wait on handshake in COMM0 */
        cmpi.w  #0x1D03,d0
        bne.b   21b

        /* fetch sample address */
        move.w  0xA15122,d1
        swap.w  d1
        move.w  #0,0xA15120         /* done with upper word */
22:
        move.w  0xA15120,d0         /* wait on handshake in COMM0 */
        cmpi.w  #0x1D04,d0
        bne.b   22b
        move.w  0xA15122,d1
        move.w  #0,0xA15120         /* done with lower word */

23:
        move.w  0xA15120,d0         /* wait on handshake in COMM0 */
        cmpi.w  #0x1D00,d0
        bne.b   23b

        /* set buffer id */
        moveq   #0,d0
        move.w  0xA15122,d0         /* COMM2 = buffer id */

        move.l  d1,a0
        bsr     set_rom_bank
        move.l  a1,d1

        move.l  d2,-(sp)
        move.l  d1,-(sp)
        move.l  d0,-(sp)

        move.w  #0,0xA15120         /* done */

        jsr     scd_upload_buf
        lea     12(sp),sp

        bra     main_loop

play_sfx:
        /* set source id */
        moveq   #0,d0
        move.b  0xA15121,d0         /* LB of COMM0 = src id */

        moveq   #0,d2
        move.w  0xA15122,d2         /* pan|vol */
        move.w  #0,0xA15120
20:
        move.w  0xA15120,d1         /* wait on handshake in COMM0 */
        cmpi.w  #0x1E01,d1
        bne.b   20b

        moveq   #0,d1
        move.w  0xA15122,d1         /* buf_id */

        move.w  d2,d3
        andi.l  #255,d3
        lsr.w   #8,d2

        move.l  #0,-(sp)            /* autoloop */
        move.l  d3,-(sp)            /* vol */
        move.l  d2,-(sp)            /* pan */ 
        move.l  #0,-(sp)            /* freq */
        move.l  d1,-(sp)            /* buf id */
        move.l  d0,-(sp)            /* src id */

        move.w  #0,0xA15120         /* done */

        jsr     scd_queue_play_src
        lea     24(sp),sp

        bra     main_loop

get_sfx_status:
        jsr     scd_get_playback_status

        move.w  d0,0xA15122         /* state */
        move.w  #0,0xA15120         /* done */

        bra     main_loop

sfx_clear:
        jsr     scd_queue_clear_pcm

        move.w  #0,0xA15120         /* done */

        bra     main_loop

update_sfx:
        /* set source id */
        moveq   #0,d0
        move.b  0xA15121,d0         /* LB of COMM0 = src id */

        moveq   #0,d2
        move.w  0xA15122,d2         /* pan|vol */

        move.w  d2,d3
        andi.l  #255,d3
        lsr.w   #8,d2

        move.l  #0,-(sp)            /* autoloop */
        move.l  d3,-(sp)            /* vol */
        move.l  d2,-(sp)            /* pan */ 
        move.l  #0,-(sp)            /* freq */
        move.l  d0,-(sp)            /* src id */

        move.w  #0,0xA15120         /* done */

        jsr     scd_queue_update_src
        lea     20(sp),sp

        bra     main_loop

stop_sfx:
        /* set source id */
        moveq   #0,d0
        move.b  0xA15121,d0         /* LB of COMM0 = src id */

        move.w  #0,0xA15120         /* done */

        move.l  d0,-(sp)            /* src id */
        jsr     scd_queue_stop_src
        lea     4(sp),sp

        bra     main_loop

flush_sfx:
        move.w  #0,0xA15120         /* done */

        jsr     scd_flush_cmd_queue

        move.b  #1,need_bump_fm
        bra     main_loop

| set standard mapper registers to default mapping

reset_banks:
        move.w  #0x2700,sr          /* disable ints */
        move.b  #1,0xA15107         /* set RV */
        move.b  #1,0xA130F3         /* bank for 0x080000-0x0FFFFF */
        move.b  #2,0xA130F5         /* bank for 0x100000-0x17FFFF */
        move.b  #3,0xA130F7         /* bank for 0x180000-0x1FFFFF */
        move.b  #4,0xA130F9         /* bank for 0x200000-0x27FFFF */
        move.b  #5,0xA130FB         /* bank for 0x280000-0x2FFFFF */
        move.b  #6,0xA130FD         /* bank for 0x300000-0x37FFFF */
        move.b  #7,0xA130FF         /* bank for 0x380000-0x3FFFFF */
        move.b  #0,0xA15107         /* clear RV */
        move.w  #0x2000,sr          /* enable ints */
        rts


| init MD VDP

init_vdp:
        move.w  d0,d2
        lea     0xC00004,a0
        lea     0xC00000,a1
        move.w  #0x8004,(a0) /* reg 0 = /IE1 (no HBL INT), /M3 (enable read H/V cnt) */
        move.w  #0x8114,(a0) /* reg 1 = /DISP (display off), /IE0 (no VBL INT), M1 (DMA enabled), /M2 (V28 mode) */
        move.w  #0x8230,(a0) /* reg 2 = Name Tbl A = 0xC000 */
        move.w  #0x8328,(a0) /* reg 3 = Name Tbl W = 0xA000 */
        move.w  #0x8407,(a0) /* reg 4 = Name Tbl B = 0xE000 */
        move.w  #0x8504,(a0) /* reg 5 = Sprite Attr Tbl = 0x0800 */
        move.w  #0x8600,(a0) /* reg 6 = always 0 */
        move.w  #0x8700,(a0) /* reg 7 = BG color */
        move.w  #0x8800,(a0) /* reg 8 = always 0 */
        move.w  #0x8900,(a0) /* reg 9 = always 0 */
        move.w  #0x8A00,(a0) /* reg 10 = HINT = 0 */
        move.w  #0x8B00,(a0) /* reg 11 = /IE2 (no EXT INT), full scroll */
        |move.w  #0x8B03,(a0) /* reg 11 = /IE2 (no EXT INT), line scroll */
        move.w  #0x8C81,(a0) /* reg 12 = H40 mode, no lace, no shadow/hilite */
        move.w  #0x8D00,(a0) /* reg 13 = HScroll Tbl = 0x0000 */
        move.w  #0x8E00,(a0) /* reg 14 = always 0 */
        move.w  #0x8F01,(a0) /* reg 15 = data INC = 1 */
        move.w  #0x9010,(a0) /* reg 16 = Scroll Size = 32x64 */
        move.w  #0x9100,(a0) /* reg 17 = W Pos H = left */
        move.w  #0x9200,(a0) /* reg 18 = W Pos V = top */

| clear VRAM
        move.w  #0x8F02,(a0)            /* set INC to 2 */
        move.l  #0x40000000,(a0)        /* write VRAM address 0 */
        moveq   #0,d0
        move.w  #0x07FF,d1              /* 2K - 1 tiles */
        tst.w   d2
        beq.b   0f
        moveq   #0,d1                   /* 1 tile */
0:
        move.l  d0,(a1)                 /* clear VRAM */
        move.l  d0,(a1)
        move.l  d0,(a1)
        move.l  d0,(a1)
        move.l  d0,(a1)
        move.l  d0,(a1)
        move.l  d0,(a1)
        move.l  d0,(a1)
        dbra    d1,0b

        tst.w   d2
        beq.b   9f

        move.l  write_sprite_attribute_table,(a0)   /* write VRAM address 0x0800 */
        moveq   #0,d0
        move.w  #0x02BF,d1              /* 704 - 1 tiles */
8:
        move.l  d0,(a1)                 /* clear VRAM */
        move.l  d0,(a1)
        move.l  d0,(a1)
        move.l  d0,(a1)
        move.l  d0,(a1)
        move.l  d0,(a1)
        move.l  d0,(a1)
        move.l  d0,(a1)
        dbra    d1,8b
        bra.b   3f

| The VDP state at this point is: Display disabled, ints disabled, Name Tbl A at 0xC000,
| Name Tbl B at 0xE000, Sprite Attr Tbl at 0x0800, HScroll Tbl at 0x0000, H40 V28 mode,
| and Scroll size is 64x32.

9:
| Clear CRAM
        move.l  #0x81048F02,(a0)        /* set reg 1 and reg 15 */
        move.l  #0xC0000000,(a0)        /* write CRAM address 0 */
        moveq   #31,d1
1:
        move.l  d0,(a1)
        dbra    d1,1b

| Clear VSRAM
        move.l  #0x40000010,(a0)         /* write VSRAM address 0 */
        moveq   #19,d1
2:
        move.l  d0,(a1)
        dbra    d1,2b

| set the default palette for text
        move.l  #0xC0000000,(a0)        /* write CRAM address 0 */
        move.l  #0x00000000,(a1)        /* set background color to black */
3:
        move.w  #0x8174,0xC00004        /* display on, vblank enabled, V28 mode */
        rts


| load font tile data

load_font:
        |lea     0xC00004,a0         /* VDP cmd/sts reg */
        |lea     0xC00000,a1         /* VDP data reg */
        |move.w  #0x8F02,(a0)        /* INC = 2 */
        |move.l  #0x40000000,(a0)    /* write VRAM address 0 */
        |lea     font_data,a2
        |move.w  #0x6B*8-1,d2
0:
        |move.l  (a2)+,d0            /* font fg mask */
        |move.l  d0,d1
        |not.l   d1                  /* font bg mask */
        |and.l   fg_color,d0         /* set font fg color */
        |and.l   bg_color,d1         /* set font bg color */
        |or.l    d1,d0
        |move.l  d0,(a1)             /* set tile line */
        |dbra    d2,0b
        |rts


| Bump the FM player to keep the music going

        .global bump_fm
bump_fm:
        tst.w   fm_idx
        beq.s   999f

        |move.w  sr,-(sp)
        |move.w  #0x2700,sr          /* disable ints */
        tst.b   REQ_ACT.w
        bmi.b   99f                 /* sram locked */
        bne.b   0f                  /* Z80 requesting action */
        tst.w   preread_cnt
        bne.b   0f                  /* buffer preread pending */
99:
        |move.w  (sp)+,sr            /* restore int level */
999:
        rts

0:
        movem.l d0-d7/a0-a6,-(sp)
        move.w  #0x0100,0xA11100    /* Z80 assert bus request */
        move.w  #0x0100,0xA11200    /* Z80 deassert reset */

        moveq   #0x05,d1            /* VGM 0x95 command */
        moveq   #0x01,d2            /* VGM buffer block */
1:
        move.w  0xA11100,d0
        andi.w  #0x0100,d0
        bne.b   1b                  /* wait for bus acquired */

        z80rd   CRITICAL,d0
        bne     8f                  /* z80 critical section, just exit */

10:
        move.b  REQ_ACT.w,d0
        cmp.b   d1,d0               /* Check for 0x95 command */
        beq.s   play_drum_sound
        cmp.b   d2,d0
        bne.w   5f                  /* not read buffer block */

play_drum_sound:
11:
        /* check for space in Z80 sram buffer */
        lea     STRM_DRUMI.w,a0     | 8
        move.b  (a0),d0             | 8     /* D0 = drum sample ID */
        tst.b   d0                  | 4     /* is the drum sample ID == 0? */
        beq.s   20f                 | 10/8
        move.b  #0,(a0)+            | 12    /* clear the drum sample ID from RAM */
        move.w  (a0)+,d1            | 8     /* D1 = drum volume and panning */

        lea     0xA1512C,a1         | 8
        move.l  a1,a2               | 4
191:
        tst.b   (a2)                | 8
        bne.s   191b                | 10/8  /* wait for CMD interrupt to finish */
192:
        move.b  d2,(a1)+            | 8     /* queue sound playback on COMM12 */
        move.b  d0,(a1)+            | 8     /* pass drum sample ID to COMM13 */
        move.w  d1,(a1)+            | 8     /* pass drum sample volume and panning to COMM14+15 */
        move.b  d2,0xA15103         | 16    /* call CMD interrupt on master SH-2 */
193:
        tst.b   (a2)                | 8
        bne.s   193b                | 10/8  /* wait for CMD interrupt to finish */
20:
        moveq   #0,d0
        z80rd   FM_BUFGEN,d0
        moveq   #0,d1
        z80rd   FM_BUFCSM,d1
        sub.w   d1,d0
        bge.b   111f
        addi.w  #256,d0             /* how far ahead the decompressor is */
111:
        cmpi.w  #8,d0
        bge.w   7f                  /* no space in circular buffer */
        moveq   #8,d1
        sub.w   d0,d1
        ble.w   7f                  /* sanity check */
        subq.w  #1,d1
        move.w  d1,preread_cnt      /* try to fill buffer */

        /* read a buffer block */
        jsr     vgm_read
        move.w  d0,d4
        move.w  offs68k,d2
        move.w  offsz80,d3
        cmpi.w  #0,d4
        beq.b   13f                 /* EOF with no more data */

        /* read returned some or all requested data */
        z80rd   FM_BUFGEN,d0
        addq.b  #1,d0
        z80wr   FM_BUFGEN,d0

        /* copy buffer to z80 sram */
        movea.l vgm_ptr,a3
        lea     0xA01000,a4
        move.w  d4,d1
        subi.w  #1,d1
        lea     0(a3,d2.w),a3
        lea     0(a4,d3.w),a4
12:
        move.b  (a3)+,(a4)+
        dbra    d1,12b

        addi.w  #512,d2
        addi.w  #512,d3
        andi.w  #0x1FFF,d2          /* 8KB buffer */
        andi.w  #0x0FFF,d3          /* 4KB buffer */

        cmpi.w  #512,d4
        beq.b   14f                 /* got entire block, not EOF */
13:
        /* EOF reached */
        move.l  fm_loop,d0
        andi.w  #0x01FF,d0          /* 1 read buffer */
        add.w   d3,d0
        andi.w  #0x0FFF,d0          /* 4KB buffer */
        move.w  d0,fm_loop2         /* offset to z80 loop start */

        /* reset */
        jsr     vgm_reset           /* restart at start of compressed data */

        move.l  fm_loop,d2
        andi.l  #0xFFFFFE00,d2
        beq.b   14f

        move.l  d2,-(sp)
        jsr     vgm_read2
        addq.l  #4,sp
        move.w  d0,d2
        andi.w  #0x1FFF,d2          /* amount data pre-read (with wrap around) */
14:
        move.w  d2,offs68k
        move.w  d3,offsz80
        move.w  #7,preread_cnt      /* refill buffer if room */
        bra.w   20b

5:
        cmpi.b  #0x02,d0
        bne.b   6f                  /* unknown request, ignore */

        /* music ended, check for loop */
        tst.w   fm_rep
        bne.b   50f                 /* repeat, loop vgm */
        /* no repeat, reset FM */
        jsr     rst_ym2612
        bra.b   7f
50:
        move.w  fm_loop2,d0
        z80wr   FM_START,d0
        lsr.w   #8,d0
        z80wr   FM_START+1,d0       /* music start = loop offset */
        move.w  fm_idx,d0
        z80wr   FM_IDX,d0           /* set FM_IDX to start music */
        bra.w   20b                 /* try to load a block */

6:
        /* check if need to preread a block */
        tst.w   preread_cnt
        beq.w   7f
        subq.w  #1,preread_cnt

        /* read a buffer */
        bra.w   20b

7:
        z80wr   FM_RQPND,#0         /* request handled */
        move.b  #0,REQ_ACT.w
8:
        move.w  #0x0000,0xA11100    /* Z80 deassert bus request */
9:
        movem.l (sp)+,d0-d7/a0-a6
        |move.w  (sp)+,sr            /* restore int level */
        rts


| reset YM2612
rst_ym2612:
        lea     FMReset,a1
        lea     0xA00000,a0
        moveq   #6,d3
0:
        move.b  (a1)+,d0                /* FM reg */
        move.b  (a1)+,d1                /* FM data */
        moveq   #15,d2
1:
        move.b  d0,0x4000(a0)           /* FM reg */
        nop
        nop
        nop
        move.b  d1,0x4001(a0)           /* FM data */
        nop
        nop
        nop
        move.b  d0,0x4002(a0)           /* FM reg */
        nop
        nop
        nop
        move.b  d1,0x4003(a0)           /* FM data */
        addq.b  #1,d0
        dbra    d2,1b
        dbra    d3,0b

        move.w  #0x4000,d1
        moveq   #28,d2
2:
        move.b  (a1)+,d1                /* YM reg */
        move.b  (a1)+,0(a0,d1.w)        /* YM data */
        dbra    d2,2b

| reset PSG
        move.b  #0x9F,0xC00011
        move.b  #0xBF,0xC00011
        move.b  #0xDF,0xC00011
        move.b  #0xFF,0xC00011
        rts


| Horizontal Blank handler

horizontal_blank:
        move.l  d0,-(sp)
        move.l  d1,-(sp)
        move.l  a0,-(sp)
        move.l  a1,-(sp)

        lea     0xC00004,a0
        lea     0xC00000,a1

        move.l  #0x40000010,(a0)

        move.w  #0x8A00,d1
        cmpi.b  #1,hint_count
        beq.s   1f
        cmpi.b  #2,hint_count
        beq.s   2f
        cmpi.b  #3,hint_count
        beq.s   3f
0:
        move.b  hint_1_interval,d1
        bra.s   5f
1:
        move.b  hint_2_interval,d1
        bra.s   5f
2:
        move.b  #0xFF,d1
        move.l  hint_1_scroll_y_positions,d0
        bra.s   4f
3:
        |move.b  #0,d1
        move.l  hint_2_scroll_y_positions,d0
        |bra.s   4f
4:
        move.l  d0,(a1)             /* update scroll A and B vertical positions */
5:
        move.w  d1,(a0) /* reg 10 = HINT = 0 */
        addi.b  #1,hint_count

        move.l  (sp)+,a1
        move.l  (sp)+,a0
        move.l  (sp)+,d1
        move.l  (sp)+,d0
        rte


| Vertical Blank handler

vert_blank:
        move.l  d1,-(sp)
        move.l  d2,-(sp)
        move.l  d3,-(sp)
        move.l  a1,-(sp)
        move.l  a2,-(sp)

        lea     0xC00004,a0
        lea     0xC00000,a1



        /* Calculate the HINT line for Scroll A section break */
        move.b  scroll_a_vert_rate_top,d0
        move.b  scroll_a_vert_rate_bottom,d1
        cmp.b   d0,d1
        blt.s   11f

10:      /* Top section has priority */
        move.w  current_scroll_a_top_y,d0
        bra.s   12f

11:      /* Bottom section has priority */
        move.w  current_scroll_a_bottom_y,d0

12:
        cmpi.w  #0x200,d2
        blt.s   14f
13:
        move.w  #0,d2
14:



        /* Calculate the HINT line for Scroll B section break */
        move.b  scroll_b_vert_rate_top,d0
        move.b  scroll_b_vert_rate_bottom,d1
        cmp.b   d0,d1
        blt.s   21f

20:      /* Top section has priority */
        move.w  current_scroll_b_top_y,d0
        bra.s   22f

21:      /* Bottom section has priority */
        move.w   current_scroll_b_bottom_y,d0

22:
        move.w  scroll_b_vert_break,d3
        sub.w   d0,d3
        andi.w  #0x3FF,d3

        cmpi.w  #0x200,d3
        blt.s   24f
23:
        move.w  #0,d3
24:



        /* Figure out what section will be drawn at the top of the screen. */
30:
        cmpi.w  #0,d2
        ble.w   31f
        move.w  current_scroll_a_top_y,d1
        bra.s   32f
31:
        move.w  current_scroll_a_bottom_y,d1
32:
        swap    d1

40:
        cmpi.w  #0,d3
        ble.w   41f
        move.w  current_scroll_b_top_y,d1
        bra.s   50f
41:
        move.w  current_scroll_b_bottom_y,d1

50:
        move.l  #0x40000010,(a0)
        move.l  d1,(a1)             /* Update scroll A and B vertical positions. */



        /* Figure out the HINT intervals and corresponding scroll positions. */
60:
        move.b  #0xFF,hint_1_interval
        move.b  #0xFF,hint_2_interval

        move.l  d1,hint_1_scroll_y_positions

        cmpi.w  #0xE0,d2
        blt.s   0f
        move.w  #0,d2           /* Interrupt intervals <=0 and >=224 will be skipped. */
0:
        cmpi.w  #0xE0,d3
        blt.s   1f
        move.w  #0,d3           /* Interrupt intervals <=0 and >=224 will be skipped. */
1:

        cmp.w   d2,d3
        bgt.s   6f
        blt.s   3f
2:
        cmpi.w  #0,d2
        ble.w   9f
        move.b  d2,hint_1_interval  /* Scroll B and A will share one HINT. */
        move.b  current_scroll_a_bottom_y,d1
        move.w  d1,hint_1_scroll_a_y
        move.w  d1,hint_1_scroll_b_y
        bra.w   9f
3:
        cmpi.w  #0,d3
        ble.s   5f
        |subi.b  #2,d3
        move.b  d3,hint_1_interval  /* Scroll B will have the first HINT. */
        move.w  current_scroll_b_bottom_y,d1
        move.w  d1,hint_1_scroll_b_y
4:
        cmpi.w  #0,d2
        ble.w   9f
        sub.b   d3,d2
        |add.b   #1,d2
        move.b  d2,hint_2_interval  /* Scroll A will have the second HINT. */
        move.w  d1,hint_2_scroll_b_y
        move.w  current_scroll_a_bottom_y,d1
        move.w  d1,hint_2_scroll_a_y
        bra.s   9f
5:
        cmpi.w  #0,d2
        ble.s   9f
        |subi.b  #2,d2
        move.b  d2,hint_1_interval  /* Scroll A will have the only HINT. */
        move.w  current_scroll_a_bottom_y,d1
        move.w  d1,hint_1_scroll_a_y
        bra.s   9f
6:
        cmpi.w  #0,d2
        ble.s   8f
        |subi.b  #2,d2
        move.b  d2,hint_1_interval  /* Scroll A will have the first HINT. */
        move.w  current_scroll_a_bottom_y,d1
        move.w  d1,hint_1_scroll_a_y
7:
        cmpi.w  #0,d3
        ble.s   9f
        sub.b   d2,d3
        |add.b   #1,d3
        move.b  d3,hint_2_interval  /* Scroll B will have the second HINT. */
        move.w  d1,hint_2_scroll_a_y
        move.w  current_scroll_b_bottom_y,d1
        move.w  d1,hint_2_scroll_b_y
        bra.s   9f
8:
        cmpi.w  #0,d3
        ble.s   9f
        |subi.b  #2,d3
        move.b  d3,hint_1_interval  /* Scroll B will have the only HINT. */
        move.w  current_scroll_b_bottom_y,d1
        move.w  d1,hint_1_scroll_b_y
        |bra.s   9f
9:

        move.b  #0,hint_count

        move.w  #0x8A00,d0
        move.w  d0,(a0)             /* reg 10 = HINT = 0 */

        tst.w   register_write_queue
        beq.s   10f
        move.w  register_write_queue,d0
        move.w  d0,(a0)
        moveq   #0,d0
        move.w  d0,register_write_queue



10:
        move.b  #1,need_bump_fm
        move.b  #1,need_ctrl_int

        /* read controllers */
        move.w  ctrl1_val,d0
        andi.w  #0xF000,d0
        cmpi.w  #0xF000,d0
        beq.b   0f               /* no pad in port 1 (or mouse) */
        lea     0xA10003,a0
        bsr     get_pad
        move.w  d2,ctrl1_val      /* controller 1 current value */
0:
        tst.b   net_type
        bne.b   1f               /* networking enabled, ignore port 2 */
        move.w  ctrl2_val,d0
        andi.w  #0xF000,d0
        cmpi.w  #0xF000,d0
        beq.b   1f               /* no pad in port 2 (or mouse) */
        lea     0xA10005,a0
        bsr     get_pad
        move.w  d2,ctrl2_val     /* controller 2 current value */
1:
        /* if SCD present, generate IRQ 2 */
        tst.w   gen_lvl2
        beq.b   2f
        lea     0xA12000,a0
        move.w  (a0),d0
        ori.w   #0x0100,d0
        move.w  d0,(a0)
2:
        tst.b   hotplug_cnt
        beq.b   3f
        subq.b  #1,hotplug_cnt
3:
        /* DLG: This code will erase our beautiful MD sky. */
        ||move.w  init_vdp_latch,d0
        ||move.w  #-1,init_vdp_latch

        ||cmpi.w  #1,d0
        ||bne.b   4f                  /* re-init vdp and vram */

        ||bsr     bump_fm
        ||bsr     init_vdp
        ||bsr     bump_fm
        ||bsr     load_font    /* DLG */
        move.w  #0x8174,0xC00004    /* display on, vblank enabled, V28 mode */
4:
        move.l  (sp)+,a2
        move.l  (sp)+,a1
        move.l  (sp)+,d3
        move.l  (sp)+,d2
        move.l  (sp)+,d1

        movea.l (sp)+,a0
        move.l  (sp)+,d0
        rte

| get current pad value
| entry: a0 = pad control port
| exit:  d2 = pad value (0 0 0 1 M X Y Z S A C B R L D U) or (0 0 0 0 0 0 0 0 S A C B R L D U)
get_pad:
        bsr.b   get_input       /* - 0 s a 0 0 d u - 1 c b r l d u */
        move.w  d0,d1
        andi.w  #0x0C00,d0
        bne.b   no_pad
        bsr.b   get_input       /* - 0 s a 0 0 d u - 1 c b r l d u */
        bsr.b   get_input       /* - 0 s a 0 0 0 0 - 1 c b m x y z */
        move.w  d0,d2
        bsr.b   get_input       /* - 0 s a 1 1 1 1 - 1 c b r l d u */
        andi.w  #0x0F00,d0      /* 0 0 0 0 1 1 1 1 0 0 0 0 0 0 0 0 */
        cmpi.w  #0x0F00,d0
        beq.b   common          /* six button pad */
        move.w  #0x010F,d2      /* three button pad */
common:
        lsl.b   #4,d2           /* - 0 s a 0 0 0 0 m x y z 0 0 0 0 */
        lsl.w   #4,d2           /* 0 0 0 0 m x y z 0 0 0 0 0 0 0 0 */
        andi.w  #0x303F,d1      /* 0 0 s a 0 0 0 0 0 0 c b r l d u */
        move.b  d1,d2           /* 0 0 0 0 m x y z 0 0 c b r l d u */
        lsr.w   #6,d1           /* 0 0 0 0 0 0 0 0 s a 0 0 0 0 0 0 */
        or.w    d1,d2           /* 0 0 0 0 m x y z s a c b r l d u */
        eori.w  #0x1FFF,d2      /* 0 0 0 1 M X Y Z S A C B R L D U */
        rts

no_pad:
        move.w  #0xF000,d2
        rts

| read single phase from controller
get_input:
        move.b  #0x00,(a0)
        nop
        nop
        move.b  (a0),d0
        move.b  #0x40,(a0)
        lsl.w   #8,d0
        move.b  (a0),d0
        rts

| get_port: returns ID bits of controller pointed to by a0 in d0
get_port:
        move.b  (a0),d0
        move.b  #0x00,(a0)
        moveq   #12,d1
        and.b   d0,d1
        sne     d2
        andi.b  #8,d2
        andi.b  #3,d0
        sne     d0
        andi.b  #4,d0
        or.b    d0,d2

        move.b  (a0),d0
        move.b  #0x40,(a0)
        moveq   #12,d1
        and.b   d0,d1
        sne     d1
        andi.b  #2,d1
        or.b    d1,d2
        andi.b  #3,d0
        sne     d0
        andi.b  #1,d0
        or.b    d0,d2

        move.w  #0xF000,d0               /* no pad in port */
        cmpi.b  #0x0D,d2
        bne.b   0f
        moveq   #0,d0                    /* pad in port */
0:
        rts

| Check ports - sets controller word to 0xF001 if mouse, or 0 if not.
|   The next vblank will try to read a pad if 0, which will set the word
|   to 0xF000 if no pad found.
chk_ports:
        /* get ID port 1 */
        lea     0xA10003,a0
        bsr.b   get_port
        move.w  d0,ctrl1_val             /* controller 1 */

        tst.b   net_type
        beq.b   0f                      /* ignore controller 2 when networking enabled */
        rts
0:
        /* get ID port 2 */
        lea     0xA10005,a0
        bsr.b   get_port
        move.w  d0,ctrl2_val             /* controller 2 */
        rts









new_sound_driver:



load_sequence:
        move.w  #0x2700,sr          /* disable ints */

        move.l  a0,-(sp)
        move.l  a1,-(sp)
        move.l  a2,-(sp)
        move.l  d0,-(sp)
        move.l  d1,-(sp)
        move.l  d2,-(sp)

        bsr     get_lump_source_and_size
        lea     sequence_data,a1
        move.l  lump_ptr,d0
        andi.l  #0x7FFFF,d0
        addi.l  #0x880000,d0
        movea.l d0,a2

        move.l  lump_size,d1
        addi.l  #3,d1
        lsr.l   #2,d1
        sub.l   #1,d1
        cmpi.l  #0xFFF,d1
        bls.s   0f
        move.l  #0xFFF,d1      | limit to 16KB (4KB * 4)
0:
        move.l  (a2)+,(a1)+
        dbra    d1,0b

        move.l  (sp)+,d2
        move.l  (sp)+,d1
        move.l  (sp)+,d0
        move.l  (sp)+,a2
        move.l  (sp)+,a1
        move.l  (sp)+,a0

        move.w  #0,0xA15120         /* done */

        move.w  #0x2000,sr              /* enable interrupts */

        bra     main_loop



load_voice:
        |move.l  a0,-(sp)
        |move.l  a1,-(sp)
        |move.l  a2,-(sp)
        |move.l  a3,-(sp)
        move.l  a6,-(sp)
        move.l  d0,-(sp)
        move.l  d1,-(sp)
        move.l  d2,-(sp)



        |move.w  #0x0100,0xA11100    /* Z80 assert bus request */
        |move.w  #0x0100,0xA11200    /* Z80 deassert reset */
1:
        |move.w  0xA11100,d0
        |andi.w  #0x0100,d0
        |bne.b   1b                  /* wait for bus acquired */

        |z80rd   CRITICAL,d0
        |bne     8f                  /* z80 critical section, just exit */



        |move.l  #0xA04000,a0
        |move.l  #0xA04001,a1
        |move.l  #0xA04002,a2
        |move.l  #0xA04003,a3

        |moveq   #0,d0           | D0 = channel number
        moveq   #0,d1           | D1 = voice number

        /* Calculate the address of the voice data */
        lea     voice_param_table,a6
        andi.w  #0x00FF,d1
        add.w   d1,a6
        lsl.w   #3,d1
        add.w   d1,a6
        lsl.w   #1,d1
        add.w   d1,a6

        /* Load the feedback/algorithm register value */
        move.b  #0xB0,d2        | D2 = register 0xB0
        add.b   d0,d2           | D2 = register 0xB0 + D0
        move.b  d2,(a0)         | Port 0 = address (D2)
        move.b  (a6)+,(a1)      | Port 1 = data (read byte from voice data)

        /* Load all other values into registers 0x30-0x8E */
        move.w  #0x30,d2
        add.b   d0,d2
load_voice_byte_loop:
        move.b  d2,(a0)
        move.b  (a6)+,(a1)
        add.b   #4,d2
        cmpi.w  #0x90,d2
        blt.s   load_voice_byte_loop



        |move.w  #0x0000,0xA11200        /* Z80 assert reset */

        |move.w  #0x0000,0xA11100        /* Z80 deassert bus request */
7:
        |move.w  0xA11100,d0
        |andi.w  #0x0100,d0
        |beq.b   7b                      /* wait for bus released */

        |move.w  #0x0100,0xA11200        /* Z80 deassert reset - run driver */



        move.l  (sp)+,d2
        move.l  (sp)+,d1
        move.l  (sp)+,d0
        move.l  (sp)+,a6
        |move.l  (sp)+,a3
        |move.l  (sp)+,a2
        |move.l  (sp)+,a1
        |move.l  (sp)+,a0

        |move.w  #0,0xA15120         /* done */

        |bra     main_loop
        rts



play_sequence:
        move.l  a0,-(sp)
        move.l  a1,-(sp)
        move.l  a2,-(sp)
        move.l  a3,-(sp)
        move.l  a4,-(sp)
        move.l  a5,-(sp)
        move.l  a6,-(sp)
        move.l  d0,-(sp)
        move.l  d1,-(sp)
        move.l  d2,-(sp)
        move.l  d3,-(sp)

        move.w  #0x0100,0xA11100    /* Z80 assert bus request */
        move.l  #0xA04000,a0
        move.l  #0xA04001,a1
        move.l  #0xA04002,a2
        move.l  #0xA04003,a3

        moveq   #0,d0           | D0 = channel number

        lea     sequence_data,a6
        moveq   #0,d1
        move.w  d0,d1
        add.w   d1,d1
        move.w  (d1,a6),d1
        add.w   d1,a6
        move.l  a6,test_a6_before
        move.l  a6,a4
        add.w   song_fm1_pos,a6
        move.l  a6,test_a6_after

        cmpi.w  #0,song_fm1_wait
        bne.w   play_sequence_tic

play_sequence_read_next:
        moveq   #0,d1
        moveq   #0,d2
        moveq   #0,d3

        move.l  #0,test_last_read1      | erase 1, 2, 3, and 4

        move.b  (a6)+,d3
        move.b  d3,test_last_read1

        cmpi.b  #0,d3
        beq.w   play_sequence_rest
        cmpi.b  #0x3F,d3
        bls.w   play_sequence_wait
        cmpi.b  #0xBF,d3
        bls.w   play_sequence_note

        subi.b  #0xC0,d3
        add.b   d3,d3
        lea     play_sequence_command_table,a5
        add.w   d3,a5
        move.w  (a5),d2
        lea     play_sequence_command_table,a5
        add.w   d2,a5
        jmp     (a5)

play_sequence_note:
        | Note off
        move.b  #0x28,d2        | D2 = register 0x28
        move.b  d2,(a0)         | Port 0 = address (D2)
        move.b  d0,(a1)         | Port 1 = data (note off)
        | TESTING STOP ALL CHANNELS!
        |move.b  #0x29,d2        | D2 = register 0x29
        |move.b  d2,(a0)         | Port 0 = address (D2)
        |move.b  #0x00,(a1)      | Port 1 = data (note off)
        |move.b  #0x2A,d2        | D2 = register 0x2A
        |move.b  d2,(a0)         | Port 0 = address (D2)
        |move.b  #0x00,(a1)      | Port 1 = data (note off)
        |move.b  #0x28,d2        | D2 = register 0x29
        |move.b  d2,(a2)         | Port 0 = address (D2)
        |move.b  #0x00,(a3)      | Port 1 = data (note off)
        |move.b  #0x29,d2        | D2 = register 0x2A
        |move.b  d2,(a2)         | Port 0 = address (D2)
        |move.b  #0x00,(a3)      | Port 1 = data (note off)
        |move.b  #0x2A,d2        | D2 = register 0x2A
        |move.b  d2,(a2)         | Port 0 = address (D2)
        |move.b  #0x00,(a3)      | Port 1 = data (note off)
        | Reference frequency
        lea     freq_table,a5
        |sub.b   #0x40,d3
        sub.b   #0x28,d3
        lsl.w   #1,d3
        add.w   d3,a5
        | Set frequency
        move.b  #0xA4,d2        | D2 = register 0xA4
        add.b   d0,d2           | D2 = register 0xA4 + D0
        move.b  d2,(a0)         | Port 0 = address (D2)
        move.b  (a5)+,(a1)      | Port 1 = data (read byte from frequency table)
        move.b  #0xA0,d2        | D2 = register 0xA0
        add.b   d0,d2           | D2 = register 0xA0 + D0
        move.b  d2,(a0)         | Port 0 = address (D2)
        move.b  (a5)+,(a1)      | Port 1 = data (read byte from frequency table)
        | Note on
        move.b  #0x28,d2        | D2 = register 0x28
        move.b  d2,(a0)         | Port 0 = address (D2)
        ori.b   #0xF0,d0
        move.b  d0,(a1)         | Port 1 = data (note on)
        andi.b  #0x0F,d0
        bra.w   play_sequence_read_next

play_sequence_rest:
        | Note off
        move.b  #0x28,d2        | D2 = register 0x28
        add.b   d0,d2           | D2 = register 0x28 + D0
        move.b  d2,(a0)         | Port 0 = address (D2)
        move.b  #0x00,(a1)      | Port 1 = data (note off)
        bra.w   play_sequence_read_next

play_sequence_wait:
        addi.w  #1,d3                   | DLG: Should this be done??
        move.w  d3,song_fm1_wait
        bra.w   play_sequence_tic

play_sequence_command_table:
        dc.w    seqcmd_wait_byte - play_sequence_command_table          /* 0xC0 */
        dc.w    seqcmd_wait_word - play_sequence_command_table          /* 0xC1 */
        dc.w    seqcmd_no_cmd - play_sequence_command_table             /* 0xC2 */
        dc.w    seqcmd_no_cmd - play_sequence_command_table             /* 0xC3 */
        dc.w    seqcmd_no_cmd - play_sequence_command_table             /* 0xC4 */
        dc.w    seqcmd_no_cmd - play_sequence_command_table             /* 0xC5 */
        dc.w    seqcmd_no_cmd - play_sequence_command_table             /* 0xC6 */
        dc.w    seqcmd_no_cmd - play_sequence_command_table             /* 0xC7 */
        dc.w    seqcmd_stereo_off - play_sequence_command_table         /* 0xC8 */
        dc.w    seqcmd_stereo_left - play_sequence_command_table        /* 0xC9 */
        dc.w    seqcmd_stereo_right - play_sequence_command_table       /* 0xCA */
        dc.w    seqcmd_stereo_both - play_sequence_command_table        /* 0xCB */
        dc.w    seqcmd_octave - play_sequence_command_table             /* 0xCC */
        dc.w    seqcmd_voice - play_sequence_command_table              /* 0xCD */
        dc.w    seqcmd_vibrato_off - play_sequence_command_table        /* 0xCE */
        dc.w    seqcmd_vibrato_speed - play_sequence_command_table      /* 0xCF */

seqcmd_no_cmd:
        nop
        bra.w   play_sequence_read_next

seqcmd_wait_byte:
        move.b  (a6)+,d2
        move.b  d2,test_last_read2
        addi.w  #1,d2                   | DLG: Should this be done??
        move.w  d2,song_fm1_wait
        bra.s   play_sequence_tic
seqcmd_wait_word:
        move.b  (a6)+,d2
        move.b  d2,test_last_read2
        lsl.w   #8,d2
        move.b  (a6)+,d2
        move.b  d2,test_last_read3
        addi.w  #1,d2                   | DLG: Should this be done??
        move.w  d2,song_fm1_wait
        bra.s   play_sequence_tic
seqcmd_stereo_off:
        bra.w   play_sequence_read_next
seqcmd_stereo_left:
        bra.w   play_sequence_read_next
seqcmd_stereo_right:
        bra.w   play_sequence_read_next
seqcmd_stereo_both:
        bra.w   play_sequence_read_next
seqcmd_octave:
        move.b  (a6)+,d2
        move.b  d2,test_last_read2
        bra.w   play_sequence_read_next
seqcmd_voice:
        move.b  (a6)+,d2
        move.b  d2,test_last_read2
        jsr     load_voice
        bra.w   play_sequence_read_next
seqcmd_vibrato_off:
        bra.w   play_sequence_read_next
seqcmd_vibrato_speed:
        move.b  (a6)+,d2
        move.b  d2,test_last_read2
        bra.w   play_sequence_read_next

        | finish me
        nop

play_sequence_tic:
        cmpi.w  #0,song_fm1_wait
        beq.s   1f
        subi.w  #1,song_fm1_wait
1:
9:
        move.w  a4,d2
        move.w  a6,d1
        sub.w   d2,d1
        move.w  d1,song_fm1_pos

play_sequence_done:
        |move.w  #0x0000,0xA11100        /* Z80 deassert bus request */

        move.l  (sp)+,d3
        move.l  (sp)+,d2
        move.l  (sp)+,d1
        move.l  (sp)+,d0
        move.l  (sp)+,a6
        move.l  (sp)+,a5
        move.l  (sp)+,a4
        move.l  (sp)+,a3
        move.l  (sp)+,a2
        move.l  (sp)+,a1
        move.l  (sp)+,a0

        move.w  #0,0xA15120         /* done */

        bra     main_loop



        /* FINISH ME! */




        .align 4
testabc:        dc.l    0x08257368
test_a6_before: dc.l    0
test_a6_after:  dc.l    0
test_last_read1:    dc.b    0
test_last_read2:    dc.b    0
test_last_read3:    dc.b    0
test_last_read4:    dc.b    0
song_fm1_pos:       dc.w    0x0000
song_fm2_pos:       dc.w    0x0000
song_fm3_pos:       dc.w    0x0000
song_fm4_pos:       dc.w    0x0000
song_fm5_pos:       dc.w    0x0000
song_fm6_pos:       dc.w    0x0000
song_psg1_pos:      dc.w    0x0000
song_psg2_pos:      dc.w    0x0000
song_psg3_pos:      dc.w    0x0000
song_pwm1_pos:      dc.w    0x0000
song_pwm2_pos:      dc.w    0x0000
song_pwm3_pos:      dc.w    0x0000
song_pwm4_pos:      dc.w    0x0000


song_fm1_wait:      dc.w    0x0000
song_fm2_wait:      dc.w    0x0000
song_fm3_wait:      dc.w    0x0000
song_fm4_wait:      dc.w    0x0000
song_fm5_wait:      dc.w    0x0000
song_fm6_wait:      dc.w    0x0000
song_psg1_wait:     dc.w    0x0000
song_psg2_wait:     dc.w    0x0000
song_psg3_wait:     dc.w    0x0000


        .align 4
sequence_data:
        .space  16384


        .align 2
voice_param_table:
        /* GFZ1 - 05 - 01_title_52 */
        dc.b    0x3D                        | 0xB0 : FB/ALG
        dc.b    0x01, 0x51, 0x21, 0x01      | 0x30 : DT/MUL
        dc.b    0x19, 0x11, 0x15, 0x11      | 0x40 : TL
        dc.b    0x12, 0x14, 0x14, 0x0F      | 0x50 : RS/AR
        dc.b    0x0A, 0x05, 0x05, 0x05      | 0x60 : AM/D1R
        dc.b    0x00, 0x00, 0x00, 0x00      | 0x70 : D2R
        dc.b    0x2B, 0x2B, 0x2B, 0x1B      | 0x80 : D1L/RR
        | dc.b    0x00, 0x00, 0x00, 0x00      | 0x90 : SSG-EG

        /* Guitar (Kid Chameleon) */
        dc.b    0x38                        | 0xB0 : FB/ALG
        dc.b    0x71, 0x72, 0x64, 0x73      | 0x30 : DT/MUL
        dc.b    0x25, 0x19, 0x07, 0x11      | 0x40 : TL
        dc.b    0x55, 0x5B, 0x1D, 0x1D      | 0x50 : RS/AR
        dc.b    0x02, 0x8F, 0x0F, 0x04      | 0x60 : AM/D1R
        dc.b    0x03, 0x00, 0x03, 0x01      | 0x70 : D2R
        dc.b    0x66, 0x03, 0x13, 0x56      | 0x80 : D1L/RR

        /* Bass (Sonic 2 (SW) - Casino Night Zone 2P) */
        dc.b    0x08                        | 0xB0 : FB/ALG
        dc.b    0x09, 0x30, 0x70, 0x00      | 0x30 : DT/MUL
        dc.b    0x25, 0x13, 0x30, 0x80      | 0x40 : TL
        dc.b    0x1F, 0x5F, 0x1F, 0x5F      | 0x50 : RS/AR
        dc.b    0x12, 0x0A, 0x0E, 0x0A      | 0x60 : AM/D1R
        dc.b    0x00, 0x04, 0x04, 0x03      | 0x70 : D2R
        dc.b    0x2F, 0x2F, 0x2F, 0x2F      | 0x80 : D1L/RR

        /* Lead (Sonic 2 (SW) - Casino Night Zone 2P) */
        dc.b    0x3A                        | 0xB0 : FB/ALG
        dc.b    0x01, 0x01, 0x07, 0x01      | 0x30 : DT/MUL
        dc.b    0x17, 0x27, 0x28, 0x80      | 0x40 : TL
        dc.b    0x8E, 0x8D, 0x8E, 0x53      | 0x50 : RS/AR
        dc.b    0x0E, 0x0E, 0x0E, 0x03      | 0x60 : AM/D1R
        dc.b    0x00, 0x00, 0x00, 0x00      | 0x70 : D2R
        dc.b    0x1F, 0x1F, 0xFF, 0x0F      | 0x80 : D1L/RR

        .align 2
freq_table:
        dc.w     (2048*0)+81    | C
        dc.w     (2048*0)+85    | C#
        dc.w     (2048*0)+91    | D
        dc.w     (2048*0)+96    | D#
        dc.w     (2048*0)+102   | E
        dc.w     (2048*0)+108   | F
        dc.w     (2048*0)+114   | F#
        dc.w     (2048*0)+121   | G

        dc.w     (2048*0)+128   | G#
        dc.w     (2048*0)+136   | A
        dc.w     (2048*0)+144   | A#
        dc.w     (2048*0)+152   | B
        dc.w     (2048*0)+161   | C
        dc.w     (2048*0)+171   | C#
        dc.w     (2048*0)+181   | D
        dc.w     (2048*0)+192   | D#
        dc.w     (2048*0)+203   | E
        dc.w     (2048*0)+215   | F
        dc.w     (2048*0)+228   | F#
        dc.w     (2048*0)+242   | G

        dc.w     (2048*0)+256   | G#
        dc.w     (2048*0)+271   | A
        dc.w     (2048*0)+287   | A#
        dc.w     (2048*0)+304   | B
        dc.w     (2048*0)+323   | C
        dc.w     (2048*0)+342   | C#
        dc.w     (2048*0)+362   | D
        dc.w     (2048*0)+384   | D#
        dc.w     (2048*0)+406   | E
        dc.w     (2048*0)+431   | F
        dc.w     (2048*0)+456   | F#
        dc.w     (2048*0)+483   | G

        dc.w     (2048*0)+512   | G#
        dc.w     (2048*0)+542   | A
        dc.w     (2048*0)+575   | A#
        dc.w     (2048*0)+609   | B
        dc.w     (2048*0)+645   | C
        dc.w     (2048*0)+683   | C#
        dc.w     (2048*0)+724   | D
        dc.w     (2048*0)+767   | D#
        dc.w     (2048*0)+813   | E
        dc.w     (2048*0)+861   | F
        dc.w     (2048*0)+912   | F#
        dc.w     (2048*0)+967   | G

        dc.w     (2048*0)+1024  | G#
        dc.w     (2048*0)+1085  | A
        dc.w     (2048*0)+1149  | A#
        dc.w     (2048*0)+1218  | B
        dc.w     (2048*0)+1290  | C
        dc.w     (2048*0)+1367  | C#
        dc.w     (2048*0)+1448  | D
        dc.w     (2048*0)+1534  | D#
        dc.w     (2048*0)+1625  | E
        dc.w     (2048*0)+1722  | F
        dc.w     (2048*0)+1825  | F#
        dc.w     (2048*0)+1933  | G

        dc.w     (2048*1)+1024  | G#
        dc.w     (2048*1)+1085  | A
        dc.w     (2048*1)+1149  | A#
        dc.w     (2048*1)+1218  | B
        dc.w     (2048*1)+1290  | C
        dc.w     (2048*1)+1367  | C#
        dc.w     (2048*1)+1448  | D
        dc.w     (2048*1)+1534  | D#
        dc.w     (2048*1)+1625  | E
        dc.w     (2048*1)+1722  | F
        dc.w     (2048*1)+1825  | F#
        dc.w     (2048*1)+1933  | G

        dc.w     (2048*2)+1024  | G#
        dc.w     (2048*2)+1085  | A
        dc.w     (2048*2)+1149  | A#
        dc.w     (2048*2)+1218  | B
        dc.w     (2048*2)+1290  | C
        dc.w     (2048*2)+1367  | C#
        dc.w     (2048*2)+1448  | D
        dc.w     (2048*2)+1534  | D#
        dc.w     (2048*2)+1625  | E
        dc.w     (2048*2)+1722  | F
        dc.w     (2048*2)+1825  | F#
        dc.w     (2048*2)+1933  | G

        dc.w     (2048*3)+1024  | G#
        dc.w     (2048*3)+1085  | A
        dc.w     (2048*3)+1149  | A#
        dc.w     (2048*3)+1218  | B
        dc.w     (2048*3)+1290  | C
        dc.w     (2048*3)+1367  | C#
        dc.w     (2048*3)+1448  | D
        dc.w     (2048*3)+1534  | D#
        dc.w     (2048*3)+1625  | E
        dc.w     (2048*3)+1722  | F
        dc.w     (2048*3)+1825  | F#
        dc.w     (2048*3)+1933  | G

        dc.w     (2048*4)+1024  | G#
        dc.w     (2048*4)+1085  | A
        dc.w     (2048*4)+1149  | A#
        dc.w     (2048*4)+1218  | B
        dc.w     (2048*4)+1290  | C
        dc.w     (2048*4)+1367  | C#
        dc.w     (2048*4)+1448  | D
        dc.w     (2048*4)+1534  | D#
        dc.w     (2048*4)+1625  | E
        dc.w     (2048*4)+1722  | F
        dc.w     (2048*4)+1825  | F#
        dc.w     (2048*4)+1933  | G

        dc.w     (2048*5)+1024  | G#
        dc.w     (2048*5)+1085  | A
        dc.w     (2048*5)+1149  | A#
        dc.w     (2048*5)+1218  | B
        dc.w     (2048*5)+1290  | C
        dc.w     (2048*5)+1367  | C#
        dc.w     (2048*5)+1448  | D
        dc.w     (2048*5)+1534  | D#
        dc.w     (2048*5)+1625  | E
        dc.w     (2048*5)+1722  | F
        dc.w     (2048*5)+1825  | F#
        dc.w     (2048*5)+1933  | G

        dc.w     (2048*6)+1024  | G#
        dc.w     (2048*6)+1085  | A
        dc.w     (2048*6)+1149  | A#
        dc.w     (2048*6)+1218  | B
        dc.w     (2048*6)+1290  | C
        dc.w     (2048*6)+1367  | C#
        dc.w     (2048*6)+1448  | D
        dc.w     (2048*6)+1534  | D#
        dc.w     (2048*6)+1625  | E
        dc.w     (2048*6)+1722  | F
        dc.w     (2048*6)+1825  | F#
        dc.w     (2048*6)+1933  | G

        dc.w     (2048*7)+1024  | G#
        dc.w     (2048*7)+1085  | A
        dc.w     (2048*7)+1149  | A#
        dc.w     (2048*7)+1218  | B
        dc.w     (2048*7)+1290  | C
        dc.w     (2048*7)+1367  | C#
        dc.w     (2048*7)+1448  | D
        dc.w     (2048*7)+1534  | D#
        dc.w     (2048*7)+1625  | E
        dc.w     (2048*7)+1722  | F
        dc.w     (2048*7)+1825  | F#
        dc.w     (2048*7)+1933  | G








| Global variables for 68000

        .align  4

vblank:
        dc.l    0
extint:
        dc.l    0

        .global gen_lvl2
gen_lvl2:
        dc.w    0

        .global    use_cd
use_cd:
        dc.w    0

        .global cd_ok
cd_ok:
        dc.w    0
first_track:
        dc.w    0
last_track:
        dc.w    0
number_tracks:
        dc.w    0

        .global megasd_ok
megasd_ok:
        dc.w    0

        .global megasd_num_cdtracks
megasd_num_cdtracks:
        dc.w    0

        .global everdrive_ok
everdrive_ok:
        dc.w    0

init_vdp_latch:
        dc.w    -1

ctrl1_val:
        dc.w    0
ctrl2_val:
        dc.w    0

net_rbix:
        dc.w    0
net_wbix:
        dc.w    0
net_rdbuf:
        .space  32
net_link_timeout:
        dc.w    DEFAULT_LINK_TIMEOUT

    .global net_type
net_type:
        dc.b    0

hotplug_cnt:
        dc.b    0

need_bump_fm:
        dc.b    1
need_ctrl_int:
        dc.b    0

gamemode:
        dc.b    0

legacy_emulator:
        dc.b    0

bnk7_save:
        dc.b    7

        .align  4

        .global    fm_start
fm_start:
        dc.l    0
        .global    fm_loop
fm_loop:
        dc.l    0
        .global    fm_loop2
fm_loop2:
        dc.l    0
        .global pcm_baseoffs
pcm_baseoffs:
        dc.l    0
        .global    vgm_ptr
vgm_ptr:
        dc.l    0
        .global    vgm_size
vgm_size:
        dc.l    0
        .global    fm_rep
fm_rep:
        dc.w    0
        .global    fm_idx
fm_idx:
        dc.w    0
offs68k:
        dc.w    0
offsz80:
        dc.w    0
preread_cnt:
        dc.w    0

        .align  4

fg_color:
        dc.l    0x11111111      /* default to color 1 for fg color */
bg_color:
        dc.l    0x00000000      /* default to color 0 for bg color */
crsr_x:
        dc.w    0
crsr_y:
        dc.w    0
dbug_color:
        dc.w    0
        
scroll_b_vert_offset_top:
        dc.w    0
scroll_b_vert_offset_bottom:
        dc.w    0
scroll_a_vert_offset_top:
        dc.w    0
scroll_a_vert_offset_bottom:
        dc.w    0
scroll_b_vert_break:
        dc.w    0
scroll_a_vert_break:
        dc.w    0

scroll_b_vert_rate_top:
        dc.b    0
scroll_b_vert_rate_bottom:
        dc.b    0
scroll_a_vert_rate_top:
        dc.b    0
scroll_a_vert_rate_bottom:
        dc.b    0

current_sky_top_y_positions:
current_scroll_a_top_y:
        dc.w    0
current_scroll_b_top_y:
        dc.w    0

current_sky_bottom_y_positions:
current_scroll_a_bottom_y:
        dc.w    0
current_scroll_b_bottom_y:
        dc.w    0

current_sky_top_x_positions:
current_scroll_a_top_x:
        dc.w    0
current_scroll_b_top_x:
        dc.w    0

current_sky_bottom_x_positions:
current_scroll_a_bottom_x:
        dc.w    0
current_scroll_b_bottom_x:
        dc.w    0

hint_1_scroll_y_positions:
hint_1_scroll_a_y:
        dc.w    0
hint_1_scroll_b_y:
        dc.w    0

hint_2_scroll_y_positions:
hint_2_scroll_a_y:
        dc.w    0
hint_2_scroll_b_y:
        dc.w    0

hint_1_scroll_x_positions:
hint_1_scroll_a_x:
        dc.w    0
hint_1_scroll_b_x:
        dc.w    0

hint_2_scroll_x_positions:
hint_2_scroll_a_x:
        dc.w    0
hint_2_scroll_b_x:
        dc.w    0

hint_count:
        dc.b    0
hint_1_interval:
        dc.b    0
hint_2_interval:
        dc.b    0
        
        .align  2

register_write_queue:
        dc.w    0

        .align  4

lump_ptr:
        dc.l    0
lump_size:
        dc.l    0


next_write_pattern_name_table:
        dc.l    0x60000003      /* bank 0 (0xE000) */
        dc.l    0x40000003      /* bank 1 (0xC000) */
        dc.l    0x60000002      /* bank 2 (0xA000) */
        dc.l    0x40000002      /* bank 3 (0x8000) */
        dc.l    0x60000001      /* bank 4 (0x6000) */

write_scroll_b_table:
write_scroll_b_table_1:
        dc.l    0x60000003
write_scroll_b_table_2:
        dc.l    0x60000003
write_scroll_b_table_3:
        dc.l    0x60000003
write_scroll_b_table_4:
        dc.l    0x60000003
write_scroll_a_table:
write_scroll_a_table_1:
        dc.l    0x40000003
write_scroll_a_table_2:
        dc.l    0x40000003
write_scroll_a_table_3:
        dc.l    0x40000003
write_scroll_a_table_4:
        dc.l    0x40000003

scroll_b_address_register_values:
scroll_b_address_register_values_1:
        dc.b    0x07
scroll_b_address_register_values_2:
        dc.b    0x07
scroll_b_address_register_values_3:
        dc.b    0x07
scroll_b_address_register_values_4:
        dc.b    0x07

scroll_a_address_register_values:
scroll_a_address_register_values_1:
        dc.b    0x30
scroll_a_address_register_values_2:
        dc.b    0x30
scroll_a_address_register_values_3:
        dc.b    0x30
scroll_a_address_register_values_4:
        dc.b    0x30

write_horizontal_scroll_table:
        dc.l    0x40000000      /* Default VRAM write to address 0x0000 */
write_sprite_attribute_table:
        dc.l    0x48000000      /* Default VRAM write to address 0x0800 */

h32_left_edge_sprites:
        dc.w    0x0080, 0x0301, 0xE050, 0x007B
        dc.w    0x00A0, 0x0302, 0xE050, 0x007B
        dc.w    0x00C0, 0x0303, 0xE050, 0x007B
        dc.w    0x00E0, 0x0304, 0xE050, 0x007B
        dc.w    0x0100, 0x0305, 0xE050, 0x007B
        dc.w    0x0120, 0x0306, 0xE050, 0x007B
        dc.w    0x0140, 0x0300, 0xE050, 0x007B

|test_start:
|        dc.l    0x08257368
|test_values:
|        .space  128
|test_end:
|        dc.l    0x08257368


crossfade_lookup:
        dc.b	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
        dc.b	0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 2, 2, 2, 2, 2, 2
        dc.b	0, 0, 0, 0, 2, 2, 2, 2, 2, 2, 2, 2, 4, 4, 4, 4
        dc.b	0, 0, 0, 2, 2, 2, 2, 2, 4, 4, 4, 4, 4, 4, 6, 6
        dc.b	0, 0, 2, 2, 2, 2, 4, 4, 4, 4, 6, 6, 6, 6, 8, 8
        dc.b	0, 0, 2, 2, 2, 4, 4, 4, 6, 6, 6, 6, 8, 8, 8, 10
        dc.b	0, 0, 2, 2, 4, 4, 4, 6, 6, 6, 8, 8, 10,10,10,12
        dc.b	0, 0, 2, 2, 4, 4, 6, 6, 8, 8, 8, 10,10,12,12,14


bank1_palette_1:
        .space  32
bank1_palette_2:
        .space  32
bank1_palette_3:
        .space  32
bank1_palette_4:
        .space  32


bank2_palette_1:
        .space  32
bank2_palette_2:
        .space  32
bank2_palette_3:
        .space  32
bank2_palette_4:
        .space  32

FMReset:
        /* block settings */
        .byte   0x30,0x00
        .byte   0x40,0x7F
        .byte   0x50,0x1F
        .byte   0x60,0x1F
        .byte   0x70,0x1F
        .byte   0x80,0xFF
        .byte   0x90,0x00

        /* disable LFO */
        .byte   0,0x22
        .byte   1,0x00
        /* disable timers & set channel 6 to normal mode */
        .byte   0,0x27
        .byte   1,0x00
        /* all KEY_OFF */
        .byte   0,0x28
        .byte   1,0x00
        .byte   1,0x04
        .byte   1,0x01
        .byte   1,0x05
        .byte   1,0x02
        .byte   1,0x06
        /* disable DAC */
        .byte   0,0x2B
        .byte   1,0x80
        .byte   0,0x2A
        .byte   1,0x80
        .byte   0,0x2B
        .byte   1,0x00
        /* turn off channels */
        .byte   0,0xB4
        .byte   1,0x00
        .byte   0,0xB5
        .byte   1,0x00
        .byte   0,0xB6
        .byte   1,0x00
        .byte   2,0xB4
        .byte   3,0x00
        .byte   2,0xB5
        .byte   3,0x00
        .byte   2,0xB6
        .byte   3,0x00

        .align  4

        .bss
        .align  2
col_store:                      /* Is this label still needed? */
decomp_buffer:
        |.space  21*224*2        /* 140 double-columns in vram, 20 in wram, 1 in wram for swap */
        .space  16384           /* 16 KB */
