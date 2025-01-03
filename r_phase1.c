/*
  CALICO

  Renderer phase 1 - BSP traversal
*/

#include "doomdef.h"
#include "r_local.h"
#include "p_local.h"
#ifdef MARS
#include "mars.h"
#endif

typedef struct
{
   VINT first;
   VINT last;
} cliprange_t
__attribute__((aligned(4)))
;

#define MAXSEGS 32

typedef struct
{
   cliprange_t *newend;
   cliprange_t *solidsegs;
   seg_t       *curline;
   sector_t    *curfsector, *curbsector;
   side_t      *curside;
   line_t      *curldef;
   angle_t    lineangle1;
   int        splitspans; /* generate wall splits until this reaches 0 */
   VINT lastv1;
   VINT lastv2;
   angle_t lastangle1;
   angle_t lastangle2;
} rbspWork_t;

static int R_ClipToViewEdges(angle_t angle1, angle_t angle2) ATTR_DATA_CACHE_ALIGN;
static void R_AddLine(rbspWork_t *rbsp, seg_t* line) ATTR_DATA_CACHE_ALIGN;
static void R_ClipWallSegment(rbspWork_t *rbsp, fixed_t first, fixed_t last, boolean solid) ATTR_DATA_CACHE_ALIGN;
static boolean R_CheckBBox(rbspWork_t *rbsp, int16_t bspcoord[4]) ATTR_DATA_CACHE_ALIGN;
static void R_Subsector(rbspWork_t *rbsp, int num) ATTR_DATA_CACHE_ALIGN;
static void R_StoreWallRange(rbspWork_t *rbsp, int start, int stop) ATTR_DATA_CACHE_ALIGN;
static void R_RenderBSPNode(rbspWork_t *rbsp, int bspnum, int16_t *outerbbox) ATTR_DATA_CACHE_ALIGN;
static void R_WallEarlyPrep(rbspWork_t *rbsp, viswall_t* segl,
   fixed_t *restrict floorheight, fixed_t *restrict floornewheight, 
   fixed_t *restrict ceilingnewheight, uint32_t *restrict fofInfo) ATTR_DATA_CACHE_ALIGN  __attribute__((noinline));

#ifdef MARS
__attribute__((aligned(4)))
#endif
static VINT checkcoord[12][4] =
{
   { 3, 0, 2, 1 },
   { 3, 0, 2, 0 },
   { 3, 1, 2, 0 },
   { 0, 0, 0, 0 },
   { 2, 0, 2, 1 },
   { 0, 0, 0, 0 },
   { 3, 1, 3, 0 },
   { 0, 0, 0, 0 },
   { 2, 0, 3, 1 },
   { 2, 1, 3, 1 },
   { 2, 1, 3, 0 },
   { 0, 0, 0, 0 }
};

static sector_t emptysector = { 0, 0, -2, -2, -2, 0, 0, 0, 0, -1 };

static int R_ClipToViewEdges(angle_t angle1, angle_t angle2)
{
   angle_t span, tspan;
   int x1, x2;
   angle_t doubleclipangle;

   // clip to view edges
   span = angle1 - angle2;
   if(span >= ANG180)
      return 0;

   angle1 -= vd.viewangle;
   angle2 -= vd.viewangle;
   doubleclipangle = vd.clipangle * 2;

   tspan = angle1 + vd.clipangle;
   if(tspan > doubleclipangle)
   {
      tspan -= doubleclipangle;
      // totally off the left edge?
      if(tspan >= span)
         return -1;
      angle1 = vd.clipangle;
   }

   tspan = vd.clipangle - angle2;
   if(tspan > doubleclipangle)
   {
      tspan -= doubleclipangle;
      if(tspan >= span)
         return -1;
      angle2 = 0 - vd.clipangle;
   }

   // find the first clippost that touches the source post (adjacent pixels
   // are touching).
   angle1 = (angle1 + ANG90) >> ANGLETOFINESHIFT;
   angle2 = (angle2 + ANG90) >> ANGLETOFINESHIFT;
   x1    = vd.viewangletox[angle1];
   x2    = vd.viewangletox[angle2];

   // does not cross a pixel?
   if(x1 == x2)
      return -1;

   return (x1<<16) | x2;
}

