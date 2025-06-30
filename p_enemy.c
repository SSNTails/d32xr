/* P_enemy.c */

#include "doomdef.h"
#include "p_local.h"


/*
===============================================================================

							ENEMY THINKING

enemies are allways spawned with targetplayer = -1, threshold = 0
Most monsters are spawned unaware of all players, but some can be made preaware

===============================================================================
*/

boolean P_CheckMeleeRange (mobj_t *actor, fixed_t range);
boolean P_CheckMissileRange (mobj_t *actor);
boolean P_Move (mobj_t *actor, fixed_t speed) ATTR_DATA_CACHE_ALIGN;
boolean P_TryWalk (mobj_t *actor) ATTR_DATA_CACHE_ALIGN;
void P_NewChaseDir (mobj_t *actor) ATTR_DATA_CACHE_ALIGN;
void A_Look (mobj_t *actor, int16_t var1, int16_t var2) ATTR_DATA_CACHE_ALIGN;
void A_Chase (mobj_t *actor, int16_t var1, int16_t var2) ATTR_DATA_CACHE_ALIGN;

/*
================
=
= P_CheckMeleeRange
=
================
*/

boolean P_CheckMeleeRange (mobj_t *actor, fixed_t range)
{
	mobj_t		*pl;
	fixed_t		dist;
	
	if (!Mobj_HasFlags2(actor, MF2_SEETARGET))
		return false;
							
	if (!actor->target)
		return false;
		
	pl = actor->target;
	dist = P_AproxDistance (pl->x-actor->x, pl->y-actor->y);
	if (dist >= range)
		return false;
	
	return true;		
}

/*
================
=
= P_CheckMissileRange
=
================
*/

boolean P_CheckMissileRange (mobj_t *actor)
{
	fixed_t		dist;
	const mobjinfo_t* ainfo = &mobjinfo[actor->type];

	if (!Mobj_HasFlags2(actor, MF2_SEETARGET))
		return false;
	
	dist = P_AproxDistance ( actor->x-actor->target->x, actor->y-actor->target->y) - 64*FRACUNIT;
	if (!ainfo->meleestate)
		dist -= 128*FRACUNIT;		/* no melee attack, so fire more */

	dist >>= 16;

	if (dist > 200)
		dist = 200;

	if (P_Random () < dist)
		return false;
		
	return true;
}


/*
================
=
= P_Move
=
= Move in the current direction
= returns false if the move is blocked
================
*/

fixed_t	xspeed[8] = {FRACUNIT,47000,0,-47000,-FRACUNIT,-47000,0,47000};
fixed_t yspeed[8] = {0,47000,FRACUNIT,47000,0,-47000,-FRACUNIT,-47000};

boolean P_Move(mobj_t *actor, fixed_t speed)
{
	fixed_t	tryx, tryy;
	boolean		good;
	ptrymove_t	tm;

	if (actor->movedir == DI_NODIR)
		return false;
	
	tryx = actor->x + speed*xspeed[actor->movedir];
	tryy = actor->y + speed*yspeed[actor->movedir];
	
	if (!P_TryMove (&tm, actor, tryx, tryy) )
	{	/* open any specials */
		if (actor->type != MT_SKIM && (actor->flags2 & MF2_FLOAT) && (actor->flags2 & MF2_ENEMY) && tm.floatok)
		{	/* must adjust height */
			if (actor->z < tm.tmfloorz)
				actor->z += FLOATSPEED;
			else
				actor->z -= FLOATSPEED;
			actor->flags2 |= MF2_INFLOAT;
			return true;
		}

//		blkline = tm.blockline;
		good = false;

		return good;
	}
	else
	{
		actor->flags2 &= ~MF2_INFLOAT;
	}

	if (! (actor->flags2 & MF2_FLOAT) )	
		actor->z = actor->floorz;
	return true; 
}


/*
==================================
=
= TryWalk
=
= Attempts to move actoron in its current (ob->moveangle) direction.
=
= If blocked by either a wall or an actor returns FALSE
= If move is either clear or blocked only by a door, returns TRUE and sets
= If a door is in the way, an OpenDoor call is made to start it opening.
=
==================================
*/

boolean P_TryWalk (mobj_t *actor)
{	
	if (!P_Move (actor, mobjinfo[actor->type].speed))
		return false;

	actor->movecount = P_Random()&15;
	return true;
}



/*
================
=
= P_NewChaseDir
=
================
*/

dirtype_t opposite[] =
{DI_WEST, DI_SOUTHWEST, DI_SOUTH, DI_SOUTHEAST, DI_EAST, DI_NORTHEAST,
DI_NORTH, DI_NORTHWEST, DI_NODIR};

dirtype_t diags[] = {DI_NORTHWEST,DI_NORTHEAST,DI_SOUTHWEST,DI_SOUTHEAST};

void P_NewChaseDir (mobj_t *actor)
{
	fixed_t		deltax,deltay;
	dirtype_t	d[3];
	dirtype_t	tdir, olddir, turnaround;

	if (!actor->target)
		I_Error ("P_NewChaseDir: called with no target");

	deltax = actor->target->x - actor->x;
	deltay = actor->target->y - actor->y;

	if (P_AproxDistance(deltax, deltay) > 4096*FRACUNIT)
	{
//		actor->angle = R_PointToAngle2(actor->x, actor->y, actor->target->x, actor->target->y);
		actor->target = NULL;
		return;
	}

	olddir = actor->movedir;
	turnaround=opposite[olddir];
	
	if (deltax>10*FRACUNIT)
		d[1]= DI_EAST;
	else if (deltax<-10*FRACUNIT)
		d[1]= DI_WEST;
	else
		d[1]=DI_NODIR;
	if (deltay<-10*FRACUNIT)
		d[2]= DI_SOUTH;
	else if (deltay>10*FRACUNIT)
		d[2]= DI_NORTH;
	else
		d[2]=DI_NODIR;

/* try direct route */
	if (d[1] != DI_NODIR && d[2] != DI_NODIR)
	{
		actor->movedir = diags[((deltay<0)<<1)+(deltax>0)];
		if (actor->movedir != turnaround && P_TryWalk(actor))
			return;
	}

/* try other directions */
	if (P_Random() > 200 || D_abs(deltay)> D_abs(deltax))
	{
		tdir=d[1];
		d[1]=d[2];
		d[2]=tdir;
	}

	if (d[1]==turnaround)
		d[1]=DI_NODIR;
	if (d[2]==turnaround)
		d[2]=DI_NODIR;
	
	if (d[1]!=DI_NODIR)
	{
		actor->movedir = d[1];
		if (P_TryWalk(actor))
			return;     /*either moved forward or attacked*/
	}

	if (d[2]!=DI_NODIR)
	{
		actor->movedir =d[2];
		if (P_TryWalk(actor))
			return;
	}

/* there is no direct path to the player, so pick another direction */

	if (olddir!=DI_NODIR)
	{
		actor->movedir =olddir;
		if (P_TryWalk(actor))
			return;
	}

	if (P_Random()&1) 	/*randomly determine direction of search*/
	{
		for (tdir=DI_EAST ; tdir<=DI_SOUTHEAST ; tdir++)
		{
			if (tdir!=turnaround)
			{
				actor->movedir =tdir;
				if ( P_TryWalk(actor) )
					return;
			}
		}
	}
	else
	{
		for (tdir=DI_SOUTHEAST ; (int)tdir >= (int)DI_EAST;tdir--)
		{
			if (tdir!=turnaround)
			{
				actor->movedir =tdir;
				if ( P_TryWalk(actor) )
				return;
			}
		}
	}

	if (turnaround !=  DI_NODIR)
	{
		actor->movedir =turnaround;
		if ( P_TryWalk(actor) )
			return;
	}

	actor->movedir = DI_NODIR;		/* can't move */
}


/*
================
=
= P_LookForPlayers
=
= If allaround is false, only look 180 degrees in front
= returns true if a player is targeted
================
*/

boolean P_LookForPlayers (mobj_t *actor, fixed_t distLimit, boolean allaround, boolean nothreshold)
{
	int 		i, j;
	angle_t		an;
	fixed_t		dist;
	mobj_t		*mo;
	
	if (! (actor->flags2 & MF2_SEETARGET) )
	{	/* pick another player as target if possible */
newtarget:
		for (i = 0; i < MAXPLAYERS; i++)
		{
			if (actor->target == players[i].mo)
			{
				i++; // advance to the next player
				break;
			}
		}

		for (j = 0; j < MAXPLAYERS; j++)
		{
			int p = (i + j) % MAXPLAYERS;
			if (playeringame[p])
			{
				actor->target = players[p].mo;
				break;
			}
		}

		return false;
	}
		
	mo = actor->target;
	if (!mo || mo->health <= 0)
		goto newtarget;

	if (distLimit > 0 && P_AproxDistance(P_AproxDistance(actor->x - mo->x, actor->y - mo->y), actor->z - mo->z) > distLimit)
		return false;
		
	if (!allaround)
	{
		an = R_PointToAngle2 (actor->x, actor->y, 
		mo->x, mo->y) - actor->angle;
		if (an > ANG90 && an < ANG270)
		{
			dist = P_AproxDistance (mo->x - actor->x,
				mo->y - actor->y);
			/* if real close, react anyway */
			if (dist > MELEERANGE)
				return false;		/* behind back */
		}
	}
	
	if (!nothreshold)
		actor->threshold = 60;

	return true;
}


