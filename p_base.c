/*
  CALICO
  
  Core mobj thinking, movement, and clipping checks.
  Derived from Doom 64 EX, used under MIT by permission.

  The MIT License (MIT)

  Copyright (C) 2016 Samuel Villarreal, James Haley
  
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
#include "mars.h"

typedef struct
{
   mobj_t      *checkthing, *hitthing;
   fixed_t      testx, testy;
   fixed_t      testfloorz, testceilingz, testdropoffz;
   subsector_t *testsubsec;
   line_t      *ceilingline;
   fixed_t      testbbox[4];
   int          testflags;
} pmovetest_t;

static boolean PB_CheckThing(mobj_t* thing, pmovetest_t *mt) ATTR_DATA_CACHE_ALIGN;
static boolean PB_BoxCrossLine(line_t* ld, pmovetest_t *mt) ATTR_DATA_CACHE_ALIGN;
static boolean PB_CheckLine(line_t* ld, pmovetest_t *mt) ATTR_DATA_CACHE_ALIGN;
static boolean PB_CrossCheck(line_t* ld, pmovetest_t *mt) ATTR_DATA_CACHE_ALIGN;
static boolean PB_CheckPosition(pmovetest_t *mt) ATTR_DATA_CACHE_ALIGN;
static boolean PB_TryMove(pmovetest_t *mt, mobj_t* mo, fixed_t tryx, fixed_t tryy) ATTR_DATA_CACHE_ALIGN;
static void P_FloatChange(mobj_t* mo) ATTR_DATA_CACHE_ALIGN;
void P_MobjThinker(mobj_t* mobj) ATTR_DATA_CACHE_ALIGN;

//
// Check for collision against another mobj in one of the blockmap cells.
//
static boolean PB_CheckThing(mobj_t *thing, pmovetest_t *mt)
{
   fixed_t  blockdist;
   int      delta;
   mobj_t  *mo;

   if(!(thing->flags & MF_SOLID))
      return true; // not blocking

   mo = mt->checkthing;
   blockdist = mobjinfo[thing->type].radius + mobjinfo[mo->type].radius;

   delta = thing->x - mt->testx;
   if(delta < 0)
      delta = -delta;
   if(delta >= blockdist)
      return true; // didn't hit it

   delta = thing->y - mt->testy;
   if(delta < 0)
      delta = -delta;
   if(delta >= blockdist)
      return true; // didn't hit it

   if(thing == mo)
      return true; // don't clip against self

   // check for skulls slamming into things
   /*if(mt->testflags & MF_SKULLFLY)
   {
      mt->hitthing = thing;
      return false;
   }*/

   // missiles can hit other things
   if(mt->testflags & MF_MISSILE)
   {
      if(mo->z > thing->z + (thing->theight << FRACBITS))
         return true; // went over
      if(mo->z + (mo->theight << FRACBITS) < thing->z)
         return true; // went underneath
      if(mo->target->type == thing->type) // don't hit same species as originator
      {
         if(thing == mo->target)
            return true; // don't explode on shooter
         if(thing->type != MT_PLAYER) // let players missile other players
            return false; // explode but do no damage
      }

      if(!(thing->flags & MF_SHOOTABLE))
         return !(thing->flags & MF_SOLID); // didn't do any damage

      // damage/explode
      mt->hitthing = thing;
      return false;
   }

   return !(thing->flags & MF_SOLID);
}