//
// Checks BSP node/subtree bounding box. Returns true if some part of the bbox
// might be visible.
//
static boolean R_CheckBBox(rbspWork_t *rbsp, int16_t bspcoord[4])
{
   int boxx;
   int boxy;
   int boxpos;

   fixed_t x1, y1, x2, y2;

   angle_t angle1, angle2;

   cliprange_t *start;

   int sx1, sx2;

   // find the corners of the box that define the edges from current viewpoint
   boxx = 2;
   boxx -= (vd.viewx < (bspcoord[BOXRIGHT]<<FRACBITS));
   boxx -= (vd.viewx <= (bspcoord[BOXLEFT]<<FRACBITS));

   boxy = 2;
   boxy -= (vd.viewy > (bspcoord[BOXBOTTOM]<<FRACBITS));
   boxy -= (vd.viewy >= (bspcoord[BOXTOP]<<FRACBITS));

   boxpos = (boxy << 2) + boxx;
   if(boxpos == 5)
      return true;

   boxpos = (boxy << 2) + boxx;
   if(boxpos == 5)
      return true;

   x1 = bspcoord[checkcoord[boxpos][0]] << FRACBITS;
   y1 = bspcoord[checkcoord[boxpos][1]] << FRACBITS;
   x2 = bspcoord[checkcoord[boxpos][2]] << FRACBITS;
   y2 = bspcoord[checkcoord[boxpos][3]] << FRACBITS;

   // check clip list for an open space
   angle1 = R_PointToAngle2(vd.viewx, vd.viewy, x1, y1);
   angle2 = R_PointToAngle2(vd.viewx, vd.viewy, x2, y2);

   sx1 = R_ClipToViewEdges(angle1, angle2);
   if (sx1 == 0)
      return true;
   if (sx1 == -1)
      return false;
   sx2 = (sx1 & 0xffff) - 1;
   sx1 = sx1 >>    16;

   start = rbsp->solidsegs;
   while(start->last < sx2)
      ++start;

   // does the clippost contain the new span?
   if(sx1 >= start->first && sx2 <= start->last)
      return false;

   return true;
}

