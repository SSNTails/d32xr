/*
  CALICO

  Renderer phase 6 - Seg Loop 
*/

#include "r_local.h"
#include "mars.h"

#define MIPSCALE 0x20000
#define LIGHTZSHIFT	10

typedef struct
{
#ifdef MARS
   inpixel_t *data;
#else
   pixel_t   *data;
#endif
   VINT      height;
   drawcol_t drawcol;

    // decals stuff
   int       numdecals;
   texdecal_t *decals;
   int       lastcol;
   uint8_t   *columncache;
} drawmip_t;

typedef struct drawtex_s
{
   drawmip_t mip[MIPLEVELS];
   VINT      maxmip;
   VINT      widthmask;
   fixed_t   texturemid;
   fixed_t   topheight;
   fixed_t   bottomheight;
   uint8_t   *columncache;
} drawtex_t;

typedef struct
{
    viswall_t* segl;

    drawtex_t tex[2];
    drawtex_t *first, *last;

    int lightmin, lightmax, lightsub, lightcoef;
#if MIPLEVELS > 1   
    unsigned minmip, maxmip;
#endif
} seglocal_t;

static char seg_lock = 0;

static void R_DrawTexture(int x, unsigned iscale, int colnum, fixed_t scale2, int floorclipx, int ceilingclipx, unsigned light, drawtex_t *tex, int miplevel) ATTR_DATA_CACHE_ALIGN;
static void R_DrawSeg(seglocal_t* lseg, unsigned short *clipbounds) ATTR_DATA_CACHE_ALIGN __attribute__((noinline));
static void R_SetupDrawTexture(drawtex_t *drawtex, texture_t *tex, fixed_t texturemid, fixed_t topheight, fixed_t bottomheight) ATTR_DATA_CACHE_ALIGN;

static void R_LockSeg(void) ATTR_DATA_CACHE_ALIGN;
static void R_UnlockSeg(void) ATTR_DATA_CACHE_ALIGN;
void R_SegCommands(void) ATTR_DATA_CACHE_ALIGN __attribute__((noinline));

//
// Render a wall texture as columns
//
static void R_DrawTexture(int x, unsigned iscale, int colnum_, fixed_t scale2, int floorclipx, int ceilingclipx, unsigned light, drawtex_t *tex, int miplevel)
{
    fixed_t top, bottom;

    top = FixedMul(scale2, tex->topheight)>>FRACBITS;
    bottom = FixedMul(scale2, tex->bottomheight)>>FRACBITS;

    top = centerY - top;
    if (top < ceilingclipx)
        top = ceilingclipx;

    bottom = centerY - bottom;
    if (bottom > floorclipx)
        bottom = floorclipx;

#if MIPLEVELS <= 1
    miplevel = 0;
#endif

    // CALICO: comment says this, but the code does otherwise...
    // colnum = colnum - tex->width * (colnum / tex->width)
    colnum_ &= tex->widthmask;

    // column has no length?
    if (top < bottom)
    {
        int colnum, mipcolnum;
        drawmip_t *mip;
        fixed_t frac;
#ifdef MARS
        inpixel_t* src;
#else
        pixel_t* src;
#endif

        colnum = colnum_ & tex->widthmask;
        mipcolnum = colnum;
        frac = tex->texturemid - (centerY - top) * iscale;

#if MIPLEVELS > 1
        if (miplevel > tex->maxmip)
            miplevel = tex->maxmip;
        if (miplevel > 0) {
            unsigned m = miplevel;
            do {
                frac >>= 1;
                mipcolnum >>= 1;
            } while (--m);
        }
#endif

        // CALICO: Jaguar-specific GPU blitter input calculation starts here.
        // We invoke a software column drawer instead.
        mip = &tex->mip[miplevel];
        src = mip->data + mipcolnum * mip->height;

        if (mip->numdecals > 0)
        {
            // decals/composite textures
            if (mip->lastcol == colnum)
            {
                src = mip->columncache;
            }
            else
            {
                boolean decaled;

                decaled = R_CompositeColumn(colnum, mip->numdecals, mip->decals,
                    src, mip->columncache, mip->height, miplevel);
                if (decaled)
                {
                    src = mip->columncache;
                    mip->lastcol = colnum;
                }
            }
        }

        mip->drawcol(x, top, bottom-1, light, frac, iscale, src, mip->height, NULL);
    }
}

