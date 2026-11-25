/*
  CALICO

  Renderer phase 2 - Wall prep
*/

#include "doomdef.h"
#include "r_local.h"
#include "mars.h"

static fixed_t R_ScaleFromGlobalAngle(fixed_t invdist, fixed_t sineb, int x) ATTR_DATA_CACHE_ALIGN;
// consecutive viswalls are often fragments of the same seg (long segs
// split in R_StoreWallRange, or segs cut into pieces by the solid seg
// clipper); everything that doesn't depend on the column range is then
// taken from the previous viswall. wc->seg doesn't survive R_WallLatePrep
// (it's unioned with v1i), so the caller keeps the last seg in *prevseg
static void R_WallLatePrep(viswall_t* wc, mapvertex_t *verts, seg_t **prevseg) ATTR_DATA_CACHE_ALIGN;
static void R_SegLoop(viswall_t* segl, unsigned short* clipbounds, fixed_t floorheight, 
    fixed_t floornewheight, fixed_t ceilingheight, fixed_t ceilingnewheight, fixed_t segexdata) ATTR_DATA_CACHE_ALIGN __attribute__((noinline));
void R_WallPrep(void) ATTR_DATA_CACHE_ALIGN __attribute__((noinline));

//
// Gets accurate distance of dx, dy
// Use P_AproxDistance whenever possible.
//
fixed_t R_PointToDist2(fixed_t dx, fixed_t dy)
{
    angle_t angle;
    fixed_t temp;

    dx = D_abs(dx);
	dy = D_abs(dy);

    if (dy > dx)
    {
        temp = dx;
        dx = dy;
        dy = temp;
    }

    angle = (tantoangle[(unsigned)FixedDiv(dy, dx) >> DBITS] + ANG90) >> ANGLETOFINESHIFT;

    // use as cosine
    return FixedDiv(dx, finesine(angle));
}

//
// Convert angle and distance within view frustum to texture scale factor.
//
// scale = stretchX * sin(visangle + ANG90 - normalangle) / (rw_distance * cos(xtoviewangle[x]))
//
// invdist is stretchX / rw_distance, computed once per wall, sineb is
// sin(visangle + ANG90 - normalangle), and 1/cos(xtoviewangle[x]) is the
// precomputed distscale[x] table (stored >> 1). The caller looks up sineb
// while the hardware divider is still working on invdist.
//
static fixed_t R_ScaleFromGlobalAngle(fixed_t invdist, fixed_t sineb, int x)
{
    return FixedMul(FixedMul(invdist, sineb), (fixed_t)distscale[x] << 1);
}