static void R_WallEarlyPrep(rbspWork_t *rbsp, viswall_t* segl,
   fixed_t *restrict floorheight, fixed_t *restrict  floornewheight, fixed_t *restrict ceilingnewheight, uint32_t *restrict fofInfo)
{
   seg_t     *seg  = segl->seg;
   line_t    *li   = rbsp->curldef;
   short      offset = seg->sideoffset >> 1;
   side_t    *si   = rbsp->curside;
   sector_t  *front_sector = rbsp->curfsector, *back_sector = rbsp->curbsector;
   fixed_t    f_floorheight, f_ceilingheight;
   fixed_t    b_floorheight, b_ceilingheight;
   int        f_lightlevel, b_lightlevel, lightshift;
   short      f_ceilingpic, b_ceilingpic;
   int        b_texturemid, t_texturemid, m_texturemid;
   short      skyhack;
   short      actionbits;
   int16_t    rowoffset, textureoffset;
   const short liflags = li->flags;
   static sector_t ftempsec;
   static sector_t btempsec;
   segl->fofSector = -1;

   front_sector = R_FakeFlat(front_sector, &ftempsec, false);

   {
      textureoffset = si->textureoffset & 0xfff;
      textureoffset <<= 4; // sign extend
      textureoffset >>= 4; // sign extend
      rowoffset = (si->textureoffset & 0xf000) | ((unsigned)si->rowoffset << 4);
      rowoffset >>= 4; // sign extend

      f_ceilingpic    = front_sector->ceilingpic;
      f_lightlevel    = front_sector->lightlevel;
      f_floorheight   = front_sector->floorheight   - vd.viewz;
      f_ceilingheight = front_sector->ceilingheight - vd.viewz;

      SETLOWER8(segl->floorceilpicnum, flattranslation[front_sector->floorpic]);

      if (f_ceilingpic != 0xff)
      {
          SETUPPER8(segl->floorceilpicnum, flattranslation[f_ceilingpic]);
      }
      else
          SETUPPER8(segl->floorceilpicnum, (uint8_t)-1);

      segl->m_texturenum = -1;

      if (!back_sector)
         back_sector = &emptysector;
      else
         back_sector = R_FakeFlat(back_sector, &btempsec, true);

      b_ceilingpic    = back_sector->ceilingpic;
      b_lightlevel    = back_sector->lightlevel;
      b_floorheight   = back_sector->floorheight   - vd.viewz;
      b_ceilingheight = back_sector->ceilingheight - vd.viewz;

      t_texturemid = b_texturemid = m_texturemid = 0;
      actionbits = 0;

      // deal with sky ceilings (also missing in 3DO)
      if(f_ceilingpic == (uint8_t)-1 && b_ceilingpic == (uint8_t)-1)
         skyhack = true;
      else 
         skyhack = false;

      // add floors and ceilings if the wall needs them
      if(f_floorheight < 0 &&                                // is the camera above the floor?
         (front_sector->floorpic != back_sector->floorpic || // floor texture changes across line?
          f_floorheight   != b_floorheight                || // height changes across line?
          f_lightlevel    != b_lightlevel                 || // light level changes across line?
          b_ceilingheight == b_floorheight))                 // backsector is closed?
      {
         actionbits |= (AC_ADDFLOOR|AC_NEWFLOOR);
      }
      *floorheight = *floornewheight = f_floorheight;
#ifdef FLOOR_OVER_FLOOR_CRAZY
      if (front_sector->fofsec != -1)
      {
         SETLOWER16(*fofInfo, (sectors[front_sector->fofsec].ceilingheight) >> FRACBITS);
         SETUPPER16(*fofInfo, (sectors[front_sector->fofsec].floorheight) >> FRACBITS);
         segl->fofSector = front_sector->fofsec;
      }
#endif

      if(!skyhack                                         && // not a sky hack wall
         (f_ceilingheight > 0 || f_ceilingpic == (uint8_t)-1)      && // ceiling below camera, or sky
         (f_ceilingpic    != b_ceilingpic                 || // ceiling texture changes across line?
          f_ceilingheight != b_ceilingheight              || // height changes across line?              
          f_lightlevel    != b_lightlevel                 || // light level changes across line?
          b_ceilingheight == b_floorheight))                 // backsector is closed?
      {
         if(f_ceilingpic == (uint8_t)-1)
            actionbits |= (AC_ADDSKY|AC_NEWCEILING);
         else
            actionbits |= (AC_ADDCEILING|AC_NEWCEILING);
      }
      segl->ceilingheight = *ceilingnewheight = f_ceilingheight;

      segl->t_topheight = f_ceilingheight; // top of texturemap

      if (!(liflags & ML_TWOSIDED))
      {
         // single-sided line
//         if (si->midtexture > 0)
         {
            SETUPPER8(segl->tb_texturenum, texturetranslation[si->midtexture]);

            // handle unpegging (bottom of texture at bottom, or top of texture at top)
            if(liflags & ML_DONTPEGBOTTOM)
#ifdef WALLDRAW2X
               t_texturemid = f_floorheight + (textures[UPPER8(segl->tb_texturenum)].height << (FRACBITS+1));
#else
               t_texturemid = f_floorheight + (textures[UPPER8(segl->tb_texturenum)].height << FRACBITS);
#endif
            else
               t_texturemid = f_ceilingheight;

            t_texturemid += rowoffset<<FRACBITS;                               // add in sidedef texture offset

#ifdef WALLDRAW2X
            t_texturemid >>= 1;
#endif
            segl->t_bottomheight = f_floorheight; // set bottom height
            actionbits |= (AC_SOLIDSIL|AC_TOPTEXTURE);                   // solid line; draw middle texture only
         }
      }
      else
      {
         // two-sided line
//         if (si->midtexture > 0)
         {
            segl->m_texturenum = texturetranslation[si->midtexture];
            if(liflags & ML_DONTPEGBOTTOM)
            {
               const fixed_t rf_floorheight = rbsp->curfsector->floorheight - vd.viewz;
               const fixed_t rb_floorheight = rbsp->curbsector->floorheight - vd.viewz;
               if(rf_floorheight > rb_floorheight)
                  m_texturemid = rf_floorheight;
               else
                  m_texturemid = rb_floorheight;
#ifdef WALLDRAW2X
               m_texturemid += (textures[segl->m_texturenum].height << (FRACBITS+1));
#else
               m_texturemid += (textures[segl->m_texturenum].height << FRACBITS);
#endif
            }
            else
            {
               const fixed_t rf_ceilingheight = rbsp->curfsector->ceilingheight - vd.viewz;
               const fixed_t rb_ceilingheight = rbsp->curbsector->ceilingheight - vd.viewz;
               if(rb_ceilingheight > rf_ceilingheight)
                  m_texturemid = rf_ceilingheight;
               else
                  m_texturemid = rb_ceilingheight;
            }
            m_texturemid += rowoffset<<FRACBITS; // add in sidedef texture offset
#ifdef WALLDRAW2X
            m_texturemid >>= 1;
#endif
            actionbits |= AC_MIDTEXTURE; // set bottom and top masks
         }

         // is bottom texture visible?
         if(b_floorheight > f_floorheight)
         {
//            if (si->bottomtexture > 0)
            {
               SETLOWER8(segl->tb_texturenum, texturetranslation[si->bottomtexture]);
               if(liflags & ML_DONTPEGBOTTOM)
                  b_texturemid = f_ceilingheight;
               else
                  b_texturemid = b_floorheight;

               b_texturemid += rowoffset<<FRACBITS; // add in sidedef texture offset
#ifdef WALLDRAW2X
               b_texturemid >>= 1;
#endif

               segl->b_topheight = *floornewheight = b_floorheight;
               segl->b_bottomheight = f_floorheight;
               actionbits |= (AC_BOTTOMTEXTURE|AC_NEWFLOOR); // generate bottom wall and floor
            }
         }

         // is top texture visible?
         if(b_ceilingheight < f_ceilingheight && !skyhack)
         {
//            if (si->toptexture > 0)
            {
               SETUPPER8(segl->tb_texturenum, texturetranslation[si->toptexture]);
               if(liflags & ML_DONTPEGTOP)
                  t_texturemid = f_ceilingheight;
               else
#ifdef WALLDRAW2X
                  t_texturemid = b_ceilingheight + (textures[UPPER8(segl->tb_texturenum)].height << (FRACBITS+1));
#else
                  t_texturemid = b_ceilingheight + (textures[UPPER8(segl->tb_texturenum)].height << FRACBITS);
#endif

               t_texturemid += rowoffset<<FRACBITS; // add in sidedef texture offset

#ifdef WALLDRAW2X
               t_texturemid >>= 1;
#endif

               segl->t_bottomheight = *ceilingnewheight = b_ceilingheight;
               actionbits |= (AC_NEWCEILING|AC_TOPTEXTURE); // draw top texture and ceiling
            }
         }

         // check if this wall is solid, for sprite clipping
         if(b_floorheight >= f_ceilingheight || b_ceilingheight <= f_floorheight)
            actionbits |= AC_SOLIDSIL;
         else
         {
            if((b_floorheight >= 0 && b_floorheight > f_floorheight) ||
               (f_floorheight < 0 && f_floorheight > b_floorheight))
            {
               actionbits |= AC_BOTTOMSIL; // set bottom mask
            }

            if(!skyhack)
            {
               if((b_ceilingheight <= 0 && b_ceilingheight < f_ceilingheight) ||
                  (f_ceilingheight >  0 && b_ceilingheight > f_ceilingheight))
               {
                  actionbits |= AC_TOPSIL; // set top mask
               }
            }
         }
      }

      if (vertexes[li->v1].y == vertexes[li->v2].y)
         lightshift = -1;
      else if (vertexes[li->v1].x == vertexes[li->v2].x)
         lightshift = 1;
      else
         lightshift = 0;

      // save local data to the viswall structure
      segl->actionbits    = actionbits;
      segl->t_texturemid  = t_texturemid;
      segl->b_texturemid  = b_texturemid;
      segl->m_texturemid  = m_texturemid;
      segl->seglightlevel = (lightshift << 8) | f_lightlevel;
      segl->offset        = ((fixed_t)textureoffset + offset) << FRACBITS;
   }
}