//
// Main seg drawing loop
//
static void R_DrawSeg(seglocal_t* lseg, unsigned short *clipbounds)
{
    viswall_t* segl = lseg->segl;

    drawcol_t drawsky = (segl->actionbits & AC_ADDSKY) != 0 ? drawcol : NULL;

    fixed_t scalefrac = segl->scalefrac;
    const fixed_t scalestep = segl->scalestep;

    const unsigned centerangle = segl->centerangle;
    unsigned offset = segl->offset;
    fixed_t distance = segl->distance;

    const int ceilingheight = segl->ceilingheight;

    int texturelight = lseg->lightmax;
    int lightmax = lseg->lightmax, lightmin = lseg->lightmin,
        lightcoef = lseg->lightcoef, lightsub = lseg->lightsub;

    const int stop = segl->stop;
    int x;
    unsigned miplevel = 0;

    drawtex_t *tex;

    for (x = segl->start; x <= stop; x++)
    {
       fixed_t r;
       int floorclipx, ceilingclipx;
       fixed_t scale2;
       unsigned colnum, iscale;

#ifdef MARS
        volatile int32_t t;
        __asm volatile (
           "mov #-128, r0\n\t"
           "add r0, r0 /* r0 is now 0xFFFFFF00 */ \n\t"
           "mov #0, %0\n\t"
           "mov.l %0, @(16, r0) /* set high bits of the 64-bit dividend */ \n\t"
           "mov.l %1, @(0, r0) /* set 32-bit divisor */ \n\t"
           "mov #-1, %0\n\t"
           "mov.l %0, @(20, r0) /* set low  bits of the 64-bit dividend, start divide */\n\t"
           : "=&r" (t) : "r" (scalefrac) : "r0");
#else
        fixed_t scale = scalefrac;
#endif

        scale2 = scalefrac;
        scalefrac += scalestep;

        //
        // get ceilingclipx and floorclipx from clipbounds
        //
        ceilingclipx = clipbounds[x];
        floorclipx = ceilingclipx & 0x00ff;
        ceilingclipx = (unsigned)ceilingclipx >> 8;

        //
        // sky mapping
        //
        if (drawsky)
        {
            int top, bottom;

            top = ceilingclipx;
            bottom = FixedMul(scale2, ceilingheight)>>FRACBITS;
            bottom = centerY - bottom;
            if (bottom > floorclipx)
                bottom = floorclipx;

            if (top < bottom)
            {
                // CALICO: draw sky column
                int colnum = ((vd.viewangle + (xtoviewangle[x]<<FRACBITS)) >> ANGLETOSKYSHIFT) & 0xff;
#ifdef MARS
                inpixel_t* data = skytexturep->data[0] + colnum * skytexturep->height;
#else
                pixel_t* data = skytexturep->data[0] + colnum * skytexturep->height;
#endif
                drawsky(x, top, --bottom, 0, (top * 18204) << 2, FRACUNIT + 7281, data, 128, NULL);
            }
        }

        //
        // texture only stuff
        //
        if (lightcoef != 0)
        {
            // calc light level
            texturelight = FixedMul(scale2, lightcoef) - lightsub;
            if (texturelight < lightmin)
                texturelight = lightmin;
            else if (texturelight > lightmax)
                texturelight = lightmax;
            // convert to a hardware value
            texturelight = HWLIGHT((unsigned)texturelight>>FRACBITS);
        }

        // calculate texture offset
        r = finetangent((centerangle + (xtoviewangle[x]<<FRACBITS)) >> ANGLETOFINESHIFT);
        r = FixedMul(distance, r);

        colnum = (offset - r) >> FRACBITS;

#ifdef MARS
        __asm volatile (
            "mov #-128, r0\n\t"
            "add r0, r0 /* r0 is now 0xFFFFFF00 */ \n\t"
            "mov.l @(20, r0), %0 /* get 32-bit quotient */ \n\t"
            : "=r" (iscale) : : "r0");
#else
        iscale = 0xffffffffu / scale;
#endif

#if MIPLEVELS > 1
        // other texture drawing info
        miplevel = iscale / MIPSCALE;
        if (miplevel > lseg->maxmip)
            lseg->maxmip = miplevel;
        if (miplevel < lseg->minmip)
            lseg->minmip = miplevel;
#endif

        for (tex = lseg->first; tex < lseg->last; tex++)
            R_DrawTexture(x, iscale, colnum, scale2, floorclipx, ceilingclipx, texturelight, tex, miplevel);
    }
}

