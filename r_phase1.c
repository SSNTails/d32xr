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

// may_alias: entries are moved around as ints
typedef struct __attribute__((aligned(4), may_alias))
{
   VINT first;
   VINT last;
} cliprange_t;

#define MAXSEGS 32

typedef struct
{
   cliprange_t *newend;
   cliprange_t *solidsegs;
   seg_t       *curline;
   const subsector_t *frontsubsec;
   sector_t *curfsector;
   const sector_t *curbsector;
   const side_t      *curside;
   const line_t      *curldef;
   const sidetex_t   *sidetex;
   int        splitspans; /* generate wall splits until this reaches 0 */
   // view space transform: tz = x*c + y*s - tz0, tx = y*c - x*s - tx0
   // for map unit x and y, in Q13 map units (tz forward, tx to the left)
   int16_t    viewcs[4]; /* cos, sin, -sin, cos (Q13) */
   fixed_t    viewntz0, viewntx0; /* -tz0, -tx0 */
#ifdef MARS
   uint32_t   colmul; /* centerX */
#endif
} rbspWork_t;

static void R_AddLine(rbspWork_t *rbsp, seg_t* line) ATTR_DATA_CACHE_ALIGN;
static void R_ClipWallSegment(rbspWork_t *rbsp, fixed_t first, fixed_t last, boolean solid) ATTR_DATA_CACHE_ALIGN;
static boolean R_CheckBBox(rbspWork_t *rbsp, int16_t bspcoord_[4]) ATTR_DATA_CACHE_ALIGN;
static void R_Subsector(rbspWork_t *rbsp, int num) ATTR_DATA_CACHE_ALIGN;
static void R_StoreWallRange(rbspWork_t *rbsp, int start, int stop) ATTR_DATA_CACHE_ALIGN;
static void R_RenderBSPNode(rbspWork_t *rbsp, int bspnum, int16_t *outerbbox) ATTR_DATA_CACHE_ALIGN;
static void R_WallEarlyPrep(rbspWork_t *rbsp, viswall_t* segl,
   fixed_t *restrict floorheight, fixed_t *restrict floornewheight, 
   fixed_t *restrict ceilingnewheight, fixed_t *restrict fofInfo) ATTR_DATA_CACHE_ALIGN;

#ifdef MARS
__attribute__((aligned(4)))
#endif
static int8_t checkcoord[12][4] =
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

static sector_t emptysector = { .floorheight = 0, .ceilingheight = 0, .floorpic = -2, .ceilingpic = -2, .lightlevel = -2, .special = 0, .tag = 0, .flags = 0, .heightsec = -1, .fofsec = -1, .specialdata = (SPTR)0, .specline = -1 };

//
// R_ProjectVertex projects a map vertex into view space.
//
// R_PointToColumnStart / R_PointToColumn give the screen column of a
// point inside the view frustum, counted from the right edge:
// floor(centerX * b / tz) with b = tz + tx, 0 <= b <= 2*tz, tz > 0.
// viewportWidth minus this is ceil(centerX - centerX * tx / tz), the first
// column at or right of the point (like Doom's original viewangletox),
// in 0..viewportWidth.

#ifdef MARS

static ATTR_FORCE_INLINE
void R_ProjectVertex(const rbspWork_t *rbsp, const mapvertex_t *v, int *ptz, int *ptx)
{
   int tz, tx;
   // vertex (x, y) and viewcs are int16 pairs in memory: two mac.w each,
   // accumulating on top of -tz0 / -tx0
   const void *pv = v, *pk = rbsp->viewcs;
      __asm (
         "lds %5,macl\n\t"
         "mac.w @%3+,@%2+\n\t"
         "mac.w @%3+,@%2+\n\t"
         "sts macl,%0\n\t"
         "lds %6,macl\n\t"
         "add #-4,%2\n\t"
         "mac.w @%3+,@%2+\n\t"
         "mac.w @%3+,@%2+\n\t"
         "sts macl,%1"
         : "=&r"(tz), "=&r"(tx), "+r"(pv), "+r"(pk)
         : "m"(*v), "r"(rbsp->viewntz0), "r"(rbsp->viewntx0), "m"(rbsp->viewcs[0]), "m"(rbsp->viewcs[1]), "m"(rbsp->viewcs[2]), "m"(rbsp->viewcs[3])
         : "mach", "macl");
      *ptz = tz;
      *ptx = tx;
   }

   // R_PointToColumnStart starts the 64/32 hardware divide of centerX * b by
   // tz, R_PointToColumn reads the quotient.
   static ATTR_FORCE_INLINE
   void R_PointToColumnStart(const rbspWork_t *rbsp, uint32_t b, uint32_t tz)
   {
      uint64_t n = (uint64_t)rbsp->colmul * b;
      SH2_DIVU_DVSR = tz;
      SH2_DIVU_DVDNTH = (uint32_t)(n >> 32);
      SH2_DIVU_DVDNTL = (uint32_t)n;
   }

   static ATTR_FORCE_INLINE
   int R_PointToColumn(const rbspWork_t *rbsp, uint32_t b, uint32_t tz)
   {
      return SH2_DIVU_DVDNTL;
   }