//
// Store information about the clipped seg range into the viswall array.
//
static void R_StoreWallRange(rbspWork_t *rbsp, int start, int stop)
{
   viswall_t *rw;
   viswallextra_t *rwex;
   int newstop, split;
   int numwalls = vd.lastwallcmd - vd.viswalls;
   const int maxlen = centerX/2;
   // split long segments
   int len = stop - start + 1;

   if (numwalls == MAXWALLCMDS)
      return;
   if (rbsp->splitspans <= 0 || len < maxlen)
   {
      split = len;
      newstop = stop;
   }
   else
   {
      split = 0;
      newstop = start + maxlen - 1;
   }

   rwex = vd.viswallextras + numwalls;
   do {
      rw = vd.lastwallcmd;
      rw->seg = rbsp->curline;
      rw->start = start;
      rw->realstart = start;
      rw->stop = newstop;
      rw->realstop = newstop;
      rw->scalestep = rbsp->lineangle1;
      rw->actionbits = 0;
      ++vd.lastwallcmd;

      R_WallEarlyPrep(rbsp, rw, &rwex->floorheight, &rwex->floornewheight, &rwex->ceilnewheight, &rwex->fofInfo);

#ifdef MARS
      Mars_R_WallNext();
#endif

      rwex++;
      numwalls++;
      if (numwalls == MAXWALLCMDS)
         return;

      start = newstop + 1;
      newstop += maxlen;
      if (newstop > stop)
         newstop = stop;
   } while (start <= stop);

   rbsp->splitspans -= split;
}