static void R_WallLatePrep(viswall_t* wc, mapvertex_t *verts, seg_t **prevseg)
{
    seg_t* seg = wc->seg;
    fixed_t      scalefrac, scale2;
    fixed_t      invdist;
    angle_t      baseangle;
    fixed_t      sineb1, sineb2;
    fixed_t      rw_distance, offset;
    const boolean newseg = seg != *prevseg;

    // this is essentially R_StoreWallRange
    if (newseg)
    {
        angle_t      normalangle;
        fixed_t      rw_offset, dx, dy;
        fixed_t      x1, y1, x2, y2;

        // calculate rw_distance for scale calculation
        x1 = verts[seg->v1].x << FRACBITS;
        y1 = verts[seg->v1].y << FRACBITS;

        x2 = verts[seg->v2].x << FRACBITS;
        y2 = verts[seg->v2].y << FRACBITS;

        normalangle = R_PointToAngle2(x1, y1, x2, y2);
        normalangle += ANG90;

        // project the view->v1 vector onto the wall normal (distance) and
        // the wall direction (texture offset); axis-aligned walls are exact
        dx = x1 - vd.viewx;
        dy = y1 - vd.viewy;
        if (x1 == x2)
        {
            rw_distance = D_abs(dx);
            rw_offset = y2 > y1 ? -dy : dy;
        }
        else if (y1 == y2)
        {
            rw_distance = D_abs(dy);
            rw_offset = x2 > x1 ? -dx : dx;
        }
        else
        {
            fixed_t sinn = finesine(normalangle >> ANGLETOFINESHIFT);
            fixed_t cosn = finecosine(normalangle >> ANGLETOFINESHIFT);
            rw_distance = FixedMul(dx, cosn) + FixedMul(dy, sinn);
            if (rw_distance < 0)
                rw_distance = 0;
            rw_offset = FixedMul(dy, cosn) - FixedMul(dx, sinn);
        }

        baseangle = vd.viewangle + ANG90 - normalangle;
        *prevseg = seg;

        // only read by R_DrawSeg for textured walls, so set them
        // unconditionally; P_SegOffset is added while the divider runs
        offset = ((fixed_t)wc->offset << FRACBITS) + rw_offset;
        wc->centerangle = baseangle;
        // unpack the vertices for walls that clip sprites in phase 8;
        // this overwrites wc->seg (union with v1i), which isn't needed past here
        volatile int v1 = wc->seg->v1, v2 = wc->seg->v2;
        wc->v1.x = verts[v1].x, wc->v1.y = verts[v1].y;
        wc->v2.x = verts[v2].x, wc->v2.y = verts[v2].y;
    }
    else
    {
        // same seg as the previous viswall, so the same sidedef texture
        // offset too; R_SegLoop doesn't modify these
        const viswall_t *prev = wc - 1;
        rw_distance = prev->distance;
        offset = prev->offset;
        wc->centerangle = baseangle = prev->centerangle;
        // this overwrites wc->seg (union with v1i), which isn't needed past here
        wc->v2 = prev->v2;
        wc->v1 = prev->v1;
    }
#ifdef WALLDRAW2X
    wc->distance = rw_distance >> 1;
#else
    wc->distance = rw_distance;
#endif

#ifdef MARS
    // invdist = FixedDiv(stretchX, rw_distance), started on the divider here
    // and collected after the sine lookups, which don't depend on it and
    // hide most of its 39-cycle latency. Nothing in between may use the divider.
    SH2_DIVU_DVSR = rw_distance;           // set 32-bit divisor
    SH2_DIVU_DVDNTH = stretchX >> 16;      // high long of 64-bit dividend
    SH2_DIVU_DVDNTL = stretchX << 16;      // low long, starts the divide
#else
    invdist = FixedDiv(stretchX, rw_distance);
#endif

    if (newseg)
        offset += (fixed_t)(seg->sideoffset >> 1) << FRACBITS;
    wc->offset = offset;

    sineb1 = finesine((baseangle + (xtoviewangle[wc->start]<<FRACBITS)) >> ANGLETOFINESHIFT);
    sineb2 = sineb1;
    if (wc->stop > wc->start)
        sineb2 = finesine((baseangle + (xtoviewangle[wc->stop]<<FRACBITS)) >> ANGLETOFINESHIFT);

#ifdef MARS
    invdist = SH2_DIVU_DVDNTL;             // get 32-bit quotient (stalls if not done)
#endif
    // the divide saturates when rw_distance is ~0 (standing on the line);
    // clamp so the multiply by distscale (up to ~1.42) can't overflow
    if ((unsigned)invdist > 0x50000000)
        invdist = 0x50000000;

    scalefrac = scale2 = wc->scalefrac =
        R_ScaleFromGlobalAngle(invdist, sineb1, wc->start);

    if (wc->stop > wc->start)
    {
        scale2 = R_ScaleFromGlobalAngle(invdist, sineb2, wc->stop);
#ifdef MARS
        SH2_DIVU_DVSR = wc->stop - wc->start;  // set 32-bit divisor
        SH2_DIVU_DVDNT = scale2 - scalefrac;   // set 32-bit dividend, start divide
#else
        wc->scalestep = IDiv(scale2 - scalefrac, wc->stop - wc->start);
#endif
    }

    wc->scale2 = scale2;

    wc->clipbounds = NULL;

    const int start = wc->start;
    const int stop = wc->stop;
    const int width = stop - start + 1;
    if (wc->actionbits & (AC_NEWFLOOR | AC_NEWCEILING | AC_TOPSIL | AC_BOTTOMSIL | AC_MIDTEXTURE | AC_FOFSIDE))
    {
        wc->clipbounds = vd.lastsegclip - start;
        vd.lastsegclip += width;
    }
    if ((wc->actionbits & AC_MIDTEXTURE) || (wc->actionbits & AC_FOFSIDE))
    {
        // lighting + column
        D_memset(vd.lastsegclip, 255, sizeof(*vd.lastsegclip)*width);
        vd.lastsegclip += width;
    }

#ifdef MARS
    if (wc->stop > wc->start)
    {
        wc->scalestep = SH2_DIVU_DVDNT; // get 32-bit quotient
    }
#endif
}