/*
===============================================================================

						ACTION ROUTINES

===============================================================================
*/

/*
==============
=
= A_Look
=
= Stay in state until a player is sighted
=
= var1: distance limit
= var2: allaround if nonzero
==============
*/

void A_Look (mobj_t *actor, int16_t var1, int16_t var2)
{
	const mobjinfo_t* ainfo = &mobjinfo[actor->type];
	
/* if current target is visible, start attacking */

/* if the sector has a living soundtarget, make that the new target */
	actor->threshold = 0;		/* any shot will wake up */

	if (!P_LookForPlayers (actor, var1 << FRACBITS, var2, true) )
		return;
	
//seeyou:
/* go into chase state */
	if (ainfo->seesound)
	{
		int		sound;
		
		switch (ainfo->seesound)
		{
		default:
			sound = ainfo->seesound;
			break;
		}

		S_StartSound (actor, sound);
	}

	P_SetMobjState (actor, ainfo->seestate);
}

void A_SpawnState(mobj_t *actor, int16_t var1, int16_t var2)
{
	if (actor->state != mobjinfo[actor->type].spawnstate)
		P_SetMobjState(actor, mobjinfo[actor->type].spawnstate);
}

/*
==============
=
= A_Chase
=
= Actor has a melee attack, so it tries to close as fast as possible
=
==============
*/

void A_Chase (mobj_t *actor, int16_t var1, int16_t var2)
{
	int		delta;
	const mobjinfo_t* ainfo = &mobjinfo[actor->type];

/* */
/* modify target threshold */
/* */
	if  (actor->threshold)
		actor->threshold--;
	
/* */
/* turn towards movement direction if not there yet */
/* */
	if (actor->movedir < 8)
	{
		actor->angle &= (7<<29);
		delta = actor->angle - (actor->movedir << 29);
		if (delta > 0)
			actor->angle -= ANG90/2;
		else if (delta < 0)
			actor->angle += ANG90/2;
	}

	if (!actor->target || !(actor->target->flags2&MF2_SHOOTABLE)
		|| (netgame && !actor->threshold && !(actor->flags2 & MF2_SEETARGET)))
	{	/* look for a new target */
		actor->target = NULL;
		P_SetMobjState (actor, ainfo->spawnstate);
		return;
	}

/* */
/* check for melee attack */
/*	 */
	if (ainfo->meleestate && P_CheckMeleeRange (actor, actor->type == MT_SKIM ? 8*FRACUNIT : 0))
	{
		if (ainfo->attacksound)
			S_StartSound (actor, ainfo->attacksound);
		P_SetMobjState (actor, ainfo->meleestate);
		return;
	}

/* */
/* check for missile attack */
/* */
	if ( !actor->movecount && ainfo->missilestate
	&& P_CheckMissileRange (actor))
	{
		P_SetMobjState (actor, ainfo->missilestate);
		return;
	}
	
/* */
/* chase towards player */
/* */
	if (--actor->movecount<0 || !P_Move (actor, ainfo->speed))
		P_NewChaseDir (actor);
}

/*============================================================================= */

void A_BuzzFly(mobj_t *actor, int16_t var1, int16_t var2)
{
	if (actor->reactiontime)
		actor->reactiontime--;

	// modify target threshold
	if (actor->threshold)
		actor->threshold--;

	if (!actor->target || !(actor->target->flags2&MF2_SHOOTABLE) || (netgame && !actor->threshold && !(actor->flags2 & MF2_SEETARGET)))
	{
		actor->target = NULL;
		actor->momx = actor->momy = actor->momz = 0;
		P_SetMobjState(actor, mobjinfo[actor->type].spawnstate); // Go back to looking around
		return;
	}

	// turn towards movement direction if not there yet
	actor->angle = R_PointToAngle2(actor->x, actor->y, actor->target->x, actor->target->y);

	// chase towards player
	{
		int dist, realspeed;
//		const fixed_t mf = 5*(FRACUNIT/4);

		realspeed = mobjinfo[actor->type].speed;

		dist = P_AproxDistance(P_AproxDistance(actor->target->x - actor->x,
			actor->target->y - actor->y), actor->target->z - actor->z);

		if (dist < 1)
			dist = 1;

		actor->momx = FixedMul(FixedDiv(actor->target->x - actor->x, dist), realspeed);
		actor->momy = FixedMul(FixedDiv(actor->target->y - actor->y, dist), realspeed);
		actor->momz = FixedMul(FixedDiv(actor->target->z - actor->z, dist), realspeed);

		fixed_t watertop = GetWatertopMo(actor);
		if (actor->z + actor->momz < watertop)
		{
			actor->z = watertop;
			actor->momz = 0;
		}
	}
}

void A_JetJawRoam(mobj_t *actor, int16_t var1, int16_t var2)
{
	if (actor->reactiontime)
	{
		actor->reactiontime--;
		P_InstaThrust(actor, actor->angle, mobjinfo[actor->type].speed*FRACUNIT/4);
	}
	else
	{
		actor->reactiontime = mobjinfo[actor->type].reactiontime;
		actor->angle += ANG180;
	}

	if (!P_LookForPlayers (actor, 2048 << FRACBITS, false, true))
		return;

	P_SetMobjState(actor, mobjinfo[actor->type].seestate);
}

void A_JetJawChomp(mobj_t *actor, int16_t var1, int16_t var2)
{
	const mobjinfo_t *ainfo = &mobjinfo[actor->type];
	int delta;

	if (!(leveltime & 7))
		S_StartSound(actor, ainfo->attacksound);

/* */
/* turn towards movement direction if not there yet */
/* */
	if (actor->movedir < 8)
	{
		actor->angle &= (7<<29);
		delta = actor->angle - (actor->movedir << 29);
		if (delta > 0)
			actor->angle -= ANG90/2;
		else if (delta < 0)
			actor->angle += ANG90/2;
	}

	if (!actor->target || !(actor->target->flags2&MF2_SHOOTABLE)
		|| !actor->target->health || !(actor->flags2 & MF2_SEETARGET))
	{
		actor->target = NULL;
		P_SetMobjState(actor, ainfo->spawnstate);
		return;
	}
	
/* */
/* chase towards player */
/* */
	if (--actor->movecount<0 || !P_Move(actor, ainfo->speed))
		P_NewChaseDir(actor);

	fixed_t watertop = GetWatertopMo(actor);
	if (actor->z > watertop - (actor->theight << FRACBITS))
		actor->z = watertop - (actor->theight << FRACBITS);
}

void A_SkimChase(mobj_t *actor, int16_t var1, int16_t var2)
{
	A_Chase(actor, var1, var2);
	actor->z = GetWatertopMo(actor);
}

// var1: type of object to drop
void A_DropMine(mobj_t *actor, int16_t var1, int16_t var2)
{
	mobj_t *mine = P_SpawnMobj(actor->x, actor->y, actor->z - 12*FRACUNIT, var1);
	mine->momx = FRACUNIT >> 8; // This causes missile collision to occur
	S_StartSound(mine, mobjinfo[actor->type].attacksound);
}

// var1: distance to change to  meleestate
void A_MineRange(mobj_t *actor, int16_t var1, int16_t var2)
{
	if (!actor->target)
		return;

	if (P_AproxDistance(P_AproxDistance(actor->x - actor->target->x, actor->y - actor->target->y), actor->z - actor->target->z)>>FRACBITS < var1)
		P_SetMobjState(actor, mobjinfo[actor->type].meleestate);
}

void A_MineExplode(mobj_t *actor, int16_t var1, int16_t var2)
{
	S_StartSound(actor, mobjinfo[actor->type].deathsound);

	P_RadiusAttack(actor, actor, 128);
	actor->flags |= MF_NOGRAVITY|MF_NOCLIP;
	P_SpawnMobj(actor->x, actor->y, actor->z, mobjinfo[actor->type].mass);
	S_StartSound(actor, sfx_s3k_6c);

	const int dist = 4; // 64
	#define RandomValue ((P_Random() & 1) ? P_Random() / dist : P_Random() / -dist)
	for (int i = 0; i < 8; i++)
	{
		P_SpawnMobj(actor->x+RandomValue*FRACUNIT,
			actor->y+RandomValue*FRACUNIT,
			actor->z+RandomValue*FRACUNIT,
			mobjinfo[actor->type].mass);
	}
	#undef RandomValue
}