static void R_LockSeg(void)
{
    int res;
    do {
        __asm volatile (\
        "tas.b %1\n\t" \
            "movt %0\n\t" \
            : "=&r" (res) \
            : "m" (seg_lock) \
            );
    } while (res == 0);
}

static void R_UnlockSeg(void)
{
    seg_lock = 0;
}

static void R_SetupDrawTexture(drawtex_t *drawtex, texture_t *tex, 
    fixed_t texturemid, fixed_t topheight, fixed_t bottomheight)
{
    int j;
    int width = tex->width, height = tex->height;
    uint8_t *columncache = drawtex->columncache;
    int mipwidth, mipheight;

    drawtex->widthmask = width-1;
    drawtex->topheight = topheight;
    drawtex->bottomheight = bottomheight;
    drawtex->texturemid = texturemid;
#if MIPLEVELS > 1
    drawtex->maxmip = !texmips ? 0 : tex->mipcount-1;
#else
    drawtex->maxmip = 0;
#endif

    mipwidth = width;
    mipheight = height;
    for (j = 0; j <= drawtex->maxmip; j++)
    {
        drawmip_t *mip = &drawtex->mip[j];

        mip->height = mipheight;
        mip->data = tex->data[j];
        mip->drawcol = (mipheight & (mipheight - 1)) ? drawcolnpo2 : drawcol;
        mipwidth >>= 1, mipheight >>= 1;
        if (mipwidth < 1)
            mipwidth = 1;
        if (mipheight < 1)
            mipheight = 1;

        // decals stuff
        mip->lastcol = -1;
        mip->columncache = columncache;
        columncache += mipheight;

        mip->numdecals = tex->decals & 0x3;
        if (mip->numdecals && R_InTexCache(&r_texcache, mip->data)) {
            mip->numdecals = 0;
            continue;
        }
        mip->decals = &decals[tex->decals >> 2];
    }
}

