/*
  CALICO

  Renderer phase 2 - Wall prep
*/

#include "doomdef.h"
#include "r_local.h"
#include "mars.h"

static fixed_t R_PointToDist(fixed_t x, fixed_t y) ATTR_DATA_CACHE_ALIGN;
static fixed_t R_ScaleFromGlobalAngle(fixed_t rw_distance, angle_t visangle, angle_t normalangle) ATTR_DATA_CACHE_ALIGN;
static void R_SetupCalc(viswall_t* wc, fixed_t hyp, angle_t normalangle, int angle1) ATTR_DATA_CACHE_ALIGN;
void R_WallLatePrep(viswall_t* wc, mapvertex_t *verts) ATTR_DATA_CACHE_ALIGN;
static void R_SegLoop(viswall_t* segl, unsigned short* clipbounds, fixed_t floorheight, 
    fixed_t floornewheight, fixed_t ceilingheight, fixed_t ceilingnewheight, fixed_t segexdata) ATTR_DATA_CACHE_ALIGN __attribute__((noinline));
void R_WallPrep(void) ATTR_DATA_CACHE_ALIGN __attribute__((noinline));

//
// Get distance to point in 3D projection
//
static fixed_t R_PointToDist(fixed_t x, fixed_t y)
{
    angle_t angle;
    fixed_t dx, dy, temp;

    dx = D_abs(x - vd.viewx);
    dy = D_abs(y - vd.viewy);

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
static fixed_t R_ScaleFromGlobalAngle(fixed_t rw_distance, angle_t visangle, angle_t normalangle)
{
    angle_t anglea, angleb;
    fixed_t num, den;
    int     sinea, sineb;

    visangle += ANG90;

    anglea = visangle - vd.viewangle;
    sinea = finesine(anglea >> ANGLETOFINESHIFT);
    angleb = visangle - normalangle;
    sineb = finesine(angleb >> ANGLETOFINESHIFT);

    num = FixedMul(stretchX, sineb);
    den = FixedMul(rw_distance, sinea);

    return FixedDiv(num, den);
}

//
// Setup texture calculations for lines with upper and lower textures
//
static void R_SetupCalc(viswall_t* wc, fixed_t hyp, angle_t normalangle, int angle1)
{
    fixed_t sineval, rw_offset;
    angle_t offsetangle;

    offsetangle = normalangle - angle1;

    if (offsetangle > ANG180)
        offsetangle = 0 - offsetangle;

    if (offsetangle > ANG90)
        offsetangle = ANG90;

    sineval = finesine(offsetangle >> ANGLETOFINESHIFT);
    rw_offset = FixedMul(hyp, sineval);

    if (normalangle - angle1 < ANG180)
        rw_offset = -rw_offset;

    wc->offset += rw_offset;
    wc->centerangle = ANG90 + vd.viewangle - normalangle;
}

void R_WallLatePrep(viswall_t* wc, mapvertex_t *verts)
{
    angle_t      distangle, offsetangle, normalangle;
    const seg_t* seg = wc->seg;
    angle_t          angle1 = wc->scalestep;
    fixed_t      sineval, rw_distance;
    fixed_t      scalefrac, scale2;
    fixed_t      hyp;
    fixed_t      x1, y1, x2, y2;

    // this is essentially R_StoreWallRange
    // calculate rw_distance for scale calculation

    x1 = verts[seg->v1].x << FRACBITS;
    y1 = verts[seg->v1].y << FRACBITS;

    x2 = verts[seg->v2].x << FRACBITS;
    y2 = verts[seg->v2].y << FRACBITS;

    hyp = R_PointToDist(x1, y1);

    normalangle = R_PointToAngle2(x1, y1, x2, y2);
    normalangle += ANG90;
    offsetangle = normalangle - angle1;

    if ((int)offsetangle < 0)
        offsetangle = 0 - offsetangle;

    if (offsetangle > ANG90)
        offsetangle = ANG90;

    distangle = ANG90 - offsetangle;
    sineval = finesine(distangle >> ANGLETOFINESHIFT);
    rw_distance = FixedMul(hyp, sineval);
    wc->distance = rw_distance;

    scalefrac = scale2 = wc->scalefrac =
        R_ScaleFromGlobalAngle(rw_distance, vd.viewangle + (xtoviewangle[wc->start]<<FRACBITS), normalangle);

    if (wc->stop > wc->start)
    {
        scale2 = R_ScaleFromGlobalAngle(rw_distance, vd.viewangle + (xtoviewangle[wc->stop]<<FRACBITS), normalangle);
#ifdef MARS
        SH2_DIVU_DVSR = wc->stop - wc->start;  // set 32-bit divisor
        SH2_DIVU_DVDNT = scale2 - scalefrac;   // set 32-bit dividend, start divide
#else
        wc->scalestep = IDiv(scale2 - scalefrac, wc->stop - wc->start);
#endif
    }

    wc->scale2 = scale2;

    // does line have top or bottom textures?
    if (wc->actionbits & (AC_TOPTEXTURE | AC_BOTTOMTEXTURE | AC_MIDTEXTURE | AC_FOFSIDE))
    {
        R_SetupCalc(wc, hyp, normalangle, angle1);// do calc setup
    }

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

    const VINT floorandlight = ((segl->seglightlevel & 0xff) << 8) | (VINT)segl->floorpicnum;
    const VINT ceilandlight = ((segl->seglightlevel & 0xff) << 8) | (VINT)segl->ceilpicnum;

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

        //
        // get ceilingclipx and floorclipx from clipbounds
        //
        ceilingclipx = clipbounds[x];
        floorclipx = ceilingclipx & 0x00ff;
        ceilingclipx = ((unsigned)ceilingclipx & 0xff00) >> 8;

        cy = cyvh >> 16;
        vh = cyvh & 0xffff;

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

        if (newclipbounds)
        {
            newclipbounds[x] = (high << 8) | low;
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
                    visplane_t *floor = R_FindPlane(floorheight, floorandlight, x, stop, segl->floor_offs);
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
                    visplane_t *ceiling = R_FindPlane(ceilingheight, ceilandlight, x, stop, 0);
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
                    const VINT fofandlight = ((backFOF->lightlevel & 0xff) << 8) | flattranslation[backFOF->floorpic];
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
                        visplane_t *fofplane = R_FindPlaneFOF(fofplaneHeight, fofandlight, x, stop, 0);
                        unsigned short *fof_bottomopen = fofplane->open;

                        SETUPPER8(fof_bottomopen[x], top);
                    }
                }
                else if (backFOF->ceilingheight < vd.viewz) // Top of FOF is visible
                {
                    const VINT fofandlight = ((255 & 0xff) << 8) | flattranslation[backFOF->ceilingpic];
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
                        visplane_t *fofplane = R_FindPlaneFOF(fofplaneHeight, fofandlight, x, stop, backFOF->floor_xoffs);
                        unsigned short *fof_topopen = fofplane->open;

//                        SETUPPER8(fof_topopen[x], 0xff);

                        if (LOWER8(fof_topopen[x]) == 0)
                            SETLOWER8(fof_topopen[x], bottom-1);
                    }
                }
            }
            if (actionbits & AC_FOFBOTTOM) // Bottom of FOF is visible
            {
                const VINT fofandlight = ((frontFOF->lightlevel & 0xff) << 8) | flattranslation[frontFOF->floorpic];
                const fixed_t fofplaneHeight = frontFOF->floorheight - vd.viewz;

                // "ceilopen"
                top = ceilingclipx;
                bottom = FixedMul(scale2, fofplaneHeight)>>FRACBITS;
                bottom = cy - bottom;
                if (bottom > floorclipx)
                    bottom = floorclipx;

                if (top < bottom)
                {
                    visplane_t *fofplane = R_FindPlaneFOF(fofplaneHeight, fofandlight, x, stop, 0);
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
                const VINT fofandlight = ((255 & 0xff) << 8) | flattranslation[frontFOF->ceilingpic];
                const fixed_t fofplaneHeight = frontFOF->ceilingheight - vd.viewz;

                // "flooropen"
                top = FixedMul(scale2, fofplaneHeight)>>FRACBITS;
                top = cy - top;
                if (top < ceilingclipx)
                    top = ceilingclipx;
                bottom = floorclipx;

                if (top < bottom)
                {
                    visplane_t *fofplane = R_FindPlaneFOF(fofplaneHeight, fofandlight, x, stop, frontFOF->floor_xoffs);
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
    volatile uint8_t *addedsegs = (volatile uint8_t *)&MARS_SYS_COMM6;
    volatile uint8_t *readysegs = addedsegs + 1;

    R_InitClipBounds(clipbounds_);

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
            R_WallLatePrep(segl, verts);

            R_SegLoop(segl, clipbounds, seglex->floorheight, seglex->floornewheight, segl->ceilingheight, seglex->ceilnewheight, seglex->fofInfo);

            seglex++;
            *readysegs = *readysegs + 1;
        }
    }
}

#else

void R_WallPrep(void)
{
    viswall_t* segl;
    unsigned clipbounds_[SCREENWIDTH/2+1];
    unsigned short *clipbounds = (unsigned short *)clipbounds_;

    R_InitClipBounds(clipbounds_);

    for (segl = vd.viswalls; segl != vd.lastwallcmd; segl++)
    {
        fixed_t floornewheight = 0, ceilingnewheight = 0;

        R_WallLatePrep(segl, vertexes);

        R_SegLoop(segl, clipbounds, floornewheight, ceilingnewheight);
    }
}

#endif

// EOF