void A_SetObjectFlags2(mobj_t *actor, int16_t var1, int16_t var2)
{
	if (var2 == 2)
		actor->flags2 |= var1;
	else if (var2 == 1)
		actor->flags2 &= ~var1;
	else
		actor->flags2 = var1;
}

/*
==============
=
= A_FaceTarget
=
==============
*/

void A_FaceTarget (mobj_t *actor, int16_t var1, int16_t var2)
{	
	if (!actor->target)
		return;
	
	actor->angle = R_PointToAngle2 (actor->x, actor->y
	, actor->target->x, actor->target->y);
}

/*
==================
=
= SkullAttack
=
= Fly at the player like a missile
=
==================
*/

#define	SKULLSPEED		(24*FRACUNIT)
void P_Shoot2 (lineattack_t *la);

void A_SkullAttack (mobj_t *actor, int16_t var1, int16_t var2)
{
	mobj_t			*dest;
	angle_t			an;
	int				dist;
	const mobjinfo_t* mobjInfo = &mobjinfo[actor->type];

	if (!actor->target)
		return;
		
	dest = actor->target;	
	actor->flags2 |= MF2_SKULLFLY;

	S_StartSound (actor, mobjInfo->activesound);
	A_FaceTarget (actor, 0, 0);

	dist = P_AproxDistance (dest->x - actor->x, dest->y - actor->y);

	{
		static int k = 0;
		int i, j;
		angle_t testang = 0;
		lineattack_t la;
		la.shooter = actor;
		la.attackrange = dist;
		la.aimtopslope = 100*FRACUNIT/160;
		la.aimbottomslope = 100*FRACUNIT/160;

		if (P_Random() & 1) // Imaginary 50% chance
		{
			i = 9;
			j = 27;
		}
		else
		{
			i = 27;
			j = 9;
		}

#define dostuff(q) \
			testang = actor->angle + ((i+(q))*(ANG90/9));\
			la.attackangle = testang;\
			P_Shoot2(&la);\
			if (P_AproxDistance(la.shootx - actor->x, la.shooty - actor->y) > dist + 2*mobjInfo->radius)\
				break;

		if (P_Random() & 1) // imaginary 50% chance
		{
			for (k = 0; k < 9; k++)
			{
				dostuff(i+k)
				dostuff(i-k)
				dostuff(j+k)
				dostuff(j-k)
			}
		}
		else
		{
			for (int k = 0; k < 9; k++)
			{
				dostuff(i-k)
				dostuff(i+k)
				dostuff(j-k)
				dostuff(j+k)
			}
		}
		actor->angle = testang;

#undef dostuff
	}

	an = actor->angle >> ANGLETOFINESHIFT;
	actor->momx = FixedMul (SKULLSPEED, finecosine(an));
	actor->momy = FixedMul (SKULLSPEED, finesine(an));
	dist = dist / SKULLSPEED;
	if (dist < 1)
		dist = 1;
	actor->momz = (dest->z+(dest->theight<<(FRACBITS-1)) - actor->z) / dist;

	actor->momz = 0; // Horizontal only
}

void A_Pain (mobj_t *actor, int16_t var1, int16_t var2)
{
	const mobjinfo_t* ainfo = &mobjinfo[actor->type];
	if (ainfo->painsound)
		S_StartSound (actor, ainfo->painsound);

	actor->flags2 &= ~MF2_FIRING;
}

void A_Fall (mobj_t *actor, int16_t var1, int16_t var2)
{
/* actor is on ground, it can be walked over */
	actor->flags &= ~MF_SOLID;

	// Also want to set these
	P_UnsetThingPosition(actor);
	actor->flags |= MF_NOBLOCKMAP|MF_NOCLIP|MF_NOGRAVITY;
	P_SetThingPosition(actor);

	if (var1)
		actor->extradata = var1;
}


/*
================
=
= A_Explode
=
================
*/

void A_Explode (mobj_t *thingy, int16_t var1, int16_t var2)
{
	P_RadiusAttack ( thingy, thingy->target, 128 );
}

//
// A_BossScream
//
// Spawns explosions and plays appropriate sounds around the defeated boss.
//
// var1: If nonzero, will spawn S_FRET at this height
// var2 = Object to spawn, if not specified, uses MT_SONIC3KBOSSEXPLODE
//
void A_BossScream(mobj_t *actor, int16_t var1, int16_t var2)
{
	const mobjtype_t explodetype = var2 > 0 ? var2 : MT_SONIC3KBOSSEXPLODE;

	const angle_t fa = ((leveltime*(ANG45/9))>>ANGLETOFINESHIFT) & FINEMASK;

	const fixed_t x = actor->x + FixedMul(finecosine(fa),mobjinfo[actor->type].radius);
	const fixed_t y = actor->y + FixedMul(finesine(fa),mobjinfo[actor->type].radius);
	const fixed_t z = actor->z + (P_Random()<<(FRACBITS-2)) - 8*FRACUNIT;

	mobj_t *mo = P_SpawnMobj(x, y, z, explodetype);

	if (mobjinfo[actor->type].deathsound)
		S_StartSound(actor, mobjinfo[actor->type].deathsound);

	if (var1 > 0)
	{
		mo = P_SpawnMobj(actor->x, actor->y, actor->z + (var1 << FRACBITS), MT_GHOST);
		mo->reactiontime = 2;
		P_SetMobjState(mo, S_FRET);
	}
}

/*
================
=
= A_BossDeath
=
= Possibly trigger special effects
================
*/

void P_DoBossVictory(mobj_t *mo)
{
	// Trigger the egg capsule (if it exists)
	if (gamemapinfo.afterBossMusic)
		S_StartSong(gamemapinfo.afterBossMusic, 1, gamemapinfo.mapNumber);

	VINT outerNum = P_FindSectorWithTag(254, -1);
	VINT innerNum = P_FindSectorWithTag(255, -1);

	if (outerNum == -1 || innerNum == -1)
		return;

	sector_t *outer = &sectors[outerNum];
	sector_t *inner = &sectors[innerNum];

	inner->floorheight += 16*FRACUNIT; // OK to just insta-set this
	inner->floorpic = R_FlatNumForName("YELFLR");
	outer->floorpic = R_FlatNumForName("TRAPFLR");
	outer->heightsec = -1;
	inner->heightsec = -1;

	// Move the outer
	floormove_t *floor = Z_Malloc (sizeof(*floor), PU_LEVSPEC);
	P_AddThinker (&floor->thinker);
	outer->specialdata = floor;
	floor->thinker.function = T_MoveFloor;
	floor->type = eggCapsuleOuter;
	floor->crush = false;
	floor->direction = 1;
	floor->sector = outer;
	floor->speed = 2*FRACUNIT;
	floor->floordestheight = 
		(outer->floorheight>>FRACBITS) + 128;

	// Move the inner
	floor = Z_Malloc (sizeof(*floor), PU_LEVSPEC);
	P_AddThinker (&floor->thinker);
	inner->specialdata = floor;
	floor->thinker.function = T_MoveFloor;
	floor->type = eggCapsuleInner;
	floor->crush = false;
	floor->direction = 1;
	floor->sector = inner;
	floor->speed = 2*FRACUNIT;
	floor->floordestheight = 
		(inner->floorheight>>FRACBITS) + 128;
}

//
// A_BossJetFume
//
// Description: Spawns jet fumes/other attachment miscellany for the boss. To only be used when he is spawned.
//
// var1:
//		0 - Triple jet fume pattern
// var2 = unused
//
void A_BossJetFume(mobj_t *actor, int16_t var1, int16_t var2)
{
	mobj_t *filler;

	if (var1 == 0) // Boss1 jet fumes
	{
		fixed_t jetx = actor->x;
		fixed_t jety = actor->y;
		fixed_t jetz = actor->z;

		// One
		P_ThrustValues(actor->angle, -64*FRACUNIT, &jetx, &jety);
		jetz += 38*FRACUNIT;

		filler = P_SpawnMobj(jetx, jety, jetz, MT_JETFUME1);
		filler->target = actor;
		filler->movecount = 1; // This tells it which one it is

		// Two
		jetz = actor->z + 12*FRACUNIT;
		jetx = actor->x;
		jety = actor->y;
		P_ThrustValues(actor->angle-ANG90, 24*FRACUNIT, &jetx, &jety);
		filler = P_SpawnMobj(jetx, jety, jetz, MT_JETFUME1);
		filler->target = actor;
		filler->movecount = 2; // This tells it which one it is

		// Three
		jetx = actor->x;
		jety = actor->y;
		P_ThrustValues(actor->angle+ANG90, 24*FRACUNIT, &jetx, &jety);
		filler = P_SpawnMobj(jetx, jety, jetz, MT_JETFUME1);
		filler->target = actor;
		filler->movecount = 3; // This tells it which one it is
	}

	actor->flags2 |= MF2_SPAWNEDJETS;
}