#else

   static ATTR_FORCE_INLINE
   void R_ProjectVertex(const rbspWork_t *rbsp, const mapvertex_t *v, int *ptz, int *ptx)
   {
      *ptz = v->x * rbsp->viewcs[0] + v->y * rbsp->viewcs[1] + rbsp->viewntz0;
      *ptx = v->x * rbsp->viewcs[2] + v->y * rbsp->viewcs[3] + rbsp->viewntx0;
   }

   static ATTR_FORCE_INLINE
   void R_PointToColumnStart(const rbspWork_t *rbsp, uint32_t b, uint32_t tz)
   {
   }

   static ATTR_FORCE_INLINE
   int R_PointToColumn(const rbspWork_t *rbsp, uint32_t b, uint32_t tz)
   {
      return FixedMul(FixedDiv(b, tz), centerX);
   }

#endif

//
// Projects the map point (x, y) into view space.
//
static ATTR_FORCE_INLINE
void R_ProjectPoint(const rbspWork_t *rbsp, int x, int y, int *ptz, int *ptx)
{
   *ptz = R_MulsW(x, rbsp->viewcs[0]) + R_MulsW(y, rbsp->viewcs[1]) + rbsp->viewntz0;
   *ptx = R_MulsW(x, rbsp->viewcs[2]) + R_MulsW(y, rbsp->viewcs[3]) + rbsp->viewntx0;
}

#define R_InsideView(a, b, tz) (((a) | (b) | ((tz) - 1)) >= 0)

//
// Checks BSP node/subtree bounding box. Returns true if some part of the bbox
// might be visible.
//
static boolean R_CheckBBox(rbspWork_t *rbsp, int16_t bspcoord_[4])
{
   int boxx;
   int boxy;
   int boxpos;

   int tz1, tx1, tz2, tx2, a1, b1, a2, b2;
   const int8_t *cc;

   cliprange_t *start;

   int sx1, sx2;

   fixed_t bspcoord[4];

   bspcoord[0] = bspcoord_[0] << FRACBITS;
   bspcoord[1] = bspcoord_[1] << FRACBITS;
   bspcoord[2] = bspcoord_[2] << FRACBITS;
   bspcoord[3] = bspcoord_[3] << FRACBITS;

   // find the corners of the box that define the edges from current viewpoint;
   // the box edges are inclusive, so the viewer is never on the line through
   // the two corners and the span between them is front-facing
   boxx = 2;
   boxx -= (vd.viewx <= bspcoord[BOXRIGHT]);
   boxx -= (vd.viewx < bspcoord[BOXLEFT]);

   boxy = 2;
   boxy -= (vd.viewy >= bspcoord[BOXBOTTOM]);
   boxy -= (vd.viewy > bspcoord[BOXTOP]);

   boxpos = (boxy << 2) + boxx;
   if(boxpos == 5)
      return true;

   boxpos = (boxy << 2) + boxx;
   if(boxpos == 5)
      return true;

   cc = checkcoord[boxpos];

   // the right corner's divide runs while the left corner is projected
   R_ProjectPoint(rbsp, bspcoord_[cc[2]], bspcoord_[cc[3]], &tz2, &tx2);
   a2 = tz2 - tx2, b2 = tz2 + tx2;

   R_PointToColumnStart(rbsp, b2, tz2);

   R_ProjectPoint(rbsp, bspcoord_[cc[0]], bspcoord_[cc[1]], &tz1, &tx1);
   a1 = tz1 - tx1, b1 = tz1 + tx1;

   // clip the front-facing span between the corners to the 90 degree view
   // frustum |tx| <= tz: with a = tz - tx and b = tz + tx, a corner is inside
   // if a >= 0 && b >= 0 (and tz > 0); the span can only enter through the
   // left edge (a1 <= 0 <= a2) and leave through the right one (b2 <= 0 <= b1)
   if (R_InsideView(a2, b2, tz2))
      sx2 = viewportWidth - R_PointToColumn(rbsp, b2, tz2);
   else if (b1 < 0 || b2 > 0)
      return false;
   else
      sx2 = viewportWidth;
   sx2 = sx2 - 1;

   // the left corner's divide runs during the clip list scan
   R_PointToColumnStart(rbsp, b1, tz1);
   if (!R_InsideView(a1, b1, tz1) && (a1 > 0 || a2 < 0))
      return false;
   
   // check clip list for an open space
   start = rbsp->solidsegs - 1;
   do {
      ++start;
   } while (start->last < sx2);

   sx1 = R_InsideView(a1, b1, tz1) ? viewportWidth - R_PointToColumn(rbsp, b1, tz1) : 0;
   if (sx1 > sx2)
      return false;

   // does the clippost contain the new span?
   if(sx1 >= start->first && sx2 <= start->last)
      return false;

   return true;
}