//
// Test for a bounding box collision with a linedef.
//
static boolean PB_BoxCrossLine(line_t *ld, pmovetest_t *mt)
{
   fixed_t x1, x2;
   fixed_t lx, ly;
   fixed_t ldx, ldy;
   fixed_t dx1, dy1, dx2, dy2;
   boolean side1, side2;
   fixed_t ldbbox[4];

   P_LineBBox(ld, ldbbox);

   // entirely outside bounding box of line?
   if(mt->testbbox[BOXRIGHT ] <= ldbbox[BOXLEFT  ] ||
      mt->testbbox[BOXLEFT  ] >= ldbbox[BOXRIGHT ] ||
      mt->testbbox[BOXTOP   ] <= ldbbox[BOXBOTTOM] ||
      mt->testbbox[BOXBOTTOM] >= ldbbox[BOXTOP   ])
   {
      return false;
   }

   if(ld->flags & ML_ST_POSITIVE)
   {
      x1 = mt->testbbox[BOXLEFT ];
      x2 = mt->testbbox[BOXRIGHT];
   }
   else
   {
      x1 = mt->testbbox[BOXRIGHT];
      x2 = mt->testbbox[BOXLEFT ];
   }

   lx  = vertexes[ld->v1].x;
   ly  = vertexes[ld->v1].y;
   ldx = (vertexes[ld->v2].x - vertexes[ld->v1].x) >> FRACBITS;
   ldy = (vertexes[ld->v2].y - vertexes[ld->v1].y) >> FRACBITS;

   dx1 = (x1 - lx) >> FRACBITS;
   dy1 = (mt->testbbox[BOXTOP] - ly) >> FRACBITS;
   dx2 = (x2 - lx) >> FRACBITS;
   dy2 = (mt->testbbox[BOXBOTTOM] - ly) >> FRACBITS;

   side1 = (ldy * dx1 < dy1 * ldx);
   side2 = (ldy * dx2 < dy2 * ldx);

   return (side1 != side2);
}

//
// Adjusts testfloorz and testceilingz as lines are contacted.
//
static boolean PB_CheckLine(line_t *ld, pmovetest_t *mt)
{
   fixed_t   opentop, openbottom, lowfloor;
   sector_t *front, *back;

   // The moving thing's destination position will cross the given line.
   // if this should not be allowed, return false.
   if(ld->sidenum[1] == -1)
      return false; // one-sided line

   if(!(mt->testflags & MF_MISSILE) && (ld->flags & (ML_BLOCKING|ML_BLOCKMONSTERS)))
      return false; // explicitly blocking

   front = LD_FRONTSECTOR(ld);
   back  = LD_BACKSECTOR(ld);

   if(front->ceilingheight < back->ceilingheight)
      opentop = front->ceilingheight;
   else
      opentop = back->ceilingheight;

   if(front->floorheight > back->floorheight)
   {
      openbottom = front->floorheight;
      lowfloor   = back->floorheight;
   }
   else
   {
      openbottom = back->floorheight;
      lowfloor   = front->floorheight;
   }

   // adjust floor/ceiling heights
   if(opentop < mt->testceilingz)
   {
      mt->testceilingz = opentop;
      mt->ceilingline  = ld;
   }
   if(openbottom > mt->testfloorz)
      mt->testfloorz = openbottom;
   if(lowfloor < mt->testdropoffz)
      mt->testdropoffz = lowfloor;

   return true;
}

//
// Check a thing against a linedef in one of the blockmap cells.
//
static boolean PB_CrossCheck(line_t *ld, pmovetest_t *mt)
{
   if(PB_BoxCrossLine(ld, mt))
   {
      if(!PB_CheckLine(ld, mt))
         return false;
   }
   return true;
}

//
// Check an mobj's position for validity against lines and other mobjs
//
static boolean PB_CheckPosition(pmovetest_t *mt)
{
   int xl, xh, yl, yh, bx, by;
   VINT *lvalidcount;
   mobj_t *mo = mt->checkthing;

   mt->testflags = mo->flags;
   if (mo->player)
      mt->testflags |= 0x80000000;

   mt->testbbox[BOXTOP   ] = mt->testy + mobjinfo[mo->type].radius;
   mt->testbbox[BOXBOTTOM] = mt->testy - mobjinfo[mo->type].radius;
   mt->testbbox[BOXRIGHT ] = mt->testx + mobjinfo[mo->type].radius;
   mt->testbbox[BOXLEFT  ] = mt->testx - mobjinfo[mo->type].radius;

   // the base floor / ceiling is from the subsector that contains the point.
   // Any contacted lines the step closer together will adjust them.
   mt->testsubsec   = R_PointInSubsector(mt->testx, mt->testy);
   mt->testfloorz   = mt->testdropoffz = mt->testsubsec->sector->floorheight;
   mt->testceilingz = mt->testsubsec->sector->ceilingheight;

   I_GetThreadLocalVar(DOOMTLS_VALIDCOUNT, lvalidcount);
   *lvalidcount = *lvalidcount + 1;
   if (*lvalidcount == 0)
      *lvalidcount = 1;

   mt->ceilingline = NULL;
   mt->hitthing    = NULL;

   if (mt->checkthing->flags & MF_NOCLIP)
      return true;

   // the bounding box is extended by MAXRADIUS because mobj_ts are grouped into
   // mapblocks based on their origin point, and can overlap into adjacent blocks
   // by up to MAXRADIUS units
   xl = mt->testbbox[BOXLEFT  ] - bmaporgx - MAXRADIUS;
   xh = mt->testbbox[BOXRIGHT ] - bmaporgx + MAXRADIUS;
   yl = mt->testbbox[BOXBOTTOM] - bmaporgy - MAXRADIUS;
   yh = mt->testbbox[BOXTOP   ] - bmaporgy + MAXRADIUS;

   if(xl < 0)
      xl = 0;
   if(yl < 0)
      yl = 0;
   if(xh < 0)
      return true;
   if(yh < 0)
      return true;

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
      {
         if(((mo->flags & MF_SOLID) || (mo->flags & MF_MISSILE)) && !P_BlockThingsIterator(bx, by, (blockthingsiter_t)PB_CheckThing, mt))
            return false;
         if(!P_BlockLinesIterator(bx, by, (blocklinesiter_t)PB_CrossCheck, mt))
            return false;
      }
   }

   return true;
}