static void P_SpawnBoss1Junk(mobj_t *mo)
{
	mobj_t *mo2 = P_SpawnMobj(
		mo->x + P_ReturnThrustX(mo->angle - ANG90, 32<<FRACBITS),
		mo->y + P_ReturnThrustY(mo->angle - ANG90, 32<<FRACBITS),
		mo->z + (32<<FRACBITS), MT_BOSSJUNK);

	mo2->angle = mo->angle;
	P_InstaThrust(mo2, mo2->angle - ANG90, 4*FRACUNIT);
	P_SetObjectMomZ(mo2, 4*FRACUNIT, false);
	P_SetMobjState(mo2, S_BOSSEGLZ1);

	mo2 = P_SpawnMobj(
		mo->x + P_ReturnThrustX(mo->angle + ANG90, 32<<FRACBITS),
		mo->y + P_ReturnThrustY(mo->angle + ANG90, 32<<FRACBITS),
		mo->z + (32<<FRACBITS), MT_BOSSJUNK);

	mo2->angle = mo->angle;
	P_InstaThrust(mo2, mo2->angle + ANG90, 4*FRACUNIT);
	P_SetObjectMomZ(mo2, 4*FRACUNIT, false);
	P_SetMobjState(mo2, S_BOSSEGLZ2);
}

angle_t InvAngle(angle_t a)
{
	return (0xffffffff-a) + 1;
}

static void P_DoBossDefaultDeath(mobj_t *mo)
{
	// Stop exploding and prepare to run.
	P_SetMobjState(mo, mobjinfo[mo->type].xdeathstate);
	mo->target = P_FindFirstMobjOfType(MT_BOSSFLYPOINT);

	P_UnsetThingPosition(mo);
	mo->flags |= MF_NOBLOCKMAP|MF_NOGRAVITY|MF_NOCLIP;
	mo->flags2 |= MF2_FLOAT;
	P_SetThingPosition(mo);

	mo->movedir = 0;
	mo->extradata = 2*TICRATE;
	mo->flags2 |= MF2_BOSSFLEE;
	mo->momz = P_MobjFlip(mo)*(1 << (FRACBITS-1));
}

void A_BossDeath (mobj_t *mo, int16_t var1, int16_t var2)
{
	int 		i;

    // make sure there is a player alive for victory
    for (i=0 ; i<MAXPLAYERS ; i++)
		if (playeringame[i] && players[i].health > 0)
			break;
    if (i == MAXPLAYERS)
		return;

	P_DoBossVictory(mo);

	switch (mo->type)
	{
		case MT_EGGMOBILE:
			P_SpawnBoss1Junk(mo);
			break;
		default:
			break;
	}

	P_DoBossDefaultDeath(mo);
}

void A_FishJump(mobj_t *mo, int16_t var1, int16_t var2)
{
	fixed_t watertop = mo->floorz;

	if (SS_SECTOR(mo->isubsector)->heightsec >= 0)
		watertop = GetWatertopMo(mo) - (64<<FRACBITS);

	if ((mo->z <= mo->floorz) || (mo->z <= watertop))
	{
		fixed_t jumpval;

		if (mo->angle)
			jumpval = (mo->angle / ANGLE_1)<<(FRACBITS-2);
		else
			jumpval = 16 << FRACBITS;

		jumpval = FixedMul(jumpval, FixedDiv(30 << FRACBITS, 35 << FRACBITS));

		mo->momz = jumpval;
		P_SetMobjState(mo, mobjinfo[mo->type].seestate);
	}

	if (mo->momz < 0
		&& (mo->state < mobjinfo[mo->type].meleestate || mo->state > mobjinfo[mo->type].xdeathstate))
		P_SetMobjState(mo, mobjinfo[mo->type].meleestate);
}

void A_MonitorPop(mobj_t *actor, int16_t var1, int16_t var2)
{
	mobjtype_t iconItem = 0;
	mobj_t *newmobj;

	// Spawn the "pop" explosion.
	if (mobjinfo[actor->type].deathsound)
		S_StartSound(actor, mobjinfo[actor->type].deathsound);
	P_SpawnMobj(actor->x, actor->y, actor->z + (actor->theight << FRACBITS)/4, MT_EXPLODE);

	// We're dead now. De-solidify.
	actor->health = 0;
	P_UnsetThingPosition(actor);
	actor->flags &= ~MF_SOLID;
	actor->flags2 &= ~MF2_SHOOTABLE;
	actor->flags |= MF_NOCLIP;
	P_SetThingPosition(actor);

	iconItem = mobjinfo[actor->type].damage;

	if (iconItem == 0)
	{
//		CONS_Printf("A_MonitorPop(): 'damage' field missing powerup item definition.\n");
		return;
	}

	newmobj = P_SpawnMobj(actor->x, actor->y, actor->z + 13*FRACUNIT, iconItem);
	newmobj->target = players[0].mo; // TODO: Not multiplayer compatible, but don't care right now
	//actor->target; // transfer the target
}

// Having one function for all box awarding cuts down ROM size
void A_AwardBox(mobj_t *actor, int16_t var1, int16_t var2)
{
	player_t *player;
	mobj_t *orb;

	if (!actor->target || !actor->target->player)
	{
//		CONS_Printf("A_AwardBox(): Monitor has no target.\n");
		return;
	}

	player = &players[actor->target->player - 1];

	switch(actor->type)
	{
		case MT_RING_ICON:
			P_GivePlayerRings(player, mobjinfo[actor->type].reactiontime);
			break;
		case MT_ELEMENTAL_ICON:
			if (player->powers[pw_underwater] <= 12*TICRATE + 1)
				P_RestoreMusic(player);
		case MT_ARMAGEDDON_ICON:
		case MT_ATTRACT_ICON:
		case MT_FORCE_ICON:
		case MT_WHIRLWIND_ICON:
			orb = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, mobjinfo[actor->type].painchance);
			orb->target = player->mo;
			player->shield = mobjinfo[orb->type].painchance;
			break;
		case MT_1UP_ICON:
			player->lives++;
			S_StopSong();
			S_StartSong(gameinfo.xtlifeMus, 0, cdtrack_xtlife);
			player->powers[pw_extralife] = EXTRALIFETICS + 1;
			break;
		case MT_INVULN_ICON:
			player->powers[pw_invulnerability] = INVULNTICS + 1;
			S_StopSong();
			S_StartSong(gameinfo.invincMus, 0, cdtrack_invincibility);
			break;
		case MT_SNEAKERS_ICON:
			player->powers[pw_sneakers] = SNEAKERTICS + 1;
			S_StopSong();
			S_StartSong(gameinfo.sneakerMus, 0, cdtrack_sneakers);
			break;
		default:
			// Dunno what kind of monitor this is, but we fail gracefully.
			break;
	}

	if (mobjinfo[actor->type].seesound)
		S_StartSound(player->mo, mobjinfo[actor->type].seesound);
}

void A_FlickyCheck(mobj_t *actor, int16_t var1, int16_t var2)
{
	if (actor->target && actor->z <= actor->floorz)
	{
		P_SetMobjState(actor, mobjinfo[actor->type].seestate);
		actor->reactiontime = P_Random();
		if (actor->reactiontime < 90)
			actor->reactiontime = 90;
	}
}

void A_FlickyFly(mobj_t *actor, int16_t var1, int16_t var2)
{
	if (actor->z <= actor->floorz)
	{
		A_FaceTarget(actor, 0, 0);
		actor->momz = mobjinfo[actor->type].mass << FRACBITS;

		if (mobjinfo[actor->type].painchance != 0)
		{
			P_InstaThrust(actor, actor->angle, mobjinfo[actor->type].speed);
			P_SetMobjState(actor, mobjinfo[actor->type].meleestate);
		}
	}
	
	if (mobjinfo[actor->type].painchance == 0)
		P_InstaThrust(actor, actor->angle, mobjinfo[actor->type].speed);

	actor->reactiontime--;
	if (actor->reactiontime == 0)
		actor->latecall = LC_REMOVE_MOBJ;
}

