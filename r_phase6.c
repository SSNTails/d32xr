/*
  CALICO

  Renderer phase 6 - Seg Loop 
*/

#include "r_local.h"
#include "mars.h"

typedef struct drawtex_s
{
#ifdef MARS
   inpixel_t *data;
#else
   pixel_t   *data;
#endif
   int       width;
   int       height;
   int       topheight;
   int       bottomheight;
   int       texturemid;
   int       pixelcount;
   drawcol_t drawcol;
} drawtex_t;

typedef struct
{
    viswall_t* segl;

    drawtex_t tex[2];
    drawtex_t *first, *last;

    unsigned lightmin, lightmax, lightsub, lightcoef;
} seglocal_t;

static char seg_lock = 0;

static void R_DrawTextures(int x, int floorclipx, int ceilingclipx, fixed_t scale2, unsigned iscale, int colnum, unsigned light, seglocal_t* lsegl) ATTR_DATA_CACHE_ALIGN;
static void R_DrawSeg(seglocal_t* lseg, unsigned short *clipbounds) ATTR_DATA_CACHE_ALIGN;

static void R_LockSeg(void) ATTR_DATA_CACHE_ALIGN;
static void R_UnlockSeg(void) ATTR_DATA_CACHE_ALIGN;
void R_SegCommands(void) ATTR_DATA_CACHE_ALIGN;

//
// Render a wall texture as columns
//
static void R_DrawTextures(int x, int floorclipx, int ceilingclipx, fixed_t scale2, unsigned iscale, int colnum_, unsigned light, seglocal_t* lsegl)
{
   drawtex_t *tex = lsegl->first;

   for (; tex < lsegl->last; tex++) {
       int top, bottom;

       FixedMul2(top, scale2, tex->topheight);
       top = centerY - top;
       if (top < ceilingclipx)
           top = ceilingclipx;

       FixedMul2(bottom, scale2, tex->bottomheight);
       bottom = centerY - 1 - bottom;
       if (bottom >= floorclipx)
           bottom = floorclipx - 1;

       // column has no length?
       if (top <= bottom)
       {
           fixed_t frac = tex->texturemid - (centerY - top) * iscale;
#ifdef MARS
           inpixel_t* src;
#else
           pixel_t* src;
#endif
           int colnum = colnum_;

           // DEBUG: fixes green pixels in MAP01...
           frac += (iscale + (iscale >> 5) + (iscale >> 6));

           while (frac < 0)
           {
               colnum--;
               frac += tex->height << FRACBITS;
           }

           // CALICO: comment says this, but the code does otherwise...
           // colnum = colnum - tex->width * (colnum / tex->width)
           colnum &= (tex->width - 1);

           // CALICO: Jaguar-specific GPU blitter input calculation starts here.
           // We invoke a software column drawer instead.
           src = tex->data + colnum * tex->height;
           tex->drawcol(x, top, bottom, light, frac, iscale, src, tex->height, NULL);

           // pixel counter
           tex->pixelcount += (bottom - top);
       }
   }
}

