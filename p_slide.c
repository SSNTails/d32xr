/*
  CALICO

  Code for sliding the player against walls

  The MIT License (MIT)
  
  Copyright (c) 2015 James Haley, Olde Skuul, id Software and ZeniMax Media
  
  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:
  
  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.
  
  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/

#include "doomdef.h"
#include "p_local.h"

typedef struct
{
   mobj_t *slidething;
   fixed_t slidex, slidey;     // the final position
   fixed_t slidedx, slidedy;   // current move for completable frac
   fixed_t blockfrac;          // the fraction of the move that gets completed
   fixed_t blocknvx, blocknvy; // the vector of the line that blocks move
   fixed_t endbox[4];          // final proposed position
   fixed_t nvx, nvy;           // normalized line vector

   vertex_t *p1, *p2; // p1, p2 are line endpoints
   fixed_t p3x, p3y, p4x, p4y; // p3, p4 are move endpoints

	int numspechit;
	line_t **spechit;
} pslidework_t;

#define CLIPRADIUS 23

enum
{
   SIDE_BACK = -1,
   SIDE_ON,
   SIDE_FRONT
};

//
// Simple point-on-line-side check.
//
static int SL_PointOnSide(pslidework_t *sw, fixed_t x, fixed_t y)
{
   fixed_t dx, dy, dist;

   dx = x - sw->p1->x;
   dy = y - sw->p1->y;

   dx = FixedMul(dx, sw->nvx);
   dy = FixedMul(dy, sw->nvy);
   dist = dx + dy;

   if(dist > FRACUNIT)
      return SIDE_FRONT;
   else if(dist < -FRACUNIT)
      return SIDE_BACK;
   else
      return SIDE_ON;
}

//
// Return fractional intercept along the slide line.
//
static fixed_t SL_CrossFrac(pslidework_t *sw)
{
   fixed_t dx, dy, dist1, dist2;

   // project move start and end points onto line normal
   dx = sw->p3x - sw->p1->x;
   dy = sw->p3y - sw->p1->y;
   dx = FixedMul(dx, sw->nvx);
   dy = FixedMul(dy, sw->nvy);
   dist1 = dx + dy;

   dx = sw->p4x - sw->p1->x;
   dy = sw->p4y - sw->p1->y;
   dx = FixedMul(dx, sw->nvx);
   dy = FixedMul(dy, sw->nvy);
   dist2 = dx + dy;

   if((dist1 < 0) == (dist2 < 0))
      return FRACUNIT; // doesn't cross the line
   else
      return FixedDiv(dist1, dist1 - dist2);
}

//
// Call with p1 and p2 set to the endpoints, and nvx, nvy set to a 
// normalized vector. Assumes the start point is definitely on the front side
// of the line. Returns the fraction of the current move that crosses the line
// segment.
//
static void SL_ClipToLine(pslidework_t *sw)
{
   fixed_t frac;
   int     side2, side3;

   // adjust start so it will be the first point contacted on the player circle
   // p3, p4 are move endpoints

   sw->p3x = sw->slidex - CLIPRADIUS * sw->nvx;
   sw->p3y = sw->slidey - CLIPRADIUS * sw->nvy;
   sw->p4x = sw->p3x + sw->slidedx;
   sw->p4y = sw->p3y + sw->slidedy;

   // if the adjusted point is on the other side of the line, the endpoint must
   // be checked.
   side2 = SL_PointOnSide(sw, sw->p3x, sw->p3y);
   if(side2 == SIDE_BACK)
      return; // ClipToPoint and slide along normal to line

   side3 = SL_PointOnSide(sw, sw->p4x, sw->p4y);
   if(side3 == SIDE_ON)
      return; // the move goes flush with the wall
   else if(side3 == SIDE_FRONT)
      return; // move doesn't cross line

   if(side2 == SIDE_ON)
   {
      frac = 0; // moves toward the line
      goto blockmove;
   }

   // the line endpoints must be on opposite sides of the move trace

   // find the fractional intercept
   frac = SL_CrossFrac(sw);

   if(frac < sw->blockfrac)
   {
blockmove:
      sw->blockfrac =  frac;
      sw->blocknvx  = -sw->nvy;
      sw->blocknvy  =  sw->nvx;
   }
}

//
// Check a linedef during wall sliding motion.
//
static boolean SL_CheckLine(line_t *ld, pslidework_t *sw)
{
   fixed_t   opentop, openbottom;
   sector_t *front, *back;
   int       side1;
   vertex_t *vtmp;
   fixed_t  ldbbox[4];

   P_LineBBox(ld, ldbbox);

   // check bounding box
   if(sw->endbox[BOXRIGHT ] < ldbbox[BOXLEFT  ] ||
      sw->endbox[BOXLEFT  ] > ldbbox[BOXRIGHT ] ||
      sw->endbox[BOXTOP   ] < ldbbox[BOXBOTTOM] ||
      sw->endbox[BOXBOTTOM] > ldbbox[BOXTOP   ])
   {
      return true;
   }

   // see if it can possibly block movement
   if(ld->sidenum[1] == -1 || (ld->flags & ML_BLOCKING))
      goto findfrac;

   front = LD_FRONTSECTOR(ld);
   back  = LD_BACKSECTOR(ld);

   if(front->floorheight > back->floorheight)
      openbottom = front->floorheight;
   else
      openbottom = back->floorheight;

   if(openbottom - sw->slidething->z > 24*FRACUNIT)
      goto findfrac; // too big a step up

   if(front->ceilingheight < back->ceilingheight)
      opentop = front->ceilingheight;
   else
      opentop = back->ceilingheight;

   if(opentop - openbottom >= 56*FRACUNIT)
      return true; // the line doesn't block movement

   // the line is definitely blocking movement at this point
findfrac:
   sw->p1  = &vertexes[ld->v1];
   sw->p2  = &vertexes[ld->v2];
   sw->nvx = finesine(ld->fineangle);
   sw->nvy = -finecosine(ld->fineangle);
   
   side1 = SL_PointOnSide(sw, sw->slidex, sw->slidey);
   switch(side1)
   {
   case SIDE_ON:
      return true;
   case SIDE_BACK:
      if(ld->sidenum[1] == -1)
         return true; // don't clip to backs of one-sided lines
      // reverse coordinates and angle
      vtmp = sw->p1;
      sw->p1   = sw->p2;
      sw->p2   = vtmp;
      sw->nvx  = -sw->nvx;
      sw->nvy  = -sw->nvy;
      break;
   default:
      break;
   }

   SL_ClipToLine(sw);
   return true;
}

//
// Returns the fraction of the move that is completable.
//
fixed_t P_CompletableFrac(pslidework_t *sw, fixed_t dx, fixed_t dy)
{
   int xl, xh, yl, yh, bx, by;
   VINT *lvalidcount;

   sw->blockfrac = FRACUNIT;
   sw->slidedx = dx;
   sw->slidedy = dy;

   sw->endbox[BOXTOP   ] = sw->slidey + CLIPRADIUS * FRACUNIT;
   sw->endbox[BOXBOTTOM] = sw->slidey - CLIPRADIUS * FRACUNIT;
   sw->endbox[BOXRIGHT ] = sw->slidex + CLIPRADIUS * FRACUNIT;
   sw->endbox[BOXLEFT  ] = sw->slidex - CLIPRADIUS * FRACUNIT;

   if(dx > 0)
      sw->endbox[BOXRIGHT ] += dx;
   else
      sw->endbox[BOXLEFT  ] += dx;

   if(dy > 0)
      sw->endbox[BOXTOP   ] += dy;
   else
      sw->endbox[BOXBOTTOM] += dy;

   I_GetThreadLocalVar(DOOMTLS_VALIDCOUNT, lvalidcount);
   *lvalidcount = *lvalidcount + 1;
   if (*lvalidcount == 0)
      *lvalidcount = 1;

   // check lines
   xl = sw->endbox[BOXLEFT  ] - bmaporgx;
   xh = sw->endbox[BOXRIGHT ] - bmaporgx;
   yl = sw->endbox[BOXBOTTOM] - bmaporgy;
   yh = sw->endbox[BOXTOP   ] - bmaporgy;

   if(xl < 0)
      xl = 0;
   if(yl < 0)
      yl = 0;
   if(yh < 0)
      return FRACUNIT;
   if(xh < 0)
      return FRACUNIT;

   xl = (unsigned)xl >> MAPBLOCKSHIFT;
   xh = (unsigned)xh >> MAPBLOCKSHIFT;
   yl = (unsigned)yl >> MAPBLOCKSHIFT;
   yh = (unsigned)yh >> MAPBLOCKSHIFT;

   if(xh >= bmapwidth)
      xh = bmapwidth - 1;
   if(yh >= bmapheight)
      yh = bmapheight - 1;

   for(bx = xl; bx <= xh; bx++)
   {
      for(by = yl; by <= yh; by++)
         P_BlockLinesIterator(bx, by, (blocklinesiter_t)SL_CheckLine, sw);
   }

   // examine results
   if(sw->blockfrac < 0x1000)
   {
      sw->blockfrac   = 0;
      sw->numspechit = 0;     // can't cross anything on a bad move
      return 0;           // solid wall
   }

   return sw->blockfrac;
}

//
// Point on side check for special crosses.
//
static int SL_PointOnSide2(fixed_t x1, fixed_t y1, 
                           fixed_t x2, fixed_t y2, 
                           fixed_t x3, fixed_t y3)
{
   fixed_t nx, ny;
   fixed_t dist;

   x1 = (x1 - x2);
   y1 = (y1 - y2);

   nx = (y3 - y2);
   ny = (x2 - x3);

   nx = FixedMul(x1, nx);
   ny = FixedMul(y1, ny);
   dist = nx + ny;

   return ((dist < 0) ? SIDE_BACK : SIDE_FRONT);
}

static void SL_CheckSpecialLines(pslidework_t *sw)
{
   fixed_t x1 = sw->slidething->x;
   fixed_t y1 = sw->slidething->y;
   fixed_t x2 = sw->slidex;
   fixed_t y2 = sw->slidey;

   fixed_t bx, by, xl, xh, yl, yh;
   fixed_t bxl, bxh, byl, byh;
   fixed_t x3, y3, x4, y4;
   int side1, side2;

   VINT *lvalidcount, vc;

   if(x1 < x2)
   {
      xl = x1;
      xh = x2;
   }
   else
   {
      xl = x2;
      xh = x1;
   }

   if(y1 < y2)
   {
      yl = y1;
      yh = y2;
   }
   else
   {
      yl = y2;
      yh = y1;
   }

   bxl = xl - bmaporgx;
   bxh = xh - bmaporgx;
   byl = yl - bmaporgy;
   byh = yh - bmaporgy;

   if(bxl < 0)
      bxl = 0;
   if(byl < 0)
      byl = 0;
   if(byh < 0)
      return;
   if(bxh < 0)
      return;

   bxl = (unsigned)bxl >> MAPBLOCKSHIFT;
   bxh = (unsigned)bxh >> MAPBLOCKSHIFT;
   byl = (unsigned)byl >> MAPBLOCKSHIFT;
   byh = (unsigned)byh >> MAPBLOCKSHIFT;

   if(bxh >= bmapwidth)
      bxh = bmapwidth - 1;
   if(byh >= bmapheight)
      byh = bmapheight - 1;

   sw->numspechit = 0;

   I_GetThreadLocalVar(DOOMTLS_VALIDCOUNT, lvalidcount);
   vc = *lvalidcount + 1;
   if (vc == 0)
      vc = 0;
   *lvalidcount = vc;
   ++lvalidcount;

   for(bx = bxl; bx <= bxh; bx++)
   {
      for(by = byl; by <= byh; by++)
      {
         short  *list;
         line_t *ld;
         int offset = by * bmapwidth + bx;
         offset = *(blockmaplump+4 + offset);
	      fixed_t ldbbox[4];
         
         for(list = blockmaplump + offset; *list != -1; list++)
         {
            ld = &lines[*list];
            if(!ld->special)
               continue;
            if(lvalidcount[*list] == vc)
               continue; // already checked
            
            lvalidcount[*list] = vc;

	         P_LineBBox(ld, ldbbox);
            if(xh < ldbbox[BOXLEFT  ] ||
               xl > ldbbox[BOXRIGHT ] ||
               yh < ldbbox[BOXBOTTOM] ||
               yl > ldbbox[BOXTOP   ])
            {
               continue;
            }

            x3 = vertexes[ld->v1].x;
            y3 = vertexes[ld->v1].y;
            x4 = vertexes[ld->v2].x;
            y4 = vertexes[ld->v2].y;

            side1 = SL_PointOnSide2(x1, y1, x3, y3, x4, y4);
            side2 = SL_PointOnSide2(x2, y2, x3, y3, x4, y4);

            if(side1 == side2)
               continue; // move doesn't cross line

            side1 = SL_PointOnSide2(x3, y3, x1, y1, x2, y2);
            side2 = SL_PointOnSide2(x4, y4, x1, y1, x2, y2);

            if(side1 == side2)
               continue; // line doesn't cross move

            if (sw->numspechit < MAXSPECIALCROSS)
               sw->spechit[sw->numspechit++] = ld;
            if (sw->numspechit == MAXSPECIALCROSS)
               return;
         }
      }
   }
}

#define PT_ADDLINES     1
#define PT_ADDTHINGS    2
#define PT_EARLYOUT     4

typedef struct {
  fixed_t     frac;           // along trace line
  boolean     isaline;
  union {
    mobj_t* thing;
    line_t* line;
  } d;
} intercept_t;

typedef boolean (*traverser_t)(intercept_t *in);

#define MAXINTERCEPTS 128
intercept_t	intercepts[MAXINTERCEPTS];
intercept_t*	intercept_p;

fixed_t   bestslidefrac;
fixed_t   secondslidefrac;

line_t*   bestslideline;
line_t*   secondslideline;

mobj_t*   slidemo;

fixed_t   tmxmove;
fixed_t   tmymove;

divline_t trace;


// PIT_AddLineIntercepts.
// Looks for lines in the given block
// that intercept the given trace
// to add to the intercepts list.
//
// A line is crossed if its endpoints
// are on opposite sides of the trace.
//
// killough 5/3/98: reformatted, cleaned up

//
// INTERCEPT ROUTINES
//

boolean PIT_AddLineIntercepts(line_t *ld, pmovetest_t *mt)
{
  int       s1;
  int       s2;
  fixed_t   frac;
  divline_t dl;

  // avoid precision problems with two routines
  if (trace.dx >  FRACUNIT*16 || trace.dy >  FRACUNIT*16 ||
      trace.dx < -FRACUNIT*16 || trace.dy < -FRACUNIT*16)
    {
      s1 = P_PointOnDivlineSide (vertexes[ld->v1].x, vertexes[ld->v1].y, &trace);
      s2 = P_PointOnDivlineSide (vertexes[ld->v2].x, vertexes[ld->v2].y, &trace);
    }
  else
    {
      s1 = P_PointOnLineSide (trace.x, trace.y, ld);
      s2 = P_PointOnLineSide (trace.x+trace.dx, trace.y+trace.dy, ld);
    }

  if (s1 == s2)
    return true;        // line isn't crossed

  // hit the line
  P_MakeDivline(ld, &dl);
  frac = P_InterceptVector(&trace, &dl);

  if (frac < 0)
    return true;        // behind source

  intercept_p->frac = frac;
  intercept_p->isaline = true;
  intercept_p->d.line = ld;
  intercept_p++;

  return true;  // continue
}

//
// PIT_AddThingIntercepts
//
// killough 5/3/98: reformatted, cleaned up

boolean PIT_AddThingIntercepts(mobj_t *thing, pmovetest_t *mt)
{
  fixed_t   x1, y1;
  fixed_t   x2, y2;
  int       s1, s2;
  divline_t dl;
  fixed_t   frac;

  // check a corner to corner crossection for hit
  if ((trace.dx ^ trace.dy) > 0)
    {
      x1 = thing->x - thing->radius;
      y1 = thing->y + thing->radius;
      x2 = thing->x + thing->radius;
      y2 = thing->y - thing->radius;
    }
  else
    {
      x1 = thing->x - thing->radius;
      y1 = thing->y - thing->radius;
      x2 = thing->x + thing->radius;
      y2 = thing->y + thing->radius;
    }

  s1 = P_PointOnDivlineSide (x1, y1, &trace);
  s2 = P_PointOnDivlineSide (x2, y2, &trace);

  if (s1 == s2)
    return true;                // line isn't crossed

  dl.x = x1;
  dl.y = y1;
  dl.dx = x2-x1;
  dl.dy = y2-y1;

  frac = P_InterceptVector (&trace, &dl);

  if (frac < 0)
    return true;                // behind source

  intercept_p->frac = frac;
  intercept_p->isaline = false;
  intercept_p->d.thing = thing;
  intercept_p++;

  return true;          // keep going
}


//
// P_TraverseIntercepts
// Returns true if the traverser function returns true
// for all lines.
//
// killough 5/3/98: reformatted, cleaned up

boolean P_TraverseIntercepts(traverser_t func, fixed_t maxfrac)
{
  intercept_t *in = NULL;
  int count = intercept_p - intercepts;
  while (count--)
    {
      fixed_t dist = INT32_MAX;
      intercept_t *scan;
      for (scan = intercepts; scan < intercept_p; scan++)
        if (scan->frac < dist)
          dist = (in=scan)->frac;
      if (dist > maxfrac)
        return true;    // checked everything in range
      if (!func(in))
        return false;           // don't bother going farther
      in->frac = INT32_MAX;
    }
  return true;                  // everything was traversed
}

//
// P_PathTraverse
// Traces a line from x1,y1 to x2,y2,
// calling the traverser function for each.
// Returns true if the traverser function returns true
// for all lines.
//
// killough 5/3/98: reformatted, cleaned up

boolean P_PathTraverse(fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2,
                       int flags, boolean trav(intercept_t *))
{
  fixed_t xt1, yt1;
  fixed_t xt2, yt2;
  fixed_t xstep, ystep;
  fixed_t partial;
  fixed_t xintercept, yintercept;
  int     mapx, mapy;
  int     mapxstep, mapystep;
  int     count;
  VINT *lvalidcount;

  I_GetThreadLocalVar(DOOMTLS_VALIDCOUNT, lvalidcount);
   *lvalidcount = *lvalidcount + 1;
   if (*lvalidcount == 0)
      *lvalidcount = 1;
      
  intercept_p = intercepts;

  if (!((x1-bmaporgx)&(MAPBLOCKSIZE-1)))
    x1 += FRACUNIT;     // don't side exactly on a line

  if (!((y1-bmaporgy)&(MAPBLOCKSIZE-1)))
    y1 += FRACUNIT;     // don't side exactly on a line

  trace.x = x1;
  trace.y = y1;
  trace.dx = x2 - x1;
  trace.dy = y2 - y1;

  x1 -= bmaporgx;
  y1 -= bmaporgy;
  xt1 = x1>>MAPBLOCKSHIFT;
  yt1 = y1>>MAPBLOCKSHIFT;

  x2 -= bmaporgx;
  y2 -= bmaporgy;
  xt2 = x2>>MAPBLOCKSHIFT;
  yt2 = y2>>MAPBLOCKSHIFT;

  if (xt2 > xt1)
    {
      mapxstep = 1;
      partial = FRACUNIT - ((x1>>MAPBTOFRAC)&(FRACUNIT-1));
      // proff: Changed abs to D_abs (see m_fixed.h)
      ystep = FixedDiv (y2-y1,D_abs(x2-x1));
    }
  else
    if (xt2 < xt1)
      {
        mapxstep = -1;
        partial = (x1>>MAPBTOFRAC)&(FRACUNIT-1);
        // proff: Changed abs to D_abs (see m_fixed.h)
        ystep = FixedDiv (y2-y1,D_abs(x2-x1));
      }
    else
      {
        mapxstep = 0;
        partial = FRACUNIT;
        ystep = 256*FRACUNIT;
      }

  yintercept = (y1>>MAPBTOFRAC) + FixedMul(partial, ystep);

  if (yt2 > yt1)
    {
      mapystep = 1;
      partial = FRACUNIT - ((y1>>MAPBTOFRAC)&(FRACUNIT-1));
      // proff: Changed abs to D_abs (see m_fixed.h)
      xstep = FixedDiv (x2-x1,D_abs(y2-y1));
    }
  else
    if (yt2 < yt1)
      {
        mapystep = -1;
        partial = (y1>>MAPBTOFRAC)&(FRACUNIT-1);
        // proff: Changed abs to D_abs (see m_fixed.h)
        xstep = FixedDiv (x2-x1,D_abs(y2-y1));
      }
    else
      {
        mapystep = 0;
        partial = FRACUNIT;
        xstep = 256*FRACUNIT;
      }

  xintercept = (x1>>MAPBTOFRAC) + FixedMul (partial, xstep);

  // Step through map blocks.
  // Count is present to prevent a round off error
  // from skipping the break.

  mapx = xt1;
  mapy = yt1;

  for (count = 0; count < 64; count++)
    {
      if (flags & PT_ADDLINES)
        if (!P_BlockLinesIterator(mapx, mapy,(blocklinesiter_t)PIT_AddLineIntercepts, NULL))
          return false; // early out

      if (flags & PT_ADDTHINGS)
        if (!P_BlockThingsIterator(mapx, mapy,(blockthingsiter_t)PIT_AddThingIntercepts, NULL))
          return false; // early out

      if (mapx == xt2 && mapy == yt2)
        break;

      if ((yintercept >> FRACBITS) == mapy)
        {
          yintercept += ystep;
          mapx += mapxstep;
        }
      else
        if ((xintercept >> FRACBITS) == mapx)
          {
            xintercept += xstep;
            mapy += mapystep;
          }
    }

  // go through the sorted list
  return P_TraverseIntercepts(trav, FRACUNIT);
}

//
// P_HitSlideLine
// Adjusts the xmove / ymove
// so that the next move will slide along the wall.
// If the floor is icy, then you can bounce off a wall.             // phares
//

void P_HitSlideLine (line_t* ld)
  {
  int     side;
  angle_t lineangle;
  angle_t moveangle;
  angle_t deltaangle;
  fixed_t movelen;
  fixed_t newlen;

  // Check for the special cases of horz or vert walls.
  if (ld->flags & ML_ST_HORIZONTAL)
    {
      tmymove = 0; // no more movement in the Y direction
    return;
    }

  if (ld->flags & ML_ST_VERTICAL)
    {
      tmxmove = 0; // no more movement in the X direction
    return;
    }

  side = P_PointOnLineSide (slidemo->x, slidemo->y, ld);

   const fixed_t dx = vertexes[ld->v2].x - vertexes[ld->v1].x;
   const fixed_t dy = vertexes[ld->v2].y - vertexes[ld->v1].y;
  lineangle = R_PointToAngle2 (0,0, dx, dy);
  if (side == 1)
    lineangle += ANG180;
  moveangle = R_PointToAngle2 (0,0, tmxmove, tmymove);

  // killough 3/2/98:
  // The moveangle+=10 breaks v1.9 demo compatibility in
  // some demos, so it needs demo_compatibility switch.

    moveangle += 10; // prevents sudden path reversal due to 
                     // rounding error  
  deltaangle = moveangle-lineangle; 
  movelen = P_AproxDistance (tmxmove, tmymove);
  
    if (deltaangle > ANG180)
      deltaangle += ANG180;

    //  I_Error ("SlideLine: ang>ANG180");

    lineangle >>= ANGLETOFINESHIFT;
    deltaangle >>= ANGLETOFINESHIFT;
    newlen = FixedMul (movelen, finecosine(deltaangle));
    tmxmove = FixedMul (newlen, finecosine(lineangle));
    tmymove = FixedMul (newlen, finesine(lineangle));
}

//
// PTR_SlideTraverse
//

boolean PTR_SlideTraverse (intercept_t* in)
  {
  line_t* li;

  if (!in->isaline)
    I_Error ("PTR_SlideTraverse: not a line?");

  li = in->d.line;

  if ( ! (li->flags & ML_TWOSIDED) )
    {
    if (P_PointOnLineSide (slidemo->x, slidemo->y, li))
      return true; // don't hit the back side

    goto isblocking;
    }

  // set openrange, opentop, openbottom.
  // These define a 'window' from one sector to another across a line

  fixed_t opentop, openbottom;
  fixed_t openrange = P_LineOpening (li, &opentop, &openbottom);

  if (openrange < slidemo->height)
  {
    goto isblocking;  // doesn't fit
  }
  if (opentop - slidemo->z < slidemo->height)
  {
    goto isblocking;  // mobj is too high
  }

  if (openbottom - slidemo->z > 24*FRACUNIT )
  {
    goto isblocking;  // too big a step up
  }

  // this line doesn't block movement

  return true;

  // the line does block movement,
  // see if it is closer than best so far

isblocking:

  if (in->frac < bestslidefrac)
    {
    secondslidefrac = bestslidefrac;
    secondslideline = bestslideline;
    bestslidefrac = in->frac;
    bestslideline = li;
    }

  return false; // stop
  }

//
// P_SlideMove
// The momx / momy move is bad, so try to slide
// along a wall.
// Find the first line hit, move flush to it,
// and slide along it
//
// This is a kludgy mess.
//

void P_SlideMove (mobj_t *mo)
  {
  fixed_t leadx;
  fixed_t leady;
  fixed_t trailx;
  fixed_t traily;
  fixed_t newx;
  fixed_t newy;
  int     hitcount;
  ptrymove_t pm;

  slidemo = mo; // the object that's sliding
  hitcount = 0;

retry:

  if (++hitcount == 3)
    goto stairstep;   // don't loop forever

  // trace along the three leading corners

  if (mo->momx > 0)
    {
    leadx = mo->x + mo->radius;
    trailx = mo->x - mo->radius;
    }
  else
    {
    leadx = mo->x - mo->radius;
    trailx = mo->x + mo->radius;
    }

  if (mo->momy > 0)
    {
    leady = mo->y + mo->radius;
    traily = mo->y - mo->radius;
    }
  else
    {
    leady = mo->y - mo->radius;
    traily = mo->y + mo->radius;
    }

  bestslidefrac = FRACUNIT+1;

  P_PathTraverse ( leadx, leady, leadx+mo->momx, leady+mo->momy,
     PT_ADDLINES, PTR_SlideTraverse );
  P_PathTraverse ( trailx, leady, trailx+mo->momx, leady+mo->momy,
     PT_ADDLINES, PTR_SlideTraverse );
  P_PathTraverse ( leadx, traily, leadx+mo->momx, traily+mo->momy,
     PT_ADDLINES, PTR_SlideTraverse );

  // move up to the wall

  if (bestslidefrac == FRACUNIT+1)
    {
    // the move must have hit the middle, so stairstep
  stairstep:

    if (!P_TryMove (&pm, mo, mo->x, mo->y + mo->momy))
      P_TryMove (&pm, mo, mo->x + mo->momx, mo->y);

      CONS_Printf("%d, %d", mo->momx, mo->momy);
    return;
    }

  // fudge a bit to make sure it doesn't hit

  bestslidefrac -= 0x800;
  if (bestslidefrac > 0)
    {
    newx = FixedMul (mo->momx, bestslidefrac);
    newy = FixedMul (mo->momy, bestslidefrac);

    // killough 3/15/98: Allow objects to drop off ledges

    if (!P_TryMove (&pm, mo, mo->x+newx, mo->y+newy))
      goto stairstep;
    }

  // Now continue along the wall.
  // First calculate remainder.

  bestslidefrac = FRACUNIT-(bestslidefrac+0x800);

  if (bestslidefrac > FRACUNIT)
    bestslidefrac = FRACUNIT;

CONS_Printf("bestslidefrac: %d", bestslidefrac);
  if (bestslidefrac <= 0)
    return;

  tmxmove = FixedMul (mo->momx, bestslidefrac);
  tmymove = FixedMul (mo->momy, bestslidefrac);

//  CONS_Printf("tmxmove: %d, tmymove: %d", tmxmove, tmymove);

  P_HitSlideLine (bestslideline); // clip the moves

  mo->momx = tmxmove;
  mo->momy = tmymove;

  if (!P_TryMove (&pm, mo, mo->x+tmxmove, mo->y+tmymove))
    goto retry;
  }

// EOF