void A_BubbleRise(mobj_t *actor, int16_t var1, int16_t var2)
{
	if (actor->type == MT_EXTRALARGEBUBBLE)
		actor->momz = FixedDiv(6*FRACUNIT, 5*FRACUNIT);
	else
		actor->momz += FRACUNIT / 32;

	if (P_Random() < 32)
		P_InstaThrust(actor, P_Random() & 1 ? actor->angle + ANG90 : actor->angle,
			(P_Random() & 1) ? FRACUNIT / 2 : -FRACUNIT/2);
	else if (P_Random() < 32)
		P_InstaThrust(actor, P_Random() & 1 ? actor->angle - ANG90 : actor->angle - ANG180,
			(P_Random() & 1) ? FRACUNIT/2 : -FRACUNIT/2);

	if (sectors[subsectors[actor->isubsector].isector].heightsec < 0
		|| actor->z + (actor->theight << (FRACBITS-1)) > GetWatertopMo(actor))
		actor->latecall = LC_REMOVE_MOBJ;
}

// Boss 1 Stuff
void A_FocusTarget(mobj_t *actor, int16_t var1, int16_t var2)
{
	if (actor->target)
	{
		fixed_t speed = mobjinfo[actor->type].speed;
		angle_t hangle = actor->angle;
		angle_t vangle = ANG90;

		actor->momx -= actor->momx>>4, actor->momy -= actor->momy>>4, actor->momz -= actor->momz>>4;
		actor->momz += FixedMul(finecosine(vangle>>ANGLETOFINESHIFT), speed);
		actor->momx += FixedMul(finesine(vangle>>ANGLETOFINESHIFT), FixedMul(finecosine(hangle>>ANGLETOFINESHIFT), speed));
		actor->momy += FixedMul(finesine(vangle>>ANGLETOFINESHIFT), FixedMul(finesine(hangle>>ANGLETOFINESHIFT), speed));
	}
}

// mobj->target getting set to NULL is our hint that the mobj was removed.
boolean P_RailThinker(mobj_t *mobj)
{
	fixed_t x, y, z;
	x = mobj->x, y = mobj->y, z = mobj->z;

	if (mobj->momx || mobj->momy)
	{
		P_XYMovement(mobj);
		if (mobj->target == NULL)
			return true; // was removed
	}

	if (mobj->momz)
	{
		P_ZMovement(mobj);
		if (mobj->target == NULL)
			return true; // was removed
	}

	return mobj->target == NULL || (x == mobj->x && y == mobj->y && z == mobj->z);
}

void A_Boss1Chase(mobj_t *actor, int16_t var1, int16_t var2)
{
	int delta;

	if (leveltime < 5)
		actor->movecount = 2*TICRATE;

	if (actor->reactiontime)
		actor->reactiontime--;

	if (actor->z < actor->floorz+33*FRACUNIT)
		actor->z = actor->floorz+33*FRACUNIT;

	// turn towards movement direction if not there yet
	if (actor->movedir < NUMDIRS)
	{
		actor->angle &= (7<<29);
		delta = actor->angle - (actor->movedir << 29);

		if (delta > 0)
			actor->angle -= ANG45;
		else if (delta < 0)
			actor->angle += ANG45;
	}

	// do not attack twice in a row
	if (actor->flags2 & MF2_JUSTATTACKED)
	{
		actor->flags2 &= ~MF2_JUSTATTACKED;
		P_NewChaseDir(actor);
		return;
	}

	if (actor->movecount)
		goto nomissile;

	if (actor->target && P_AproxDistance(actor->x - actor->target->x, actor->y - actor->target->y) > 768*FRACUNIT)
		goto nomissile;

	if (actor->reactiontime <= 0)
	{
		if (actor->health > mobjinfo[actor->type].damage)
		{
			if (P_Random() & 1)
				P_SetMobjState(actor, mobjinfo[actor->type].missilestate);
			else
				P_SetMobjState(actor, mobjinfo[actor->type].meleestate);
		}
		else
			P_SetMobjState(actor, mobjinfo[actor->type].mass);

		actor->flags2 |= MF2_JUSTATTACKED;
		actor->reactiontime = 2*TICRATE;
		return;
	}

	// ?
nomissile:
	// possibly choose another target
	if ((splitscreen || netgame) && P_Random() < 2)
	{
		if (P_LookForPlayers(actor, 0, true, true))
			return; // got a new target
	}

	// chase towards player
	if (--actor->movecount < 0 || !P_Move(actor, mobjinfo[actor->type].speed))
		P_NewChaseDir(actor);
}

void A_Boss1Laser(mobj_t *actor, int16_t var1, int16_t var2)
{
	fixed_t x, y, z, floorz, speed;
	int upperend = (var2>>8);
	var2 &= 0xff;
	int i;
	angle_t angle;
	mobj_t *point;
	int dur;

	if (!actor->target)
		return;

	if (states[actor->state].tics > 1)
		dur = actor->tics;
	else
	{
		if ((upperend & 1) && (actor->extradata > 1))
			actor->extradata--;

		dur = actor->extradata;
	}

	switch (var2)
	{
		case 0:
			x = actor->x + P_ReturnThrustX(actor->angle+ANG90, 44*FRACUNIT);
			y = actor->y + P_ReturnThrustY(actor->angle+ANG90, 44*FRACUNIT);
			z = actor->z + 56*FRACUNIT;
			break;
		case 1:
			x = actor->x + P_ReturnThrustX(actor->angle-ANG90, 44*FRACUNIT);
			y = actor->y + P_ReturnThrustY(actor->angle-ANG90, 44*FRACUNIT);
			z = actor->z + 56*FRACUNIT;
			break;
		case 2:
			A_Boss1Laser(actor, var1, 3); // middle laser
			A_Boss1Laser(actor, var1, 0); // left laser
			A_Boss1Laser(actor, var1, 1); // right laser
			return;
			break;
		case 3:
			x = actor->x + P_ReturnThrustX(actor->angle, 42*FRACUNIT);
			y = actor->y + P_ReturnThrustY(actor->angle, 42*FRACUNIT);
			z = actor->z + (actor->theight << (FRACBITS-1));
			break;
		default:
			x = actor->x;
			y = actor->y;
			z = actor->z + (actor->theight << (FRACBITS-1));
			break;
	}

	if (!(actor->flags2 & MF2_FIRING) && dur > 1)
	{
		actor->angle = R_PointToAngle2(x, y, actor->target->x, actor->target->y);
		if (mobjinfo[var1].seesound)
			S_StartSound(actor, mobjinfo[var1].seesound);

		point = P_SpawnMobj(x + P_ReturnThrustX(actor->angle, mobjinfo[actor->type].radius), y + P_ReturnThrustY(actor->angle, mobjinfo[actor->type].radius), actor->z - (actor->theight << (FRACBITS-1)) / 2, MT_EGGMOBILE_TARGET);
		point->angle = actor->angle;
		point->reactiontime = dur+1;
		point->target = actor->target;
		actor->target = point;
	}

	angle = R_PointToAngle2(z + (mobjinfo[var1].height>>1), 0, actor->target->z, P_AproxDistance(x - actor->target->x, y - actor->target->y));

	point = P_SpawnMobj(x, y, z, var1);

	const int iterations = 24;
	point->target = actor;
	point->angle = actor->angle;
	speed = mobjinfo[point->type].radius + (mobjinfo[point->type].radius / 2);
	point->momz = FixedMul(finecosine(angle>>ANGLETOFINESHIFT), speed);
	point->momx = FixedMul(finesine(angle>>ANGLETOFINESHIFT), FixedMul(finecosine(point->angle>>ANGLETOFINESHIFT), speed));
	point->momy = FixedMul(finesine(angle>>ANGLETOFINESHIFT), FixedMul(finesine(point->angle>>ANGLETOFINESHIFT), speed));

	const mobjinfo_t *pointInfo = &mobjinfo[point->type];
	for (i = 0; i < iterations; i++)
	{
		mobj_t *mo = P_SpawnMobjNoSector(point->x, point->y, point->z, point->type);
		P_SetThingPosition2(mo, point->isubsector);
		mo->floorz = point->floorz;
		mo->ceilingz = point->ceilingz;
		mo->z = point->z;
		mo->target = actor;

		mo->angle = point->angle;
//		P_UnsetThingPosition(mo);
//		mo->flags = MF_NOCLIP|MF_NOGRAVITY;
//		P_SetThingPosition(mo);

		if ((dur & 1) && pointInfo->missilestate)
			P_SetMobjState(mo, pointInfo->missilestate);

		x = point->x, y = point->y, z = point->z;
		if (P_RailThinker(point))
			break;
	}

	x += point->momx;
	y += point->momy;
	const sector_t *pointSector = SS_SECTOR(R_PointInSubsector2(x, y));
	floorz = pointSector->floorheight;
	if (z - floorz < (mobjinfo[MT_EGGMOBILE_FIRE].height>>1) && (dur & 1))
	{
		point = P_SpawnMobj(x, y, floorz, MT_EGGMOBILE_FIRE);

		point->angle = actor->angle;
		point->target = actor;

		if (pointSector->heightsec >= 0 && point->z <= GetWatertopSec(pointSector))
		{
//			for (i = 0; i < 2; i++)
			{
				mobj_t *steam = P_SpawnMobj(x, y, GetWatertopSec(pointSector) - mobjinfo[MT_DUST].height/2, MT_DUST);
				if (mobjinfo[point->type].painsound)
					S_StartSound(steam, mobjinfo[point->type].painsound);
			}
		}
		else
		{
//			fixed_t distx = P_ReturnThrustX(point, point->angle, mobjinfo[point->type].radius);
//			fixed_t disty = P_ReturnThrustY(point, point->angle, mobjinfo[point->type].radius);
//			if (P_TryMove(point, point->x + distx, point->y + disty, false) // prevents the sprite from clipping into the wall or dangling off ledges
//				&& P_TryMove(point, point->x - 2*distx, point->y - 2*disty, false)
//				&& P_TryMove(point, point->x + distx, point->y + disty, false))
			{
				if (mobjinfo[point->type].seesound)
					S_StartSound(point, mobjinfo[point->type].seesound);
			}
//			else
//				P_RemoveMobj(point);
		}
	}

	if (dur > 1)
		actor->flags2 |= MF2_FIRING;
	else
		actor->flags2 &= ~MF2_FIRING;
}