//
// Main seg drawing loop
//
static void R_DrawSeg(seglocal_t* lseg, unsigned short *clipbounds)
{
   viswall_t* segl = lseg->segl;

   const volatile unsigned actionbits = segl->actionbits;

   unsigned scalefrac = segl->scalefrac;
   const volatile unsigned scalestep = segl->scalestep;

   const volatile unsigned centerangle = segl->centerangle;
   const volatile unsigned offset = segl->offset;
   const volatile unsigned distance = segl->distance;

   const volatile int ceilingheight = segl->ceilingheight;

   unsigned texturelight = lseg->lightmax;
   const boolean gradientlight = lseg->lightmin != lseg->lightmax;

   const volatile int stop = segl->stop;
   int x;

   for (x = segl->start; x <= stop; x++)
   {
      int floorclipx, ceilingclipx;
      unsigned scale2;

#ifdef MARS
      uintptr_t divisor;

      do {
          int32_t t;
          __asm volatile (
              "mov #0, %0\n\t"
              "mov #-128, %1\n\t"
              "add %1,%1\n\t"
              "mov.l %0, @(16,%1) /* set high bits of the 64-bit dividend */ \n\t"
              "mov.l %2, @(0,%1) /* set 32-bit divisor */ \n\t"
              "mov #-1, %0\n\t"
              "mov.l %0, @(20,%1) /* set low  bits of the 64-bit dividend, start divide */\n\t"
              : "=&r" (t), "=&r" (divisor)
              : "r" (scalefrac) );
      } while (0);
#else
      fixed_t scale = scalefrac;
#endif

      scale2 = (unsigned)scalefrac >> HEIGHTBITS;
      scalefrac += scalestep;

      //
      // get ceilingclipx and floorclipx from clipbounds
      //
      ceilingclipx = clipbounds[x];
      floorclipx = ceilingclipx & 0x00ff;
      ceilingclipx = (unsigned)ceilingclipx >> 8;

      //
      // texture only stuff
      //
      {
          unsigned colnum;
          unsigned iscale;
          fixed_t r;

          // calculate texture offset
          r = finetangent((centerangle + xtoviewangle[x]) >> ANGLETOFINESHIFT);
          FixedMul2(r, distance, r);

          // other texture drawing info
          colnum = (offset - r) >> FRACBITS;

          if (gradientlight)
          {
              // calc light level
              texturelight = scale2 * lseg->lightcoef;
              if (texturelight <= lseg->lightsub)
              {
                  texturelight = lseg->lightmin;
              }
              else
              {
                  texturelight -= lseg->lightsub;
                  texturelight /= FRACUNIT;
                  if (texturelight < lseg->lightmin)
                      texturelight = lseg->lightmin;
                  else if (texturelight > lseg->lightmax)
                      texturelight = lseg->lightmax;
              }

              // convert to a hardware value
              texturelight = HWLIGHT(texturelight);
          }

          //
          // draw textures
          //

#ifdef MARS
          do {
              __asm volatile (
                  "mov #-128, %1\n\t"
                  "add %1,%1\n\t"
                  "mov.l @(20,%1), %0 /* get 32-bit quotient */ \n\t"
                  : "=r" (iscale), "=&r" (divisor));
          } while (0);
#else
          iscale = 0xffffffffu / scale;
#endif
          R_DrawTextures(x, floorclipx, ceilingclipx, scale2, iscale, colnum, texturelight, lseg);
      }

      // sky mapping
      if (actionbits & AC_ADDSKY)
      {
          int top, bottom;

          top = ceilingclipx;
          FixedMul2(bottom, scale2, ceilingheight);
          bottom = (centerY - bottom) - 1;

          if (bottom >= floorclipx)
              bottom = floorclipx - 1;

          if (top <= bottom)
          {
              // CALICO: draw sky column
              int colnum = ((vd.viewangle + xtoviewangle[x]) >> ANGLETOSKYSHIFT) & 0xff;
#ifdef MARS
              inpixel_t* data = skytexturep->data + colnum * skytexturep->height;
#else
              pixel_t* data = skytexturep->data + colnum * skytexturep->height;
#endif
              drawcol(x, top, bottom, 0, (top * 18204) << 2, FRACUNIT + 7281, data, 128, NULL);
          }
      }
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

void R_SegCommands(void)
{
    viswall_t* segl;
    seglocal_t lseg;
    drawtex_t* toptex, * bottomtex;
    int extralight;
    unsigned clipbounds_[SCREENWIDTH / 2 + 1];
    unsigned short *clipbounds = (unsigned short *)clipbounds_;
    unsigned short *newclipbounds = segclip;

    // initialize the clipbounds array
    R_InitClipBounds(clipbounds_);

    extralight = vd.extralight;
    toptex = &lseg.tex[0];
    bottomtex = &lseg.tex[1];

    lseg.lightmin = 0;
    lseg.lightmax = 0;
    lseg.lightcoef = 0;
    lseg.lightsub = 0;

    // workaround annoying compilation warnings
    toptex->height = 0;
    toptex->pixelcount = 0;
    bottomtex->height = 0;
    bottomtex->pixelcount = 0;

    for (segl = viswalls; segl < lastwallcmd; segl++)
    {
        int seglight;
        unsigned actionbits;

        if (segl->start > segl->stop)
            continue;

#ifdef MARS
        do
        {
            actionbits = *(volatile short *)&segl->actionbits;
        } while ((actionbits & AC_READY) == 0);

        if (actionbits & AC_DRAWN || !(actionbits & (AC_TOPTEXTURE | AC_BOTTOMTEXTURE | AC_ADDSKY)))
        {
            goto skip_draw;
        }

        R_LockSeg();
        actionbits = *(volatile short *)&segl->actionbits;
        if (actionbits & AC_DRAWN) {
            R_UnlockSeg();
            goto skip_draw;
        } else {
            segl->actionbits = actionbits|AC_DRAWN;
            R_UnlockSeg();
        }
#endif

        lseg.segl = segl;
        lseg.first = lseg.tex + 1;
        lseg.last = lseg.tex + 1;

        if (vd.fixedcolormap)
        {
            lseg.lightmin = lseg.lightmax = vd.fixedcolormap;
        }
        else
        {
            seglight = segl->seglightlevel + extralight;
#ifdef MARS
            if (seglight < 0)
                seglight = 0;
            if (seglight > 255)
                seglight = 255;
#endif
            lseg.lightmax = seglight;
            lseg.lightmin = lseg.lightmax;

            if (detailmode == detmode_high)
            {
#ifdef MARS
                if (seglight <= 160 + extralight)
                    seglight = (seglight >> 1);
#else
                seglight = seglight - (255 - seglight) * 2;
                if (seglight < 0)
                    seglight = 0;
#endif
                lseg.lightmin = seglight;
            }

            if (lseg.lightmin != lseg.lightmax)
            {
                lseg.lightcoef = ((lseg.lightmax - lseg.lightmin) << FRACBITS) / (800 - 160);
                lseg.lightsub = 160 * lseg.lightcoef;
            }
            else
            {
                lseg.lightmin = HWLIGHT(lseg.lightmax);
                lseg.lightmax = lseg.lightmin;
            }
        }

        if (actionbits & AC_TOPTEXTURE)
        {
            texture_t* tex = &textures[segl->t_texturenum];
            toptex->topheight = segl->t_topheight;
            toptex->bottomheight = segl->t_bottomheight;
            toptex->texturemid = segl->t_texturemid;
            toptex->width = tex->width;
            toptex->height = tex->height;
            toptex->data = tex->data;
            toptex->drawcol = (tex->height & (tex->height - 1)) ? drawcolnpo2 : drawcol;
            lseg.first--;
        }

        if (actionbits & AC_BOTTOMTEXTURE)
        {
            texture_t* tex = &textures[segl->b_texturenum];
            bottomtex->topheight = segl->b_topheight;
            bottomtex->bottomheight = segl->b_bottomheight;
            bottomtex->texturemid = segl->b_texturemid;
            bottomtex->width = tex->width;
            bottomtex->height = tex->height;
            bottomtex->data = tex->data;
            bottomtex->drawcol = (tex->height & (tex->height - 1)) ? drawcolnpo2 : drawcol;
            lseg.last++;
        }

        R_DrawSeg(&lseg, clipbounds);

        segl->t_topheight = toptex->pixelcount;
        segl->b_topheight = bottomtex->pixelcount;

skip_draw:
        if(actionbits & (AC_NEWFLOOR|AC_NEWCEILING))
        {
            int i;
            int x = segl->start;
            int width = segl->stop - segl->start + 1;
            for (i = 0; i < width; i++, x++)
                clipbounds[x] = newclipbounds[i];
            newclipbounds += width;
        }
    }
}

#ifdef MARS

void Mars_Sec_R_SegCommands(void)
{
    R_SegCommands();
}

#endif