//
// Clips the given range of columns, but does not include it in the clip list.
// Does handle windows, e.g., linedefs with upper and lower textures.
//
static void R_ClipWallSegment(rbspWork_t *rbsp, fixed_t first, fixed_t last, boolean solid)
{
    fixed_t      scratch;
    cliprange_t* start, * next;

    // find the first range that touches the range (adjacent pixels are touching)
    scratch = first - 1;
    start = rbsp->solidsegs;
    while (start->last < scratch)
        ++start;

    // add visible pieces and close up holes
    if (first < start->first)
    {
        if (last < start->first - 1)
        {
            // post is entirely visible (above start)
            R_StoreWallRange(rbsp, first, last);

            if (solid)
            {
                next = rbsp->newend;
                ++rbsp->newend;

               if (next != start)
               {
                  int *s = (int *)start;
                  int n = *s++;
                  int cnt = next - start;

                  next = start;
                  do {
                     int nn = *s;
                     *s++ = n, n = nn;
                  } while (--cnt);
               }

                next->first = first;
                next->last = last;
            }

            return;
        }

        // there is a fragment above *start
        R_StoreWallRange(rbsp, first, start->first - 1);

        // now adjust the clip size
        if (solid)
        {
            start->first = first;
        }
    }

    // bottom contained in start?
    if (last <= start->last)
        return;

    next = start;
    while (last >= (next + 1)->first - 1)
    {
        // there is a fragment between two posts
        R_StoreWallRange(rbsp, next->last + 1, (next + 1)->first - 1);
        ++next;

        if (last <= next->last)
        {
            // bottom is contained in next
            last = next->last;
            goto crunch;
        }
    }

    // there is a fragment after *next
    R_StoreWallRange(rbsp, next->last + 1, last);
crunch:
    if (solid)
    {
        // adjust the clip size
        start->last = last;

        // remove start+1 to next from the clip list, because start now covers
        // their area
        if (next == start) // post just extended past the bottom of one post
            return;

        if (next != rbsp->newend)
        {
            int cnt = rbsp->newend - next;
            do
            {
               start++, next++;
               *((int*)start) = *((int*)next);
            } while (--cnt);
        }

        rbsp->newend = start + 1;
    }
}