static void R_WallEarlyPrep(rbspWork_t *rbsp, viswall_t* segl,
   fixed_t *restrict floorheight, fixed_t *restrict  floornewheight, fixed_t *restrict ceilingnewheight, fixed_t *restrict fofInfo)
{
//   const seg_t     *seg  = segl->seg;
   const line_t    *li   = rbsp->curldef;
   const side_t    *si   = rbsp->curside;
   fixed_t    f_floorheight, f_ceilingheight;
   fixed_t    b_floorheight, b_ceilingheight;
   int        f_lightlevel, b_lightlevel, lightshift;
   short      f_floorpic, f_ceilingpic;
   short      b_floorpic, b_ceilingpic;
   int        b_texturemid, t_texturemid, m_texturemid, fof_texturemid;
   boolean    floorskyhack;
   boolean    skyhack;
   int        actionbits;
   int16_t    rowoffset, textureoffset;
   const short liflags = li->flags;
   static sector_t ftempsec;
   static sector_t btempsec;
   #ifdef FLOOR_OVER_FLOOR
   segl->fofSector = *fofInfo = -1; // fofSector is back sector FOF, fofInfo is front sector FOF
   #endif

   const sector_t *front_sector = R_FakeFlat(rbsp->curfsector, &ftempsec, vd.viewsubsector == rbsp->frontsubsec);

   textureoffset = si->textureoffset & 0xfff;
   textureoffset <<= 4; // sign extend
   textureoffset >>= 4; // sign extend
   rowoffset = (si->textureoffset & 0xf000) | ((unsigned)si->rowoffset << 4);
   rowoffset >>= 4; // sign extend

   f_floorpic      = front_sector->floorpic;
   f_ceilingpic    = front_sector->ceilingpic;
   f_lightlevel    = front_sector->lightlevel;
   f_floorheight   = front_sector->floorheight   - vd.viewz;
   f_ceilingheight = front_sector->ceilingheight - vd.viewz;

   segl->m_texturenum = (uint8_t)-1;
   segl->fof_texturenum = (uint8_t)-1;

   const sector_t *back_sector;
   if (!rbsp->curbsector)
      back_sector = &emptysector;
   else
      back_sector = R_FakeFlat(rbsp->curbsector, &btempsec, false);

   b_floorpic      = back_sector->floorpic;
   b_ceilingpic    = back_sector->ceilingpic;
   b_lightlevel    = back_sector->lightlevel;
   b_floorheight   = back_sector->floorheight   - vd.viewz;
   b_ceilingheight = back_sector->ceilingheight - vd.viewz;

   t_texturemid = b_texturemid = m_texturemid = fof_texturemid = 0;
   actionbits = 0;

   floorskyhack = f_floorpic == 0xff && b_floorpic == 0xff;

   // deal with sky ceilings (also missing in 3DO)
   skyhack = f_ceilingpic == 0xff && b_ceilingpic == 0xff;

   // add floors and ceilings if the wall needs them
   if(!floorskyhack                                         && // not a sky hack wall
      (f_floorheight < 0 || f_floorpic == (uint8_t)-1)        && // is the camera above the floor?
      (f_floorpic      != b_floorpic                   || // floor texture changes across line?
         f_floorheight   != b_floorheight                || // height changes across line?
         f_lightlevel    != b_lightlevel                 || // light level changes across line?
         b_ceilingheight == b_floorheight))                 // backsector is closed?
   {
      if(f_floorpic == (uint8_t)-1)
         actionbits |= (AC_ADDFLOORSKY|AC_NEWFLOOR);
      else
         actionbits |= (AC_ADDFLOOR|AC_NEWFLOOR);
   }
   *floorheight = *floornewheight = f_floorheight;
   segl->floorheight_t = f_floorheight >> FRACBITS;

   segl->t_bottomheight = f_floorheight; // bottom of texturemap

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

   const sidetex_t *st = SIDETEX(si);

   if (li->sidenum[1] < 0)
   {
      // single-sided line
//         if (si->midtexture > 0)
      {
         segl->t_texturenum = texturetranslation[st->midtexture];

         // handle unpegging (bottom of texture at bottom, or top of texture at top)
         if(liflags & ML_DONTPEGBOTTOM)
            t_texturemid = f_floorheight + (((textures[segl->t_texturenum].dblHeight) + rowoffset) << FRACBITS);
         else
            t_texturemid = f_ceilingheight + (rowoffset<<FRACBITS);

#ifdef WALLDRAW2X
         t_texturemid >>= 1;
#endif
         segl->t_bottomheight = f_floorheight; // set bottom height
         actionbits |= (AC_SOLIDSIL|AC_TOPTEXTURE);                   // solid line; draw middle texture only
      }

#ifdef FLOOR_OVER_FLOOR
      if (front_sector->fofsec >= 0 && !(front_sector->flags & SF_FOF_SWAPHEIGHTS))
      {
         const sector_t *frontFOF = I_TO_SEC(front_sector->fofsec);
         *fofInfo = front_sector->fofsec;

         if (frontFOF->ceilingheight < vd.viewz)
         {
            // Top of FOF is visible
            actionbits |= AC_FOFTOP;
         }
         else if (frontFOF->floorheight > vd.viewz)
         {
            // Bottom of FOF is visible
            actionbits |= AC_FOFBOTTOM;
         }
      }
#endif
   }
   else
   {
      // two-sided line
//         if (st->midtexture > 0)
      if (st->midtexture > 0 && D_abs(vd.viewx_t - ((vertexes[li->v1].x+vertexes[li->v2].x)>>1)) < ((liflags & ML_CULL_MIDTEXTURE) ? 768 : 1536) && D_abs(vd.viewy_t - ((vertexes[li->v1].y+vertexes[li->v2].y)>>1)) < ((liflags & ML_CULL_MIDTEXTURE) ? 768 : 1536)) // Don't draw midtextures when too far away to really matter
      {
         segl->m_texturenum = texturetranslation[st->midtexture];
         if(liflags & ML_DONTPEGBOTTOM)
         {
            const fixed_t rf_floorheight = rbsp->curfsector->floorheight - vd.viewz;
            const fixed_t rb_floorheight = rbsp->curbsector->floorheight - vd.viewz;
            if(rf_floorheight > rb_floorheight)
               m_texturemid = rf_floorheight + (((textures[segl->m_texturenum].dblHeight) + rowoffset) << FRACBITS);
            else
               m_texturemid = rb_floorheight + (((textures[segl->m_texturenum].dblHeight) + rowoffset) << FRACBITS);
         }
         else
         {
            const fixed_t rf_ceilingheight = rbsp->curfsector->ceilingheight - vd.viewz;
            const fixed_t rb_ceilingheight = rbsp->curbsector->ceilingheight - vd.viewz;
            if(rb_ceilingheight > rf_ceilingheight)
               m_texturemid = rf_ceilingheight;
            else
               m_texturemid = rb_ceilingheight;

            m_texturemid += rowoffset<<FRACBITS; // add in sidedef texture offset
         }
#ifdef WALLDRAW2X
         m_texturemid >>= 1;
#endif
         actionbits |= AC_MIDTEXTURE; // set bottom and top masks
      }

#ifdef FLOOR_OVER_FLOOR
      if (back_sector->fofsec >= 0 && !(back_sector->flags & SF_FOF_SWAPHEIGHTS))
      {
         const sector_t *backFOF = I_TO_SEC(back_sector->fofsec);
         segl->fofSector = back_sector->fofsec;
         *fofInfo = front_sector->fofsec;
         actionbits |= AC_FOFSIDE; // Means the backsector has a FOF

         if (backFOF->ceilingheight < vd.viewz)
            segl->fof_picnum = backFOF->ceilingpic;
         else if (backFOF->floorheight > vd.viewz)
            segl->fof_picnum = backFOF->floorpic;

         if (front_sector->fofsec != back_sector->fofsec)
         {
            const line_t *fofline = &lines[backFOF->specline];
            const side_t *fofside = &sides[fofline->sidenum[0]];
            fof_texturemid = backFOF->ceilingheight - vd.viewz;
#ifdef WALLDRAW2X
            fof_texturemid >>= 1;
#endif
            segl->fof_texturenum = texturetranslation[SIDETEX(fofside)->midtexture];
            segl->fof_sideThickness = (backFOF->ceilingheight - backFOF->floorheight) >> FRACBITS;
//               fof_texturemid += rowoffset<<FRACBITS; // add in sidedef texture offset

            if (front_sector->fofsec >= 0)
            {
               const sector_t *frontFOF = I_TO_SEC(front_sector->fofsec);

               if (frontFOF->ceilingheight >= backFOF->ceilingheight
                  && frontFOF->floorheight <= backFOF->floorheight)
                  segl->fof_texturenum = (uint8_t)-1;
               else if (backFOF->ceilingheight > frontFOF->ceilingheight && backFOF->floorheight < frontFOF->ceilingheight)
                  segl->fof_sideThickness -= (frontFOF->ceilingheight - backFOF->floorheight) >> FRACBITS;
               else if (backFOF->floorheight < frontFOF->floorheight && backFOF->ceilingheight > frontFOF->floorheight)
               {
                  // NOTE: This may need the >>= 1 on 'amount'
                  VINT amount = (backFOF->ceilingheight - frontFOF->floorheight) >> FRACBITS;
                  segl->fof_sideThickness -= amount;
                  segl->fof_texturemid -= amount;
               }
               
               *fofInfo = front_sector->fofsec;
               if (frontFOF->ceilingheight < vd.viewz)
               {
                  // Rendering the ceiling
                  actionbits |= AC_FOFTOP;
               }
               else if (frontFOF->floorheight > vd.viewz)
               {
                  actionbits |= AC_FOFBOTTOM;
               }
            }
            else // Regular sector in front
            {
               if ((f_floorheight < 0 && backFOF->ceilingheight <= front_sector->floorheight)
                  || (f_floorheight > 0 && backFOF->floorheight >= front_sector->ceilingheight))
                  segl->fof_texturenum = (uint8_t)-1;
               else if (backFOF->floorheight < front_sector->floorheight)
                  segl->fof_sideThickness -= (front_sector->floorheight - backFOF->floorheight) >> FRACBITS;
            }
         }
      }
      else if (front_sector->fofsec >= 0 && !(front_sector->flags & SF_FOF_SWAPHEIGHTS))
      {
         const sector_t *frontFOF = I_TO_SEC(front_sector->fofsec);
         *fofInfo = front_sector->fofsec;
         if (frontFOF->ceilingheight < vd.viewz)
         {
            // Rendering the ceiling
            actionbits |= AC_FOFTOP;
         }
         else if (frontFOF->floorheight > vd.viewz)
         {
            actionbits |= AC_FOFBOTTOM;
         }
      }
#endif

      // is bottom texture visible?
      if(b_floorheight > f_floorheight && !floorskyhack)
      {
//            if (si->bottomtexture > 0)
         {
            segl->b_texturenum = texturetranslation[st->bottomtexture];
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
            segl->t_texturenum = texturetranslation[st->toptexture];
            if(liflags & ML_DONTPEGTOP)
               t_texturemid = f_ceilingheight + (rowoffset<<FRACBITS);
            else
               t_texturemid = b_ceilingheight + (((textures[segl->t_texturenum].dblHeight) + rowoffset) << FRACBITS);

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
         if(!floorskyhack)
         {
            if((b_floorheight >= 0 && b_floorheight > f_floorheight) ||
               (f_floorheight < 0 && f_floorheight > b_floorheight))
            {
               actionbits |= AC_BOTTOMSIL; // set bottom mask
            }
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

      // Special case for closed sectors acting as sky windows
      if (b_floorheight == b_ceilingheight && b_ceilingpic == (uint8_t)-1 && b_floorpic == (uint8_t)-1)
      {
         actionbits &= ~(AC_TOPSIL | AC_BOTTOMSIL | AC_SOLIDSIL | AC_TOPTEXTURE | AC_BOTTOMTEXTURE | AC_MIDTEXTURE | AC_NEWCEILING | AC_NEWFLOOR);
         actionbits |= AC_ADDSKY;
      }
   }

   if (vertexes[li->v1].y == vertexes[li->v2].y)
      lightshift = -1;
   else if (vertexes[li->v1].x == vertexes[li->v2].x)
      lightshift = 1;
   else
      lightshift = 0;

   // save local data to the viswall structure
   segl->actionbits    = (uint16_t)actionbits;
   segl->t_texturemid  = t_texturemid;
   segl->b_texturemid  = b_texturemid;
   segl->m_texturemid  = m_texturemid;
   segl->fof_texturemid = fof_texturemid;
   segl->seglightlevel = (lightshift << 8) | f_lightlevel;
   segl->uoffset       = textureoffset;
   if (f_floorpic != 0xff)
   {
         segl->floorpicnum = flattranslation[f_floorpic];
         segl->floor_offs = front_sector->floor_xoffs;
   }
   else
   {
         segl->floorpicnum = -1;
         segl->floor_offs = 0;
   }

   if (f_ceilingpic != 0xff)
   {
      segl->ceilpicnum = flattranslation[f_ceilingpic];
   }
   else
      segl->ceilpicnum = -1;
}

//
// Store information about the clipped seg range into the viswall array.
//
static void R_StoreWallRange(rbspWork_t *rbsp, int start, int stop)
{
   viswall_t *rw;
   viswallextra_t *rwex;
   int newstop, split;
   viswall_t *lastwall = vd.viswalls + MAXWALLCMDS;
   const int maxlen = (unsigned)centerX >> 1;
   // split long segments
   int len = stop - start + 1;

   if (start > stop)
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

   const int numwalls = vd.lastwallcmd - vd.viswalls;
   rwex = vd.viswallextras + numwalls;
   do {
      rw = vd.lastwallcmd;

      if (rw == lastwall)
         return;

      rw->seg = rbsp->curline;
      rw->start = start;
      rw->stop = newstop;
      rw->actionbits = 0;
      vd.lastwallcmd = rw + 1;

      R_WallEarlyPrep(rbsp, rw, &rwex->floorheight, &rwex->floornewheight, &rwex->ceilnewheight, &rwex->fofInfo);

#ifdef MARS
      Mars_R_WallNext();
#endif

      rwex++;

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
    start = rbsp->solidsegs - 1;
    do {
        ++start;
    } while (start->last < scratch);

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

               // start is never past the right sentinel, so next != start
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

        // move next+1..newend-1 down to start+1
        if (++next != rbsp->newend)
        {
            int cnt = rbsp->newend - next;
            next--;
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
const sector_t *R_FakeFlat(const sector_t *sec, sector_t *tempsec,
                     boolean isViewSubsector)
{
   if (sec->fofsec >= 0 && ((sec->flags & SF_FOF_SWAPHEIGHTS) || isViewSubsector))
   {
      // Replace sector being drawn, with a copy to be hacked
      *tempsec = *sec;

      const sector_t *fofsec = I_TO_SEC(sec->fofsec);

      if (sec->heightsec < 0) // Standard midpoint version
      {
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
      }
      else // This one flips depending on water status
      {
         const sector_t *watersec = I_TO_SEC(sec->heightsec);

         if (vd.underwater)
         {
            if (fofsec->floorheight <= watersec->ceilingheight)
            {
               tempsec->ceilingheight = fofsec->floorheight;
               tempsec->ceilingpic = fofsec->floorpic;
            }
            else
            {
               tempsec->ceilingheight = watersec->ceilingheight;
               tempsec->ceilingpic = watersec->ceilingpic;
            }
         }
         else if (fofsec->ceilingheight >= watersec->ceilingheight)
         {
            tempsec->floorheight = fofsec->ceilingheight;
            tempsec->floorpic = fofsec->ceilingpic;
         }
         else
         {
            tempsec->floorheight = watersec->ceilingheight;
            tempsec->floorpic = watersec->ceilingpic;
         }
      }

      return tempsec;
   }
   else if (sec->heightsec >= 0)
   {
      const sector_t *watersec = I_TO_SEC(sec->heightsec);

      // Replace sector being drawn, with a copy to be hacked
      *tempsec = *sec;

      // Replace floor and ceiling height with other sector's heights.


      // I don't think we need to be Boom-accurate here.
//      if ((underwater && (tempsec->floorheight = sec->floorheight,
//                          tempsec->ceilingheight = watersec->ceilingheight-1,
//                          !back)) || vd.viewz <= watersec->floorheight)
      if (vd.underwater)
      { // head-below-floor hack
         tempsec->floorheight = sec->floorheight;
         tempsec->ceilingheight = watersec->ceilingheight;
         tempsec->floorpic    = sec->floorpic;
//       tempsec->floor_xoffs = s->floor_xoffs;
//       tempsec->floor_yoffs = s->floor_yoffs;
         tempsec->ceilingpic = watersec->ceilingpic; 
         tempsec->lightlevel = watersec->lightlevel;
      }
      else
      {
         tempsec->floorheight = watersec->ceilingheight;
         tempsec->floorpic = watersec->ceilingpic;
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

      return tempsec;               // Use other sector
   }

   return sec;
}

//
// Clips the given segment and adds any visible pieces to the line list.
//
static void R_AddLine(rbspWork_t *rbsp, seg_t *line)
{
   int tz1, tx1, tz2, tx2, a1, b1, a2, b2;
   fixed_t x1, x2;
   sector_t *frontsector;
   sector_t *backsector;
   const mapvertex_t *v1 = &vertexes[line->v1], *v2 = &vertexes[line->v2];
   int side;
   const line_t *ldef;
   const side_t *sidedef;
   boolean solid;

   if (!vd.underwater && (lines[line->linedef].flags & ML_UNDERWATERONLY))
      return;

   if ((lines[line->linedef].flags & ML_CULLING) && (D_abs(vd.viewx_t - v1->x) > 2048 || D_abs(vd.viewy_t - v1->y) > 2048))
      return;

   R_ProjectVertex(rbsp, v1, &tz1, &tx1);
   a1 = tz1 - tx1, b1 = tz1 + tx1;

   // v1's divide runs during the tests below
   R_PointToColumnStart(rbsp, b1, tz1);

   R_ProjectVertex(rbsp, v2, &tz2, &tx2);
   a2 = tz2 - tx2, b2 = tz2 + tx2;

   // both vertices beyond the same frustum edge?
   if ((a1 & a2) < 0 || (b1 & b2) < 0)
      return;
   
   // facing away (v1 not left of v2 as seen from the viewer)?
   if ((int64_t)tx1 * tz2 <= (int64_t)tz1 * tx2)
      return;

   // clip the front-facing span to the 90 degree view frustum |tx| <= tz:
   // with a = tz - tx and b = tz + tx, a vertex is inside if a >= 0 && b >= 0
   // (and tz > 0); the span can only enter through the left edge
   // (a1 <= 0 <= a2) and leave through the right one (b2 <= 0 <= b1)
   if (R_InsideView(a1, b1, tz1))
      x1 = viewportWidth - R_PointToColumn(rbsp, b1, tz1);
   else if (a1 > 0 || a2 < 0)
      return;
   else
      x1 = 0;

   R_PointToColumnStart(rbsp, b2, tz2); // runs during the backsector checks
   if (!R_InsideView(a2, b2, tz2) && (b1 < 0 || b2 > 0))
      return;

   // decide which clip routine to use
   side = line->sideoffset & 1;
   ldef = &lines[line->linedef];
   frontsector = rbsp->curfsector;
   backsector = ldef->sidenum[1] >= 0 ? LD_SIDESECTOR(ldef, side^1) : NULL;
   sidedef = &sides[ldef->sidenum[side]];
   solid = false;

   if (!backsector)
   {
       solid = true;
   }
   else
   {
      if (*(int8_t *)&backsector->ceilingpic == -1 || *(int8_t *)&frontsector->ceilingpic == -1)
      {
         // When both ceilings are skies, consider them always "open" to prevent HOM
//         solid = false;
      }
      else if (backsector->ceilingheight <= frontsector->floorheight ||
         backsector->floorheight >= frontsector->ceilingheight)
      {
         solid = true;
      }
   /*   else if (backsector->ceilingheight == frontsector->ceilingheight &&
         backsector->floorheight == frontsector->floorheight)
      {
         // reject empty lines used for triggers and special events
         // ...except SRB2 doesn't really use them! Is it worth tossing?
         if (!(backsector->fofsec || frontsector->fofsec) &&
            *(int32_t *)&backsector->floorpic == *(int32_t *)&frontsector->floorpic && // compares both floorpic, ceilingpic, lightlevel, and special
            // hack to get rid of the extu.w on SH-2
            SIDETEX(sidedef)->midtexture == 0)
            return;
      }*/
   }

   rbsp->curline    = line;
   rbsp->curside    = sidedef;
   rbsp->curldef    = ldef;
   rbsp->curbsector = backsector;

   x2 = R_InsideView(a2, b2, tz2) ? viewportWidth - R_PointToColumn(rbsp, b2, tz2) : viewportWidth;
   if (x1 >= x2)
      return;
   x2 = x2 - 1;

   R_ClipWallSegment(rbsp, x1, x2, solid);
}

   /*

   if (ldef->sidenum[1] < 0)
   {
      solid = true;
      rbsp->curbsector = NULL;
   }
   else
   {
      const sector_t *backsector = I_TO_SEC(sides[ldef->sidenum[side^1]].sector);
      rbsp->curbsector = backsector;

      if (*(int8_t *)&backsector->ceilingpic == -1 || *(int8_t *)&frontsector->ceilingpic == -1)
      {
         // When both ceilings are skies, consider them always "open" to prevent HOM
//         solid = false;
      }
      else if (backsector->ceilingheight <= frontsector->floorheight ||
         backsector->floorheight >= frontsector->ceilingheight)
      {
         solid = true;
      }*/
   /*   else if (backsector->ceilingheight == frontsector->ceilingheight &&
         backsector->floorheight == frontsector->floorheight)
      {
         // reject empty lines used for triggers and special events
         // ...except SRB2 doesn't really use them! Is it worth tossing?
         if (!(backsector->fofsec || frontsector->fofsec) &&
            *(int32_t *)&backsector->floorpic == *(int32_t *)&frontsector->floorpic && // compares both floorpic, ceilingpic, lightlevel, and special
            // hack to get rid of the extu.w on SH-2
            SIDETEX(sidedef)->midtexture == 0)
            return;
      }*/
/*   }

   rbsp->curline = line;
   rbsp->curside = sidedef;
   rbsp->curldef = ldef;
   
   R_ClipWallSegment(rbsp, x1, x2, solid);
}*/

visplane_t *floorplane, *ceilingplane;
//
// Determine floor/ceiling planes, add sprites of things in sector,
// draw one or more segments.
//
static void R_Subsector(rbspWork_t *rbsp, int num)
{
   const subsector_t *sub = &subsectors[num];
   seg_t       *line;
   int          count;
   const int secnum = sub->isector;

   if (sector_thinglist[secnum]) // frontsector->thinglist
   {
      if(validcount[secnum+1] != validcount[0]) // not already processed?
      {
         validcount[secnum+1] = validcount[0];  // mark it as processed
         if (vd.lastvissector < vd.vissectors + MAXVISSSEC)
         {
           *vd.lastvissector++ = secnum; // frontsector
         }
      }
   }

   rbsp->frontsubsec = sub;
   rbsp->curfsector = I_TO_SEC(secnum); // frontsector
   line     = &segs[sub->firstline];
   count    = P_GetSubsectorNumlines(sub);

   while(--count >= 0)
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
   int viewc, views;

   solidsegs[0].first = -2;
   solidsegs[0].last  = -1;
   solidsegs[1].first = viewportWidth;
   solidsegs[1].last  = viewportWidth+1;
   rbsp.solidsegs = solidsegs;
   rbsp.newend = &solidsegs[2];
   rbsp.splitspans = viewportWidth + viewportWidth/2;

   viewc = vd.viewcos >> 3;
   views = vd.viewsin >> 3;
   rbsp.viewcs[0] = viewc;
   rbsp.viewcs[1] = views;
   rbsp.viewcs[2] = -views;
   rbsp.viewcs[3] = viewc;
   rbsp.viewntz0 = -(FixedMul(vd.viewx, viewc) + FixedMul(vd.viewy, views));
   rbsp.viewntx0 = -(FixedMul(vd.viewy, viewc) - FixedMul(vd.viewx, views));
#ifdef MARS
   rbsp.colmul = centerX;
#endif
   
   R_RenderBSPNode(&rbsp, numnodes - 1, worldbbox);
}

// EOF