//
// Main seg clipping loop
//
static void R_SegLoop(viswall_t* segl, unsigned short* clipbounds, 
    fixed_t floorheight, fixed_t floornewheight, fixed_t ceilingheight, fixed_t ceilingnewheight, fixed_t segexdata)
{
    const volatile int actionbits = segl->actionbits;

    fixed_t scalefrac = segl->scalefrac;
    volatile const fixed_t scalestep = segl->scalestep;

    int x, start = segl->start;
    const int stop = segl->stop;

//    const fixed_t ceilingheight = segl->ceilingheight;

    visplane_key_t floorKey = { .parts.flat = segl->floorpicnum & 0xff, .parts.light = segl->seglightlevel & 0xff, .parts.offs = segl->floor_offs };
    visplane_key_t ceilingKey = { .parts.flat = segl->ceilpicnum & 0xff, .parts.light = segl->seglightlevel & 0xff, .parts.offs = 0 };

    unsigned short *flooropen = (actionbits & AC_ADDFLOOR) ? vd.visplanes[0].open : NULL;
    unsigned short *ceilopen = (actionbits & AC_ADDCEILING) ? vd.visplanes[0].open : NULL;

    unsigned short *newclipbounds = segl->clipbounds;

    const int cyvh = (centerY << 16) | viewportHeight;

    sector_t *frontFOF = segexdata >= 0 ? I_TO_SEC(segexdata) : NULL;
    sector_t *backFOF = segl->fofSector >= 0 ? I_TO_SEC(segl->fofSector) : NULL;

    for (x = start; x <= stop; x++)
    {
        int floorclipx, ceilingclipx;
        int low, high, top, bottom;
        int cy, vh;
        fixed_t scale2;

        scale2 = scalefrac;
        scalefrac += scalestep;
#ifdef MARS
        // keep GCC from turning the FixedMul()s below into 64-bit running sums
        // (strength reduction), which the SH-2 has to keep on the stack
        __asm volatile ("" : "+r" (scale2));
#endif

        //
        // get ceilingclipx and floorclipx from clipbounds
        //
        ceilingclipx = clipbounds[x];
        floorclipx = ceilingclipx & 0x00ff;
        ceilingclipx = ((unsigned)ceilingclipx & 0xff00) >> 8;

        cy = cyvh >> 16;
        vh = cyvh & 0xffff;

        if (newclipbounds)
        {
            const int newclip = actionbits & (AC_NEWFLOOR|AC_NEWCEILING);

            //
            // calc high and low
            //
            low = FixedMul(scale2, floornewheight)>>FRACBITS;
            low = cy - low;
            if (low < 0)
                low = 0;
            else if (low > floorclipx)
                low = floorclipx;

            high = FixedMul(scale2, ceilingnewheight)>>FRACBITS;
            high = cy - high;
            if (high > vh)
                high = vh;
            else if (high < ceilingclipx)
                high = ceilingclipx;

            newclipbounds[x] = (high << 8) | low;

            if (!(newclip & AC_NEWFLOOR))
                low = floorclipx;
            if (!(newclip & AC_NEWCEILING))
                high = ceilingclipx;
            // rewrite clipbounds
            clipbounds[x] = (high << 8) | low;
        }

        int newclip = actionbits & (AC_NEWFLOOR|AC_NEWCEILING);
        if (newclip)
        {
            if (!(newclip & AC_NEWFLOOR))
                low = floorclipx;
            if (!(newclip & AC_NEWCEILING))
                high = ceilingclipx;
            // rewrite clipbounds
            clipbounds[x] = (high << 8) | low;
        }

        //
        // floor
        //
        if (flooropen)
        {
            top = FixedMul(scale2, floorheight)>>FRACBITS;
            top = cy - top;
            if (top < ceilingclipx)
                top = ceilingclipx;
            bottom = floorclipx;

            if (top < bottom)
            {
                if (!MARKEDOPEN(flooropen[x]))
                {
                    visplane_t *floor = R_FindPlane(floorheight, floorKey.compare, x, stop);
                    flooropen = floor->open;
                }
                flooropen[x] = (top << 8) + (bottom-1);
            }
        }

        //
        // ceiling
        //
        if (ceilopen)
        {
            top = ceilingclipx;
            bottom = FixedMul(scale2, ceilingheight)>>FRACBITS;
            bottom = cy - bottom;
            if (bottom > floorclipx)
                bottom = floorclipx;

            if (top < bottom)
            {
                if (!MARKEDOPEN(ceilopen[x]))
                {
                    visplane_t *ceiling = R_FindPlane(ceilingheight, ceilingKey.compare, x, stop);
                    ceilopen = ceiling->open;
                }
                ceilopen[x] = (top << 8) + (bottom-1);
            }
        }

#ifdef FLOOR_OVER_FLOOR
//        if (actionbits & (AC_FOFBOTTOM|AC_FOFTOP|AC_FOFSIDE))
        {
            if (actionbits & AC_FOFSIDE)
            {
                if (backFOF->floorheight > vd.viewz) // Bottom of FOF is visible
                {
                    const fixed_t fofplaneHeight = backFOF->floorheight - vd.viewz;

                    // "ceilopen"
                    if (!frontFOF && ceilingheight < fofplaneHeight)
                        top = FixedMul(scale2, ceilingheight)>>FRACBITS;
                    else if (frontFOF && frontFOF->floorheight != fofplaneHeight)
                        top = FixedMul(scale2, frontFOF->floorheight - vd.viewz)>>FRACBITS;
                    else
                        top = FixedMul(scale2, fofplaneHeight)>>FRACBITS;

                    top = cy - top;
                    if (top < ceilingclipx)
                        top = ceilingclipx;
                    bottom = floorclipx;

//                    if (top < bottom)
                    {
                        visplane_key_t key = { .parts.flat = flattranslation[backFOF->floorpic], .parts.light = backFOF->lightlevel, .parts.offs = 0 };
                        visplane_t *fofplane = R_FindPlaneFOF(fofplaneHeight, key.compare, x, stop);
                        unsigned short *fof_bottomopen = fofplane->open;

                        SETUPPER8(fof_bottomopen[x], top);
                    }
                }
                else if (backFOF->ceilingheight < vd.viewz) // Top of FOF is visible
                {
                    const fixed_t fofplaneHeight = backFOF->ceilingheight - vd.viewz;

                    top = ceilingclipx;
                    
                    if (!frontFOF && floorheight > fofplaneHeight)
                        bottom = FixedMul(scale2, floorheight)>>FRACBITS;
                    else if (frontFOF && frontFOF->ceilingheight != fofplaneHeight)
                        bottom = FixedMul(scale2, frontFOF->ceilingheight - vd.viewz)>>FRACBITS;
                    else
                        bottom = FixedMul(scale2, fofplaneHeight)>>FRACBITS;

                    bottom = cy - bottom;
                    if (bottom > floorclipx)
                        bottom = floorclipx;

                    if (top < bottom)
                    {
                        const visplane_key_t key = { .parts.flat = flattranslation[backFOF->ceilingpic], .parts.light = segl->seglightlevel & 0xff, .parts.offs = backFOF->floor_xoffs };

                        visplane_t *fofplane = R_FindPlaneFOF(fofplaneHeight, key.compare, x, stop);
                        unsigned short *fof_topopen = fofplane->open;

//                        SETUPPER8(fof_topopen[x], 0xff);

                        if (LOWER8(fof_topopen[x]) == 0)
                            SETLOWER8(fof_topopen[x], bottom-1);
                    }
                }
            }
            if (actionbits & AC_FOFBOTTOM) // Bottom of FOF is visible
            {
                const fixed_t fofplaneHeight = frontFOF->floorheight - vd.viewz;

                // "ceilopen"
                top = ceilingclipx;
                bottom = FixedMul(scale2, fofplaneHeight)>>FRACBITS;
                bottom = cy - bottom;
                if (bottom > floorclipx)
                    bottom = floorclipx;

                if (top < bottom)
                {
                    const visplane_key_t key = { .parts.flat = flattranslation[frontFOF->floorpic], .parts.light = frontFOF->lightlevel, .parts.offs = 0 };
                    visplane_t *fofplane = R_FindPlaneFOF(fofplaneHeight, key.compare, x, stop);
                    unsigned short *fof_bottomopen = fofplane->open;

                    if (fof_bottomopen[x] == 0xff00)
                    {
                        SETUPPER8(fof_bottomopen[x], 0);
                        SETLOWER8(fof_bottomopen[x], bottom-1);
                    }
                    else if (UPPER8(fof_bottomopen[x]) == 0)
                        fof_bottomopen[x] = 0xff00;
                    

                    SETLOWER8(fof_bottomopen[x], bottom-1);

                    if (UPPER8(fof_bottomopen[x]) == 0xff)
                        SETUPPER8(fof_bottomopen[x], top);
                }
            }
            else if (actionbits & AC_FOFTOP) // Top of FOF is visible
            {
                const fixed_t fofplaneHeight = frontFOF->ceilingheight - vd.viewz;

                // "flooropen"
                top = FixedMul(scale2, fofplaneHeight)>>FRACBITS;
                top = cy - top;
                if (top < ceilingclipx)
                    top = ceilingclipx;
                bottom = floorclipx;

                if (top < bottom)
                {
                    const visplane_key_t key = { .parts.flat = flattranslation[frontFOF->ceilingpic], .parts.light = segl->seglightlevel & 0xff, .parts.offs = frontFOF->floor_xoffs };

                    visplane_t *fofplane = R_FindPlaneFOF(fofplaneHeight, key.compare, x, stop);
                    unsigned short *fof_topopen = fofplane->open;

                    SETUPPER8(fof_topopen[x], top);

                    if (LOWER8(fof_topopen[x]) == 0)
                        SETLOWER8(fof_topopen[x], bottom-1);
                }
            }
        }
#endif
    }
}