static void A_GooSpray(mobj_t *actor, int speedvar)
{
	if (leveltime % (speedvar*15/10)-1 == 0)
	{
		const fixed_t ns = 3 * FRACUNIT;
		mobj_t *goop;
		fixed_t fz = actor->z+(actor->theight<<FRACBITS)+56*FRACUNIT;

		const angle_t fa = P_Random() << 24;

		goop = P_SpawnMobj(actor->x, actor->y, fz, mobjinfo[actor->type].painchance);
		P_ThrustValues(fa, ns, &goop->momx, &goop->momy);
		goop->momz = 4*FRACUNIT;
		goop->reactiontime = 255;
//		goop->reactiontime = 30+(P_Random()/32);

		S_StartSound(actor, mobjinfo[actor->type].attacksound);

		if (P_Random() & 1)
		{
			goop->momx *= 2;
			goop->momy *= 2;
		}
		else if (P_Random() > 128)
		{
			goop->momx *= 3;
			goop->momy *= 3;
		}

		actor->flags2 |= MF2_JUSTATTACKED;
	}
}

void A_Boss2Chase(mobj_t *actor, int16_t var1, int16_t var2)
{
	const mobjinfo_t *aInfo = &mobjinfo[actor->type];

	if (actor->health <= 0)
		return;

	// When reactiontime hits zero, he will go the other way
	if (--actor->reactiontime == 0)
	{
		actor->reactiontime = P_Random();
		if (actor->reactiontime < 2*TICRATE) // 2-second floor
			actor->reactiontime = 2*TICRATE;

		actor->movecount = -actor->movecount;
	}

	actor->target = P_FindFirstMobjOfType(MT_AXIS);

	if (actor->movecount >= 1)
		actor->movecount = 1;
	else
		actor->movecount = -1;
	
	const int speedvar = actor->health;

	if (!actor->target)
		return;

	const fixed_t radius = mobjinfo[actor->target->type].radius;
	actor->target->angle += FixedDiv(FixedMul(ANG45/15,actor->movecount*actor->health), speedvar);

	P_UnsetThingPosition(actor);
	{
		fixed_t fx = 0;
		fixed_t fy = 0;
		P_ThrustValues(actor->target->angle, radius, &fx, &fy);
		actor->angle = R_PointToAngle2(actor->x, actor->y, actor->target->x + fx, actor->target->y + fy);
		actor->x = actor->target->x + fx;
		actor->y = actor->target->y + fy;
	}
	P_SetThingPosition(actor);

	// Spray goo once every second
	A_GooSpray(actor, speedvar);
}

void A_Boss2Pogo(mobj_t *actor, int16_t var1, int16_t var2)
{
	const mobjinfo_t *aInfo = &mobjinfo[actor->type];

	if (actor->z <= actor->floorz + 8*FRACUNIT && actor->momz <= 0)
	{
		if (actor->state != S_EGGMOBILE2_POGO1)
			P_SetMobjState(actor, S_EGGMOBILE2_POGO1);
		// Pogo Mode
	}
	else if (actor->momz < 0 && actor->reactiontime)
	{
		const fixed_t ns = 3 * FRACUNIT;
		mobj_t *goop;
		fixed_t fz = actor->z+(actor->theight << FRACBITS)+24*FRACUNIT;
		angle_t fa;

		// spray in all 8 directions!
		for (int i = 0; i < 8; i++)
		{
			actor->movedir++;
			actor->movedir %= NUMDIRS;
			fa = (actor->movedir*FINEANGLES/8) & FINEMASK;

			goop = P_SpawnMobj(actor->x, actor->y, fz, aInfo->painchance);
			goop->momx = FixedMul(finecosine(fa),ns);
			goop->momy = FixedMul(finesine(fa),ns);
			goop->momz = 4*FRACUNIT;

			goop->reactiontime = 30+(P_Random()/32);
		}
		actor->reactiontime = 0; // we already shot goop, so don't do it again!
		if (aInfo->attacksound)
			S_StartSound(actor, aInfo->attacksound);
		actor->flags2 |= MF2_JUSTATTACKED;
	}
}

void A_Boss2PogoTarget(mobj_t *actor, int16_t var1, int16_t var2)
{

}

void A_Boss2TakeDamage(mobj_t *actor, int16_t var1, int16_t var2)
{

}

void A_PrepareRepeat(mobj_t *actor, int16_t var1, int16_t var2)
{
	actor->extradata = var1;
}

void A_Repeat(mobj_t *actor, int16_t var1, int16_t var2)
{
	if (var1 && (!actor->extradata || actor->extradata > var1))
		actor->extradata = var1;

	actor->extradata--;
	if (actor->extradata > 0)
		P_SetMobjState(actor, var2);
}

//
// var1 = height
// var2 = unused
//
void A_ChangeHeight(mobj_t *actor, int16_t var1, int16_t var2)
{
	P_UnsetThingPosition(actor);
	actor->theight = var1;
	P_SetThingPosition(actor);
}

void A_UnidusBall(mobj_t *actor, int16_t var1, int16_t var2)
{
	actor->angle += ANG180 / 16;

	if (actor->movecount)
	{
		if (P_AproxDistance(actor->momx, actor->momy) < (mobjinfo[actor->type].damage << (FRACBITS-1)))
			P_ExplodeMissile(actor);
		return;
	}

	if (!actor->target || !actor->target->health)
	{
		actor->latecall = LC_REMOVE_MOBJ;
		return;
	}

	P_UnsetThingPosition(actor);
	{
		const angle_t angle = actor->movedir + ANGLE_1 * (mobjinfo[actor->type].speed*(leveltime%360));
		const uint16_t fa = angle>>ANGLETOFINESHIFT;

		actor->x = actor->target->x + FixedMul(finecosine(fa),actor->threshold);
		actor->y = actor->target->y + FixedMul(  finesine(fa),actor->threshold);
		actor->z = actor->target->z + (actor->target->theight << (FRACBITS-1)) - (actor->theight << (FRACBITS-1));
	}
	P_SetThingPosition(actor);
}

// Function: A_BubbleSpawn
//
// Description: Spawns a randomly sized bubble from the object's location. Only works underwater.
//
void A_BubbleSpawn(mobj_t *actor, int16_t var1, int16_t var2)
{
	if (SS_SECTOR(actor->isubsector)->heightsec >= 0
		&& actor->z < GetWatertopMo(actor) - 32*FRACUNIT)
	{
		int i;
		uint8_t prandom;
		actor->flags2 &= ~MF2_DONTDRAW;

		// Quick! Look through players!
		// Don't spawn bubbles unless a player is relatively close by (var1).
		for (i = 0; i < MAXPLAYERS; ++i)
			if (playeringame[i] && players[i].mo
			 && P_AproxDistance(actor->x - players[i].mo->x, actor->y - players[i].mo->y) < (1024<<FRACBITS))
				break; // Stop looking.
		if (i == MAXPLAYERS)
			return; // don't make bubble!

		prandom = P_Random();

		if (leveltime % (3*TICRATE) < 8)
			P_SpawnMobj(actor->x, actor->y, actor->z + (actor->theight << (FRACBITS-1)), MT_EXTRALARGEBUBBLE);
		else if (prandom > 128)
			P_SpawnMobj(actor->x, actor->y, actor->z + (actor->theight << (FRACBITS-1)), MT_SMALLBUBBLE);
		else if (prandom < 128 && prandom > 96)
			P_SpawnMobj(actor->x, actor->y, actor->z + (actor->theight << (FRACBITS-1)), MT_MEDIUMBUBBLE);
	}
	else
	{
		actor->flags2 |= MF2_DONTDRAW;
		return;
	}
}