void R_SegCommands(void)
{
    int i, segcount;
    seglocal_t lseg;
    drawtex_t* toptex, * bottomtex;
    int extralight;
    uint32_t clipbounds_[SCREENWIDTH / 2 + 1];
    uint16_t *clipbounds = (uint16_t *)clipbounds_;

    // initialize the clipbounds array
    R_InitClipBounds(clipbounds_);

    D_memset(&lseg, 0, sizeof(lseg));

    toptex = &lseg.tex[0];
    bottomtex = &lseg.tex[1];

    D_memset(toptex, 0, sizeof(*toptex));
    D_memset(bottomtex, 0, sizeof(*bottomtex));

    I_SetThreadLocalVar(DOOMTLS_COLORMAP, dc_colormaps);

    I_GetThreadLocalVar(DOOMTLS_COLUMNCACHE, toptex->columncache);
    bottomtex->columncache = toptex->columncache + COLUMN_CACHE_SIZE;

    extralight = vd.extralight;

    segcount = vd.lastwallcmd - vd.viswalls;
    for (i = 0; i < segcount; i++)
    {
        int seglight;
        unsigned actionbits;
        viswall_t* segl = vd.viswalls + i;

#ifdef MARS
        while ((MARS_SYS_COMM6 & 0xff) <= i)
            continue;
#endif
        if (segl->start > segl->stop)
            continue;

#ifdef MARS
        R_LockSeg();
        actionbits = *(volatile short *)&segl->actionbits;
        if (actionbits & AC_DRAWN || !(actionbits & (AC_TOPTEXTURE | AC_BOTTOMTEXTURE | AC_ADDSKY))) {
            R_UnlockSeg();
            goto post_draw;
        } else {
            segl->actionbits = actionbits|AC_DRAWN;
            R_UnlockSeg();
        }
#endif

        lseg.segl = segl;
        lseg.first = lseg.tex + 1;
        lseg.last = lseg.tex + 1;
#if MIPLEVELS > 1
        lseg.minmip = MIPLEVELS;
        lseg.maxmip = 0;
#endif

        if (vd.fixedcolormap)
        {
            lseg.lightmin = lseg.lightmax = vd.fixedcolormap;
        }
        else
        {
            seglight = segl->seglightlevel & 0xff;
            seglight += (int8_t)((unsigned)segl->seglightlevel >> 8);
            if (seglight < 0)
                seglight = 0;
#ifdef MARS
            lseg.lightmax = seglight;
            lseg.lightmax += extralight;
#endif
            if (lseg.lightmax > 255)
                lseg.lightmax = 255;

#ifdef MARS
            seglight = seglight - (255 - seglight - seglight/2) * 2;
#else
            seglight = seglight - (255 - seglight) * 2;
#endif
            seglight += extralight;
            if (seglight < MINLIGHT)
                seglight = MINLIGHT;
            if (seglight > lseg.lightmax)
                seglight = lseg.lightmax;
            lseg.lightmin = seglight;

            if (lseg.lightmin != lseg.lightmax)
            {
                lseg.lightcoef = ((unsigned)(lseg.lightmax - lseg.lightmin) << FRACBITS) / (800 - 160);
                lseg.lightsub = 160 * lseg.lightcoef;
                lseg.lightcoef <<= LIGHTZSHIFT;
                lseg.lightmin <<= FRACBITS;
                lseg.lightmax <<= FRACBITS;
            }
            else
            {
                lseg.lightcoef = 0;
                lseg.lightmin = lseg.lightmax = HWLIGHT((unsigned)lseg.lightmax);
            }
        }

        if (actionbits & AC_TOPTEXTURE)
        {
            R_SetupDrawTexture(toptex, &textures[segl->t_texturenum],
                segl->t_texturemid, segl->t_topheight, segl->t_bottomheight);
            lseg.first--;
        }

        if (actionbits & AC_BOTTOMTEXTURE)
        {
            R_SetupDrawTexture(bottomtex, &textures[segl->b_texturenum],
                segl->b_texturemid, segl->b_topheight, segl->b_bottomheight);
            lseg.last++;
        }

        R_DrawSeg(&lseg, clipbounds);

#if MIPLEVELS > 1
        if (1)
        {
            if (lseg.maxmip >= MIPLEVELS)
                lseg.maxmip = MIPLEVELS-1;
            segl->miplevels[0] = lseg.minmip;
            segl->miplevels[1] = lseg.maxmip;
        }
        else
#endif
        {
            segl->miplevels[0] = 0;
            segl->miplevels[1] = 0;
        }

post_draw:
        if(actionbits & (AC_NEWFLOOR|AC_NEWCEILING))
        {
            unsigned w = segl->stop - segl->start + 1;
            unsigned short *src = segl->clipbounds + segl->start, *dst = clipbounds + segl->start;

            if ((actionbits & (AC_NEWFLOOR|AC_NEWCEILING)) == (AC_NEWFLOOR|AC_NEWCEILING)) {
                --src;
                do {
                    *dst++ = *++src;
                } while (--w > 0);
            }
            else {
                int8_t *psrc = (int8_t *)src;
                int8_t *pdst = (int8_t *)dst;
                if (actionbits & AC_NEWFLOOR) {
                    psrc++, pdst++;
                }                 
                do {
                    *pdst = *psrc, psrc += 2, pdst += 2;
                } while (--w > 0);
            }
        }
    }
}

#ifdef MARS

void Mars_Sec_R_SegCommands(void)
{
    viswall_t *segl;

    for (segl = vd.viswalls; segl < vd.lastwallcmd; segl++)
    {
        if (segl->actionbits & AC_TOPTEXTURE)
        {
            texture_t* tex = &textures[segl->t_texturenum];
            Mars_ClearCacheLines(tex->data, (sizeof(tex->data)+31)/16);
        }
        if (segl->actionbits & AC_BOTTOMTEXTURE)
        {
            texture_t* tex = &textures[segl->b_texturenum];
            Mars_ClearCacheLines(tex->data, (sizeof(tex->data)+31)/16);
        }

        if (segl->actionbits & AC_ADDFLOOR)
        {
            flattex_t *flat = &flatpixels[segl->floorpicnum];
            Mars_ClearCacheLines(flat->data, (sizeof(flat->data)+31)/16);
        }
        if (segl->actionbits & AC_ADDCEILING)
        {
            flattex_t *flat = &flatpixels[segl->ceilingpicnum];
            Mars_ClearCacheLines(flat->data, (sizeof(flat->data)+31)/16);
        }
    }

    R_SegCommands();
}

#endif