//
// killough 3/7/98: Hack floor/ceiling heights for deep water etc.
//
// If player's view height is underneath fake floor, lower the
// drawn ceiling to be just under the floor height, and replace
// the drawn floor and ceiling textures, and light level, with
// the control sector's.
//
// Similar for ceiling, only reflected.
//
// killough 4/11/98, 4/13/98: fix bugs, add 'back' parameter
//
sector_t *R_FakeFlat(sector_t *sec, sector_t *tempsec,
                     boolean back)
{
   if (sec->fofsec != -1 && (sec->flags & SF_FOF_SWAPHEIGHTS))
   {
      // Replace sector being drawn, with a copy to be hacked
      *tempsec = *sec;

      const sector_t *fofsec = &sectors[sec->fofsec];
      const fixed_t midpoint = fofsec->floorheight + (fofsec->ceilingheight - fofsec->floorheight)/2;

      if (vd.viewz <= midpoint)
      {
         tempsec->ceilingheight = fofsec->floorheight;
         tempsec->ceilingpic = fofsec->floorpic;      
      }
      else if (vd.viewz > midpoint)
      {
         tempsec->floorheight = fofsec->ceilingheight;
         tempsec->floorpic = fofsec->ceilingpic;
      }

      sec = tempsec;
   }
   else if (sec->heightsec != -1)
   {
      const sector_t *watersec = &sectors[sec->heightsec];
      boolean underwater = vd.viewsubsector->sector->heightsec != -1 && vd.viewz<=sectors[vd.viewsubsector->sector->heightsec].ceilingheight;

      // Replace sector being drawn, with a copy to be hacked
      *tempsec = *sec;

      // Replace floor and ceiling height with other sector's heights.
      tempsec->floorheight = watersec->ceilingheight-1;
      tempsec->floorpic = watersec->ceilingpic;

      if ((underwater && (tempsec->floorheight = sec->floorheight,
                          tempsec->ceilingheight = watersec->ceilingheight-1,
                          !back)) || vd.viewz <= watersec->floorheight)
      { // head-below-floor hack
         tempsec->floorpic    = sec->floorpic;
//       tempsec->floor_xoffs = s->floor_xoffs;
//       tempsec->floor_yoffs = s->floor_yoffs;
         tempsec->ceilingpic = watersec->ceilingpic; 
         tempsec->lightlevel = watersec->lightlevel;
      }

      if (sec->ceilingheight < watersec->ceilingheight)
      {
         tempsec->ceilingpic = sec->ceilingpic;
         tempsec->ceilingheight = sec->ceilingheight;
         tempsec->lightlevel = sec->lightlevel;
      }
      if (sec->floorheight > watersec->ceilingheight)
      {
         tempsec->floorpic = sec->floorpic;
         tempsec->floorheight = sec->floorheight;
         tempsec->lightlevel = sec->lightlevel;
      }
      sec = tempsec;               // Use other sector
   }

   return sec;
}

//
// Clips the given segment and adds any visible pieces to the line list.
//
static void R_AddLine(rbspWork_t *rbsp, seg_t *line)
{
   angle_t angle1, angle2;
   fixed_t x1, x2;
   sector_t *frontsector;
   sector_t *backsector;
   mapvertex_t *v1 = &vertexes[line->v1], *v2 = &vertexes[line->v2];
   int side;
   line_t *ldef;
   side_t *sidedef;
   boolean solid;

   if (line->v1 == rbsp->lastv2)
      angle1 = rbsp->lastangle2;
   else
      angle1 = R_PointToAngle2(vd.viewx, vd.viewy, v1->x << FRACBITS, v1->y << FRACBITS);
   if (line->v2 == rbsp->lastv1)
      angle2 = rbsp->lastangle1;
   else
      angle2 = R_PointToAngle2(vd.viewx, vd.viewy, v2->x << FRACBITS, v2->y << FRACBITS);

   rbsp->lastv1 = line->v1;
   rbsp->lastv2 = line->v2;
   rbsp->lastangle1 = angle1;
   rbsp->lastangle2 = angle2;

   x1 = R_ClipToViewEdges(angle1, angle2);
   if (x1 <= 0)
      return;
   x2 = (x1 & 0xffff) - 1;
   x1 = x1 >>    16;

   // decide which clip routine to use
   side = line->sideoffset & 1;
   ldef = &lines[line->linedef];
   if ((ldef->flags & ML_CULLING) && P_AproxDistance(vd.viewx - (v1->x << FRACBITS), vd.viewy - (v1->y << FRACBITS)) > 2048*FRACUNIT)
      return;

   frontsector = rbsp->curfsector;//R_FakeFlat(rbsp->curfsector, &ftempsec, false);
   backsector = (ldef->flags & ML_TWOSIDED) ? &sectors[sides[ldef->sidenum[side^1]].sector] : 0;
   sidedef = &sides[ldef->sidenum[side]];

   solid = false;
   sector_t *oldbsector = backsector;

   if (!backsector)
      solid = true;
   else if (backsector->ceilingpic == (uint8_t)-1 && frontsector->ceilingpic == (uint8_t)-1)
   {
      // When both ceilings are skies, consider them always "open" to prevent HOM
      solid = false;
   }
   else if (backsector->ceilingheight <= frontsector->floorheight ||
       backsector->floorheight >= frontsector->ceilingheight)
       solid = true;
   else
   {
      if (backsector->ceilingheight <= frontsector->floorheight ||
         backsector->floorheight >= frontsector->ceilingheight)
      {
         solid = true;
      }
      else if (backsector->ceilingheight == frontsector->ceilingheight &&
         backsector->floorheight == frontsector->floorheight)
      {
         // reject empty lines used for triggers and special events
         if (sidedef->midtexture == 0 &&
            backsector->ceilingpic == frontsector->ceilingpic &&
            backsector->floorpic == frontsector->floorpic &&
            *(int8_t *)&backsector->lightlevel == *(int8_t *)&frontsector->lightlevel) // hack to get rid of the extu.w on SH-2
            return;
      }
   }

   rbsp->curline = line;
   rbsp->curside = sidedef;
   rbsp->curldef = ldef;
   rbsp->curbsector = oldbsector;
   rbsp->lineangle1 = angle1;
   R_ClipWallSegment(rbsp, x1, x2, solid);
}