// Function: A_SignSpin
//
// Description: Spawns MT_SPARK around the signpost.
//
void A_SignSpin(mobj_t *actor, int16_t var1, int16_t var2)
{
	fixed_t radius = mobjinfo[actor->type].radius;

	actor->angle += ANG45 / 9;
	for (int i = -1; i < 2; i += 2)
	{
		P_SpawnMobj(
			actor->x + P_ReturnThrustX(actor->angle, i * radius),
			actor->y + P_ReturnThrustY(actor->angle, i * radius),
			actor->z + (actor->theight << FRACBITS),
			mobjinfo[actor->type].painchance);
	}
}

void A_SteamBurst(mobj_t *actor, int16_t var1, int16_t var2)
{
	const mobjinfo_t *info = &mobjinfo[actor->type];

	if (!(P_Random() & 7))
	{
		if (info->deathsound)
			S_StartSound(actor, info->deathsound); // Hiss!
	}
	else
	{
		if (info->painsound)
			S_StartSound(actor, info->painsound);
	}

	for (int i = 0; i < MAXPLAYERS; i++)
	{
		if (playeringame[i])
		{
			player_t *player = &players[i];
			const mobjinfo_t *playerInfo = &mobjinfo[player->mo->type];

			if (player->mo->z > actor->z + 8*FRACUNIT
				|| player->mo->z < actor->z)
				continue;

			if (P_AproxDistance(player->mo->x - actor->x, player->mo->y - actor->y) > info->radius + playerInfo->radius)
				continue;

			if (player && player->mo->state == playerInfo->painstate) // can't use gas jets when player is in pain!
				return;

			fixed_t speed = info->mass << FRACBITS; // gas jets use this for the vertical thrust
			int8_t flipval = 1;//P_MobjFlip(special); // virtually everything here centers around the thruster's gravity, not the object's!

			player->mo->momz = flipval * speed;

			P_ResetPlayer(player);
			P_SetMobjState(player->mo, S_PLAY_FALL1);
		}
	}
}

// 'Lob' the arrow in a parabola towards the target
static void P_ParabolicMove(mobj_t *actor, fixed_t x, fixed_t y, fixed_t z, fixed_t speed)
{
	fixed_t dh;

	x -= actor->x;
	y -= actor->y;
	z -= actor->z;

	dh = P_AproxDistance(x, y);

	actor->momx = FixedMul(FixedDiv(x, dh), speed);
	actor->momy = FixedMul(FixedDiv(y, dh), speed);

	fixed_t gravity = GRAVITY / 2;
	if (!gravity)
		return;

	dh = FixedDiv(FixedMul(dh, gravity), speed);
	actor->momz = (dh>>1) + FixedDiv(z, dh<<1);
}

// Function: A_HoodFire
//
// Description: Firing Robo-Hood
//
// var1 = object type to fire
// var2 = unused
//
void A_HoodFire(mobj_t *actor, int16_t var1, int16_t var2)
{
	mobj_t *arrow;

	const mobjinfo_t *aInfo = &mobjinfo[actor->type];

	// Check target first.
	if (!actor->target)
	{
		actor->reactiontime = aInfo->reactiontime;
		P_SetMobjState(actor, aInfo->spawnstate);
		return;
	}

	A_FaceTarget(actor, 0, 0);

	if (!(arrow = P_SpawnMissile(actor, actor->target, (mobjtype_t)var1)))
		return;

	const mobjinfo_t *arrowInfo = &mobjinfo[arrow->type];

	// Set a parabolic trajectory for the arrow.
	P_ParabolicMove(arrow, actor->target->x, actor->target->y, actor->target->z, arrowInfo->speed);
}

// Function: A_HoodThink
//
// Description: Thinker for Robo-Hood
//
// var1 = unused
// var2 = unused
//
void A_HoodThink(mobj_t *actor, int16_t var1, int16_t var2)
{
	fixed_t dx, dy, dz, dm;
	const mobjinfo_t *aInfo = &mobjinfo[actor->type];

	// Check target first.
	if (!actor->target)
	{
		actor->reactiontime = aInfo->reactiontime;
		P_SetMobjState(actor, aInfo->spawnstate);
		return;
	}

	dx = (actor->target->x - actor->x), dy = (actor->target->y - actor->y), dz = (actor->target->z - actor->z);
	dm = P_AproxDistance(dx, dy);
	// Target dangerously close to robohood, retreat then.
	if ((dm < 256<<FRACBITS) && (D_abs(dz) < 128<<FRACBITS) && !(actor->flags2 & MF2_SPAWNEDJETS))
	{
		S_StartSound(actor, aInfo->attacksound);
		A_FaceTarget(actor, 0, 0);
		actor->angle = -actor->angle; // Do a 180
		P_SetObjectMomZ(actor, 6*FRACUNIT, false);
		P_InstaThrust(actor, actor->angle, 6*FRACUNIT);
		P_SetMobjState(actor, S_ROBOHOOD_JUMP1);
		return;
	}

	// If target on sight, look at it.
	if (actor->target)
	{
		angle_t dang = R_PointToAngle2(actor->x, actor->y, actor->target->x, actor->target->y);
		if (actor->angle >= ANG180)
			actor->angle = InvAngle(InvAngle(actor->angle)>>1);
		else
			actor->angle >>= 1;

		if (dang >= ANG180)
			dang = InvAngle(InvAngle(dang)>>1);
		else
			dang >>= 1;

		actor->angle += dang;
	}

	// Check whether to do anything.
	if ((--actor->reactiontime) <= 0)
	{
		actor->reactiontime = aInfo->reactiontime;

		// If way too far, don't shoot.
		if ((dm < (3072<<FRACBITS)) && actor->target)
		{
			P_SetMobjState(actor, aInfo->missilestate);
			S_StartSound(actor, aInfo->activesound);
			return;
		}
	}
}

// Function: A_HoodFall
//
// Description: Falling Robo-Hood
//
// var1 = unused
// var2 = unused
//
void A_HoodFall(mobj_t *actor, int16_t var1, int16_t var2)
{
	if (!P_IsObjectOnGround(actor))
		return;

	const mobjinfo_t *aInfo = &mobjinfo[actor->type];

	actor->momx = actor->momy = 0;
	actor->reactiontime = aInfo->reactiontime;
	P_SetMobjState(actor, aInfo->seestate);
}

// Function: A_FaceStabRev
//
// Description: Facestabber rev action
//
// var1 = effective duration
// var2 = effective nextstate
//
void A_FaceStabRev(mobj_t *actor, int16_t var1, int16_t var2)
{/*
	const mobjinfo_t *aInfo = &mobjinfo[actor->type];

	if (!actor->target)
	{
		P_SetMobjState(actor, aInfo->spawnstate);
		return;
	}

	actor->extradata = 0;

	if (!actor->reactiontime)
	{
		actor->reactiontime = var1;
		S_StartSound(actor, aInfo->activesound);
	}
	else
	{
		if ((--actor->reactiontime) == 0)
		{
			S_StartSound(actor, aInfo->attacksound);
			P_SetMobjState(actor, var2);
		}
		else
		{
			P_TryMove(actor, actor->x - P_ReturnThrustX(actor->angle, 2<<FRACBITS), actor->y - P_ReturnThrustY(actor->angle, 2<<FRACBITS), false);
			if (!P_MobjWasRemoved(actor))
				P_FaceStabFlume(actor);
		}
	}*/
}