// 
// Try to move to the new position, and relink the mobj to the new position if
// successful.
//
static boolean PB_TryMove(pmovetest_t *mt, mobj_t *mo, fixed_t tryx, fixed_t tryy)
{
   mt->testx = tryx;
   mt->testy = tryy;
   mt->checkthing = mo; // store for PB_CheckThing

   if(!PB_CheckPosition(mt))
      return false; // solid wall or thing

   if (!(mo->flags & MF_NOCLIP))
   {
      if(mt->testceilingz - mt->testfloorz < (mo->theight << FRACBITS))
         return false; // doesn't fit
      if(mt->testceilingz - mo->z < (mo->theight << FRACBITS))
         return false; // mobj must lower itself to fit
      if(mt->testfloorz - mo->z > 24*FRACUNIT)
         return false; // too big a step up
      if (!(mt->testflags & (MF_FLOAT|0x80000000)) && mt->testfloorz - mt->testdropoffz > 24*FRACUNIT)
         return false; // don't stand over a dropoff
   }

   // the move is ok, so link the thing into its new position
   P_UnsetThingPosition(mo);
   mo->floorz   = mt->testfloorz;
   mo->ceilingz = mt->testceilingz;
   mo->x        = tryx;
   mo->y        = tryy;
   P_SetThingPosition2(mo, mt->testsubsec);

   return true;
}

static void P_BounceMove(mobj_t *mo)
{
   // For now, instead of finding the line we hit, just reverse momentum.
   mo->momx = -mo->momx;
   mo->momy = -mo->momy;
}

#define STOPSPEED 0x1000
#define FRICTION  0xd240

//
// Do horizontal movement.
//
void P_XYMovement(mobj_t *mo)
{
   fixed_t xleft, yleft, xuse, yuse;

   xleft = xuse = (mo->momx*2) & ~7;
   yleft = yuse = (mo->momy*2) & ~7;

   while(xuse > MAXMOVE || xuse < -MAXMOVE || yuse > MAXMOVE || yuse < -MAXMOVE)
   {
      xuse >>= 1;
      yuse >>= 1;
   }

   while(xleft || yleft)
   {
      pmovetest_t mt;

      xleft -= xuse;
      yleft -= yuse;

      if(!PB_TryMove(&mt, mo, mo->x + xuse, mo->y + yuse))
      {
         // blocked move

         // flying skull?
/*         if(mo->flags & MF_SKULLFLY)
         {
            mo->extradata = (intptr_t)mt.hitthing;
            mo->latecall = L_SkullBash;
            return;
         }*/

        if (mo->type == MT_FLINGRING)
        {
            P_BounceMove(mo);
            xleft = yleft = 0;
        }

         // explode a missile?
         if(mo->flags & MF_MISSILE)
         {
            if(mt.ceilingline && mt.ceilingline->sidenum[1] != -1 && LD_BACKSECTOR(mt.ceilingline)->ceilingpic == -1)
            {
               mo->latecall = P_RemoveMobj;
               return;
            }
            mo->extradata = (intptr_t)mt.hitthing;
            mo->latecall = L_MissileHit;
            return;
         }

         mo->momx = mo->momy = 0;
         return;
      }
   }

   // slow down

   if(mo->flags & MF_MISSILE)
      return; // no friction for missiles or flying skulls ever

   if (mo->type == MT_FLINGRING)
      return;

   if(mo->z > mo->floorz)
      return; // no friction when airborne

   if(mo->momx > -STOPSPEED && mo->momx < STOPSPEED &&
      mo->momy > -STOPSPEED && mo->momy < STOPSPEED)
   {
      mo->momx = 0;
      mo->momy = 0;
   }
   else
   {
      mo->momx = (mo->momx >> 8) * (FRICTION >> 8);
      mo->momy = (mo->momy >> 8) * (FRICTION >> 8);
   }
}