#ifdef MARS

void Mars_Sec_R_WallPrep(void)
{
    viswall_t *segl;
    viswallextra_t *seglex;
    viswall_t *first, *last, *verylast;
    uint32_t clipbounds_[SCREENWIDTH/2+1];
    uint16_t *clipbounds = (uint16_t *)clipbounds_;
    mapvertex_t *verts;
    seg_t *prevseg;
    volatile uint8_t *addedsegs = (volatile uint8_t *)&MARS_SYS_COMM6;
    volatile uint8_t *readysegs = addedsegs + 1;

    R_InitClipBounds(clipbounds_);

    prevseg = NULL;

    first = last = vd.viswalls;
    verylast = NULL;
    seglex = vd.viswallextras;
    verts = /*W_POINTLUMPNUM(gamemaplump+ML_VERTEXES)*/vertexes;

    for (segl = first; segl != verylast; )
    {
        int8_t nextsegs = *addedsegs;

        // check if master CPU finished exec'ing R_BSP()
        if (nextsegs == -2)
        {
            Mars_ClearCacheLine(&vd.lastwallcmd);
            verylast = vd.lastwallcmd;
            last = verylast;
        }
        else
        {
            last = first + (uint8_t)nextsegs;
        }

        for (; segl < last; segl++)
        {
#ifdef MARS
            Mars_ClearCacheLine(seglex);
#endif
            R_WallLatePrep(segl, verts, &prevseg);

            R_SegLoop(segl, clipbounds, seglex->floorheight, seglex->floornewheight, segl->ceilingheight, seglex->ceilnewheight, seglex->fofInfo);

            seglex++;
            *readysegs = *readysegs + 1;
        }

        SH2_Delay64();
    }
}

#else

void R_WallPrep(void)
{
    viswall_t* segl;
    unsigned clipbounds_[SCREENWIDTH/2+1];
    unsigned short *clipbounds = (unsigned short *)clipbounds_;
    seg_t *prevseg;

    R_InitClipBounds(clipbounds_);
    prevseg = NULL;

    for (segl = vd.viswalls; segl != vd.lastwallcmd; segl++)
    {
        fixed_t floornewheight = 0, ceilingnewheight = 0;

        R_WallLatePrep(segl, vertexes, &prevseg);

        R_SegLoop(segl, clipbounds, floornewheight, ceilingnewheight);
    }
}

#endif

// EOF