// Function: A_FaceStabHurl
//
// Description: Facestabber hurl action
//
// var1 = homing strength (recommended strength between 0-8)
// var2 = effective nextstate
//
void A_FaceStabHurl(mobj_t *actor, int16_t var1, int16_t var2)
{/*
	if (actor->target)
	{
		angle_t visang = R_PointToAngle2(actor->x, actor->y, actor->target->x, actor->target->y);
		// Calculate new direction.
		angle_t dirang = actor->angle;
		angle_t diffang = visang - dirang;

		if (var1) // Allow homing?
		{
			if (diffang > ANG180)
			{
				angle_t workang = (int32_t)var1*(InvAngle(diffang)>>5);
				diffang += InvAngle(workang);
			}
			else
				diffang += (int32_t)var1*(diffang>>5);
		}
		diffang += ANG45;

		// Check the sight cone.
		if (diffang < ANG90)
		{
			actor->angle = dirang;
			if (++actor->extravalue2 < 4)
				actor->extravalue2 = 4;
			else if (actor->extravalue2 > 26)
				actor->extravalue2 = 26;

			if (P_TryMove(actor,
				actor->x + P_ReturnThrustX(dirang, UPPER8(actor->extradata)<<FRACBITS),
				actor->y + P_ReturnThrustY(dirang, UPPER8(actor->extradata)<<FRACBITS),
				false))
			{
				// Do the spear damage.
#define NUMSTEPS 3
#define NUMGRADS 5
#define MAXVAL (NUMSTEPS*NUMGRADS)

				int8_t step = (++actor->extravalue1);
				fixed_t basesize = FRACUNIT/MAXVAL;
				mobj_t *hwork = actor;
				int32_t dist = 113;
				fixed_t xo = P_ReturnThrustX(actor->angle, dist*basesize);
				fixed_t yo = P_ReturnThrustY(actor->angle, dist*basesize);

				while (step > 0)
				{
					if (!P_MobjWasRemoved(hwork->hnext))
					{
						hwork = hwork->hnext;
						hwork->angle = actor->angle + ANG90;
						P_SetScale(hwork, FixedSqrt(step*basesize), true);
						hwork->fuse = 2;
						P_MoveOrigin(hwork, actor->x + xo*(15-step), actor->y + yo*(15-step), actor->z + (actor->height - hwork->height)/2 + (P_MobjFlip(actor)*(8<<FRACBITS)));
						if (P_MobjWasRemoved(hwork))
						{
							// if one of the sections are removed, erase the entire damn thing.
							mobj_t *hnext = actor->hnext;
							hwork = actor;
							do
							{
								hnext = hwork->hnext;
								P_RemoveMobj(hwork);
								hwork = hnext;
							}
							while (!P_MobjWasRemoved(hwork));
							return;
						}
					}
					step -= NUMGRADS;
				}

				if (actor->extravalue1 >= MAXVAL)
					actor->extravalue1 -= NUMGRADS;

				P_FaceStabFlume(actor);
				return;
#undef MAXVAL
#undef NUMGRADS
#undef NUMSTEPS
			}
			if (P_MobjWasRemoved(actor))
				return;
		}
	}

	P_SetMobjState(actor, var2);
	if (!P_MobjWasRemoved(actor))
	{
		const mobjinfo_t *aInfo = &mobjinfo[actor->type];
		actor->reactiontime = aInfo->reactiontime;
	}*/
}

// Function: A_FaceStabMiss
//
// Description: Facestabber miss action
//
// var1 = unused
// var2 = effective nextstate
//
void A_FaceStabMiss(mobj_t *actor, int16_t var1, int16_t var2)
{/*
	if (++actor->extravalue1 >= 3)
	{
		actor->extravalue2 -= 2;
		actor->extravalue1 = 0;
		S_StartSound(actor, sfx_s3k_47);
//		P_SharpDust(actor, MT_SPINDUST, actor->angle);
	}

	if (actor->extravalue2 <= 0 || !P_TryMove(actor,
		actor->x + P_ReturnThrustX(actor, actor->angle, actor->extravalue2<<FRACBITS),
		actor->y + P_ReturnThrustY(actor, actor->angle, actor->extravalue2<<FRACBITS),
		false))
	{
		if (P_MobjWasRemoved(actor))
			return;
		actor->extravalue2 = 0;
		P_SetMobjState(actor, var2);
	}*/
}

// Function: A_GuardChase
//
// Description: Modified A_Chase for Egg Guard
//
// var1 = unused
// var2 = unused
//
void A_GuardChase(mobj_t *actor, int16_t var1, int16_t var2)
{
	int32_t delta;
	const mobjinfo_t *aInfo = &mobjinfo[actor->type];

	if (actor->reactiontime)
		actor->reactiontime--;

	if (actor->threshold != 42) // In formation...
	{
		fixed_t speed;
		ptrymove_t tm;

		speed = LOWER8(actor->extradata) << FRACBITS;

		if (actor->flags2 & MF2_SPAWNEDJETS)
			speed <<= 1;

		if (speed
		&& !P_TryMove(&tm, actor,
			actor->x + P_ReturnThrustX(actor->angle, speed),
			actor->y + P_ReturnThrustY(actor->angle, speed))
		&& speed > 0) // can't be the same check as previous so that P_TryMove gets to happen.
		{
			int32_t direction = UPPER8(actor->extradata);

			switch (direction)
			{
				case TMGD_BACK:
				default:
					actor->angle += ANG180;
					break;
				case TMGD_RIGHT:
					actor->angle -= ANG90;
					break;
				case TMGD_LEFT:
					actor->angle += ANG90;
					break;
			}
		}

		if (LOWER8(actor->extradata) < aInfo->speed)
		{
			const uint8_t newvalue = LOWER8(actor->extradata) + 1;
			SETLOWER8(actor->extradata, newvalue);
		}
	}
	else // Break ranks!
	{
		actor->flags2 |= MF2_SHOOTABLE;

		// turn towards movement direction if not there yet
		if (actor->movedir < 8)
		{
			actor->angle &= (7<<29);
			delta = actor->angle - (actor->movedir << 29);

			if (delta > 0)
				actor->angle -= ANG45;
			else if (delta < 0)
				actor->angle += ANG45;
		}

		// chase towards player
		if (--actor->movecount < 0 || !P_Move(actor, (actor->flags2 & MF2_SPAWNEDJETS) ? aInfo->speed * 2 : aInfo->speed))
		{
			P_NewChaseDir(actor);
//			actor->movecount += 5; // Increase tics before change in direction allowed.
		}
	}

	// Shields should think and move on their own.
	// They are spawned immediately after the guard, so it will think
	// next all by itself, no need to call it here.

/*
	// Now that we've moved, its time for our shield to move!
	// Otherwise it'll never act as a proper overlay.
	if (actor->tracer && actor->tracer->state
	&& actor->tracer->state->action.acp1)
	{
		var1 = actor->tracer->state->var1, var2 = actor->tracer->state->var2;
		actor->tracer->state->action.acp1(actor->tracer);
	}*/
}

// Function: A_EggShield
//
// Description: Modified A_Chase for Egg Guard's shield
//
// var1 = unused
// var2 = unused
//
void A_EggShield(mobj_t *actor, int16_t var1, int16_t var2)
{
	fixed_t blockdist;
	fixed_t newx, newy;
	fixed_t movex, movey;
	angle_t angle;
	const mobjinfo_t *aInfo = &mobjinfo[actor->type];

	if (!actor->target || !actor->target->health)
	{
		actor->latecall = LC_REMOVE_MOBJ;
		return;
	}

	newx = actor->target->x + P_ReturnThrustX(actor->target->angle, FRACUNIT);
	newy = actor->target->y + P_ReturnThrustY(actor->target->angle, FRACUNIT);

	movex = newx - actor->x;
	movey = newy - actor->y;

	actor->angle = actor->target->angle;
	actor->z = actor->target->z;

	actor->floorz = actor->target->floorz;
	actor->ceilingz = actor->target->ceilingz;

	if (!movex && !movey)
		return;

	P_UnsetThingPosition(actor);
	actor->x = newx;
	actor->y = newy;
	P_SetThingPosition(actor);

	// Search for players to push
	for (int i = 0; i < MAXPLAYERS; i++)
	{
		if (!playeringame[i])
			continue;

		const player_t *player = &players[i];

		if (!player->mo)
			continue;

		if (player->mo->z > actor->z + (actor->theight << FRACBITS))
			continue;

		if (player->mo->z + (player->mo->theight << FRACBITS) < actor->z)
			continue;

		const mobjinfo_t *pInfo = &mobjinfo[MT_PLAYER];

		blockdist = aInfo->radius + pInfo->radius;

		if (D_abs(actor->x - player->mo->x) >= blockdist || D_abs(actor->y - player->mo->y) >= blockdist)
			continue; // didn't hit it

		angle = R_PointToAngle2(actor->x, actor->y, player->mo->x, player->mo->y) - actor->angle;

		if (angle > ANG90 && angle < ANG270)
			continue;

		// Blocked by the shield
		player->mo->momx += movex;
		player->mo->momy += movey;
		return;
	}
}

void A_EggShieldBroken(mobj_t *actor, int16_t var1, int16_t var2)
{
	mobj_t *guard = actor->target;

	if (!guard || guard->health == 0)
		return;

	// Lost the shield, so get mad!!!
	actor->target = NULL;
	guard->threshold = 42;
	P_SetMobjState(guard, mobjinfo[guard->type].painstate);
	guard->flags2 |= MF2_SHOOTABLE;
}

/*============================================================================= */

/* a move in p_base.c caused a missile to hit another thing or wall */
void L_MissileHit (mobj_t *mo)
{
	int	damage;
	mobj_t	*missilething;
	const mobjinfo_t* moinfo = &mobjinfo[mo->type];

	missilething = (mobj_t *)SPTR_TO_LPTR(mo->extradata);
	if (missilething)
	{
		mo->extradata = 0;
		damage = ((P_Random()&7)+1)* moinfo->damage;
		P_DamageMobj (missilething, mo, mo->target, damage);
	}
	P_ExplodeMissile (mo);
}

/*  */