//
// Float a flying monster up or down.
//
static void P_FloatChange(mobj_t *mo)
{
   mobj_t *target;
   fixed_t dist, delta;

   target = mo->target;                              // get the target object
   delta  = (target->z + (mo->theight >> (FRACBITS-1))) - mo->z; // get the height difference
   
   dist   = P_AproxDistance(target->x - mo->x, target->y - mo->y);
   delta *= 3;

   if(delta < 0)
   {
      if(dist < -delta)
         mo->z -= FLOATSPEED; // adjust height downward
   }
   else if(dist < delta)
      mo->z += FLOATSPEED;    // adjust height upward
}

//
// Do movement on the z axis.
//
void P_ZMovement(mobj_t *mo)
{
   mo->z += mo->momz;

   if((mo->flags & MF_FLOAT) && (mo->flags & MF_ENEMY) && mo->target)
   {
      // float toward target if too close
      P_FloatChange(mo);
   }

   // clip movement
   if(mo->z <= mo->floorz)
   {
      mo->z = mo->floorz; // hit the floor

      if (mo->type == MT_FLINGRING)
      {
         mo->momz = -FixedMul(mo->momz, FixedDiv(17*FRACUNIT, 20*FRACUNIT));
      }
      else if (mo->type == MT_GFZDEBRIS)
      {
         P_RemoveMobj(mo);
      }
      else if (mo->type == MT_MEDIUMBUBBLE)
      {
         // Hit the floor, so split!
         mobj_t *explodemo;
         explodemo = P_SpawnMobj(mo->x, mo->y, mo->z, MT_SMALLBUBBLE);
         explodemo->momx += (P_Random() % 32) * FRACUNIT/8;
         explodemo->momy += (P_Random() % 32) * FRACUNIT/8;
         explodemo = P_SpawnMobj(mo->x, mo->y, mo->z, MT_SMALLBUBBLE);
         explodemo->momx += (P_Random() % 64) * FRACUNIT/8;
         explodemo->momy -= (P_Random() % 64) * FRACUNIT/8;
         explodemo = P_SpawnMobj(mo->x, mo->y, mo->z, MT_SMALLBUBBLE);
         explodemo->momx -= (P_Random() % 128) * FRACUNIT/8;
         explodemo->momy += (P_Random() % 128) * FRACUNIT/8;
         explodemo = P_SpawnMobj(mo->x, mo->y, mo->z, MT_SMALLBUBBLE);
         explodemo->momx -= (P_Random() % 96) * FRACUNIT/8;
         explodemo->momy -= (P_Random() % 96) * FRACUNIT/8;

         P_RemoveMobj(mo);
      }
      else if (mo->type == MT_SMALLBUBBLE)
      {
         // Hit the floor, so POP!
         P_RemoveMobj(mo);
      }
      else
      {
         if(mo->momz < 0)
            mo->momz = 0;
         if(mo->flags & MF_MISSILE)
         {
            mo->latecall = P_ExplodeMissile;
            return;
         }
      }
   }
   else if(!(mo->flags & MF_NOGRAVITY))
   {
      // apply gravity
      if (mo->subsector->sector->heightsec != -1
         && sectors[mo->subsector->sector->heightsec].floorheight > mo->z + (mo->theight << (FRACBITS-1)))
         mo->momz -= GRAVITY/2/3; // Less gravity underwater.
      else
         mo->momz -= GRAVITY/2;
   }

   if(mo->z + (mo->theight << FRACBITS) > mo->ceilingz)
   {
      mo->z = mo->ceilingz - (mo->theight << FRACBITS); // hit the ceiling

      if (mo->type == MT_FLINGRING && false) // TODO: MF_VERTICALFLIP
      {
         mo->momz = -FixedMul(mo->momz, FixedDiv(17*FRACUNIT, 20*FRACUNIT));
      }
      else if (mo->type == MT_SMALLBUBBLE || mo->type == MT_MEDIUMBUBBLE
         || mo->type == MT_EXTRALARGEBUBBLE)
      {
         P_RemoveMobj(mo);
      }
      else
      {
         if(mo->momz > 0)
            mo->momz = 0;

         if(mo->flags & MF_MISSILE)
            mo->latecall = P_ExplodeMissile;
      }
   }
}