visplane_t *floorplane, *ceilingplane;
//
// Determine floor/ceiling planes, add sprites of things in sector,
// draw one or more segments.
//
static void R_Subsector(rbspWork_t *rbsp, int num)
{
   subsector_t *sub = &subsectors[num];
   seg_t       *line, *stopline;
   int          count;
   sector_t    *frontsector = sub->sector;
      
   if (frontsector->thinglist)
   {
      if(frontsector->validcount != validcount[0]) // not already processed?
      {
         frontsector->validcount = validcount[0];  // mark it as processed
         if (vd.lastvissector < vd.vissectors + MAXVISSSEC)
         {
           *vd.lastvissector++ = frontsector;
         }
      }
   }

   line     = &segs[sub->firstline];
   count    = sub->numlines;
   stopline = line + count;

   rbsp->curfsector = frontsector;
   while(line != stopline)
      R_AddLine(rbsp, line++);
}

//
// Recursively descend through the BSP, classifying nodes according to the
// player's point of view, and render subsectors in view.
//
static void R_DecodeBBox(int16_t *bbox, const int16_t *outerbbox, uint16_t encbbox)
{
   uint16_t l;

   l = outerbbox[BOXTOP] - outerbbox[BOXBOTTOM];
   bbox[BOXBOTTOM] = outerbbox[BOXBOTTOM] + ((l * (encbbox & 0x0f)) >> 4);
   bbox[BOXTOP] = outerbbox[BOXTOP] - ((l * (encbbox & 0xf0)) >> 8);

   encbbox >>= 8;
   l = outerbbox[BOXRIGHT] - outerbbox[BOXLEFT];
   bbox[BOXLEFT] = outerbbox[BOXLEFT] + ((l * (encbbox & 0x0f)) >> 4);
   bbox[BOXRIGHT] = outerbbox[BOXRIGHT] - ((l * (encbbox & 0xf0)) >> 8);
}

static void R_RenderBSPNode(rbspWork_t *rbsp, int bspnum, int16_t *outerbbox)
{
   node_t *bsp;
   int     i, side;
   int16_t bbox[2][4];

#ifdef MARS
   if((int16_t)bspnum < 0) // reached a subsector leaf?
#else
   if(bspnum & NF_SUBSECTOR) // reached a subsector leaf?
#endif
   {
      R_Subsector(rbsp, bspnum == -1 ? 0 : bspnum & ~NF_SUBSECTOR);
      return;
   }

   bsp = &nodes[bspnum];

   // decide which side the view point is on
   side = R_PointOnSide(vd.viewx, vd.viewy, bsp);
   for (i = 0; i < 2; i++)
      R_DecodeBBox(bbox[i], outerbbox, bsp->encbbox[i]);

   // recursively render front space
   R_RenderBSPNode(rbsp, bsp->children[side], bbox[side]);

   // possibly divide back space
   side ^= 1;
   if(R_CheckBBox(rbsp, bbox[side])) {
      R_RenderBSPNode(rbsp, bsp->children[side], bbox[side]);
   }
}

//
// Kick off the rendering process by initializing the solidsegs array and then
// starting the BSP traversal.
//
void R_BSP(void)
{
   rbspWork_t rbsp;
   cliprange_t solidsegs[MAXSEGS];

#ifdef MARS
   W_POINTLUMPNUM(gamemaplump);
#endif

   solidsegs[0].first = -2;
   solidsegs[0].last  = -1;
   solidsegs[1].first = viewportWidth;
   solidsegs[1].last  = viewportWidth+1;
   rbsp.solidsegs = solidsegs;
   rbsp.newend = &solidsegs[2];
   rbsp.splitspans = viewportWidth + viewportWidth/2;
   rbsp.lastv1 = -1;
   rbsp.lastv2 = -1;

   R_RenderBSPNode(&rbsp, numnodes - 1, worldbbox);
}

// EOF