static void P_Boss1Thinker(mobj_t *mobj)
{
   const mobjinfo_t *mobjInfo = &mobjinfo[MT_EGGMOBILE]; // Always MT_EGGMOBILE
   const state_t *stateInfo = &states[mobj->state];

   if (mobj->health <= 0 && !(mobj->flags2 & MF2_BOSSFLEE))
      return; // Just sleeping, out of bounds...

	if ((mobj->flags2 & MF2_FRET) && stateInfo->nextstate == mobjInfo->spawnstate && mobj->tics == 1)
   {
		mobj->flags2 &= ~(MF2_FRET|MF2_SKULLFLY);
		mobj->momx = mobj->momy = mobj->momz = 0;
	}

   if (mobj->flags2 & MF2_FRET)
      return;

//	if (!mobj->tracer) // TODO: Need new way to handle jet fumes
//	{
//		var1 = 0;
//		A_BossJetFume(mobj);
//	}

	if (!mobj->target || !(mobj->target->flags & MF_SHOOTABLE))
	{
		if (mobj->target && mobj->target->health
         && mobj->target->type == MT_EGGMOBILE_TARGET) // Oh, we're just firing our laser.
            return; // It's okay, then.

		if (mobj->health <= 0)
			return;

		// look for a new target
		if (P_LookForPlayers(mobj, false, true) && mobjInfo->seesound)
			S_StartSound(mobj, mobjInfo->seesound);

		return;
	}

	if (mobj->flags2 & MF2_SKULLFLY)
	{
		fixed_t dist = mobj->z - (mobj->floorz + (2*(mobj->theight<<FRACBITS)));
		if (dist > 0 && mobj->momz > 0)
			mobj->momz = FixedMul(mobj->momz, FRACUNIT - (dist>>12));
	}
	else if (mobj->state != mobjInfo->spawnstate && mobj->health > 0 && mobj->flags & MF_FLOAT)
		mobj->momz = FixedMul(mobj->momz,7*FRACUNIT/8);

	if (mobj->state == mobjInfo->meleestate
		|| (mobj->state == mobjInfo->missilestate
		&& mobj->health > mobjInfo->damage))
		mobj->angle = R_PointToAngle2(mobj->x, mobj->y, mobj->target->x, mobj->target->y);
}

//
// Perform main thinking logic for a single mobj per tic.
//
void P_MobjThinker(mobj_t *mobj)
{
   if (!(mobj->flags & MF_STATIC))
   {
      // momentum movement
      if(mobj->momx || mobj->momy)
         P_XYMovement(mobj);

      // removed or has a special action to perform?
      if(mobj->latecall)
         return;

      if(mobj->z != mobj->floorz || mobj->momz)
         P_ZMovement(mobj);

      // removed or has a special action to perform?
      if(mobj->latecall)
         return;

      switch(mobj->type)
      {
         case MT_FLINGRING:
            mobj->threshold--;
            if (mobj->threshold < 3*TICRATE && (mobj->threshold & 1))
               mobj->flags2 |= MF2_DONTDRAW;
            else
               mobj->flags2 &= ~MF2_DONTDRAW;

            if (mobj->threshold == 0)
            {
               P_RemoveMobj(mobj);
               return;
            }
            break;
         case MT_RING_ICON:
         case MT_ATTRACT_ICON:
         case MT_FORCE_ICON:
         case MT_ARMAGEDDON_ICON:
         case MT_WHIRLWIND_ICON:
         case MT_ELEMENTAL_ICON:
         case MT_SNEAKERS_ICON:
         case MT_INVULN_ICON:
         case MT_1UP_ICON:
            if (mobj->z < mobj->floorz + (mobjinfo[mobj->type].damage << FRACBITS))
               mobj->momz = mobjinfo[mobj->type].speed;
            else
               mobj->momz = 0;
            break;
         case MT_GHOST:
         case MT_EGGMOBILE_TARGET:
            if (mobj->reactiontime)
            {
               mobj->reactiontime--;
               if (mobj->reactiontime == 0)
               {
                  P_RemoveMobj(mobj);
                  return;
               }
            }
            break;
         case MT_EGGMOBILE:
            P_Boss1Thinker(mobj);
            if (mobj->flags2 & MF2_BOSSFLEE)
            {
               if (mobj->extradata)
               {
                  mobj->extradata = (int)mobj->extradata - 1;
                  if (!mobj->extradata)
                  {
                     if (mobj->target)
                     {
                        mobj->momz = FixedMul(FixedDiv(mobj->target->z - mobj->z, P_AproxDistance(mobj->x - mobj->target->x, mobj->y - mobj->target->y)), FRACUNIT >> 8);
                        mobj->angle = R_PointToAngle2(mobj->x, mobj->y, mobj->target->x, mobj->target->y);
                     }
                     else
                        mobj->momz = 8*FRACUNIT;
                  }
               }
               else if (mobj->target)
               {
                  P_InstaThrust(mobj, mobj->angle, 12*FRACUNIT);
                  if (P_AproxDistance(mobj->x - mobj->target->x, mobj->y - mobj->target->y) < 32*FRACUNIT)
                  {
                     mobj->flags2 &= ~MF2_BOSSFLEE;
                     mobj->tics = -1;
                     mobj->momx = mobj->momy = mobj->momz = 0;
                  }
               }
            }
            break;
         default:
            break;
      }
   }
   else
   {
      switch(mobj->type)
      {
         case MT_SCORE:
            mobj->z += mobjinfo[mobj->type].speed;
               break;
      }
   }

   // cycle through states
   if (mobj->tics != -1)
   {
       mobj->tics--;

       // you can cycle through multiple states in a tic
       if (!mobj->tics)
           P_SetMobjState(mobj, states[mobj->state].nextstate);
   }
}

//
// Do the main thinking logic for mobjs during a gametic.
//
void P_RunMobjBase2(void)
{
    mobj_t* mo;
    mobj_t* next;

    // First, handle the ringmobj animations
    for (int i = 0; i < NUMMOBJTYPES; i++)
    {
      const mobjinfo_t *info = &mobjinfo[i];

      if (info->flags & MF_RINGMOBJ)
      {
         // cycle through states
         if (ringmobjtics[i] != -1)
         {
            ringmobjtics[i]--;

            // you can cycle through multiple states in a tic
            if (!ringmobjtics[i])
            {
               do
               {
                  const statenum_t nextstate = states[ringmobjstates[i]].nextstate;

                  const state_t *st = &states[nextstate];

                  ringmobjstates[i] = nextstate;
                  ringmobjtics[i] = st->tics;

                  // Sprite and frame can be derived
               } while (!ringmobjtics[i]);
            }
         }
      }
    }

    for (mo = mobjhead.next; mo != (void*)&mobjhead; mo = next)
    {
#ifdef MARS
        // clear cache for mobj flags following the sight check as 
        // the other CPU might have modified the MF_SEETARGET state
        if (mo->tics == 1)
            Mars_ClearCacheLine(&mo->flags);
#endif
        next = mo->next;	/* in case mo is removed this time */
        if (!mo->player)
            P_MobjThinker(mo);
    }
}

/*
===================
=
= P_RunMobjLate
=
= Run stuff that doesn't happen every tick
===================
*/

void P_RunMobjLate(void)
{
    mobj_t* mo;
    mobj_t* next;

    for (mo = mobjhead.next; mo != (void*)&mobjhead; mo = next)
    {
        next = mo->next;	/* in case mo is removed this time */
        if (mo->flags & (MF_RINGMOBJ|MF_STATIC))
            continue;
        if (mo->latecall)
        {
            mo->latecall(mo);
            mo->latecall = NULL;
        }
    }

    /* move entities, removed this frame, from limbo to free list */
    for (mo = limbomobjhead.next; mo != (void*)&limbomobjhead; mo = next)
    {
        next = mo->next;
        P_FreeMobj(mo);
    }
}
