/* P_mobj.c */

#include "doomdef.h"
#include "p_local.h"
#include "p_camera.h"
#include "sounds.h"

void G_PlayerReborn (int player);

/*
===============
=
= P_AddMobjToList
=
===============
*/
static void P_AddMobjToList (mobj_t *mobj_, mobj_t *head_)
{
	degenmobj_t *mobj = (void*)mobj_, *head = (void *)head_;
	((degenmobj_t *)head->prev)->next = mobj;
	mobj->next = head;
	mobj->prev = head->prev;
	head->prev = mobj;
}

static void P_RemoveMobjFromCurrList (mobj_t *mobj_)
{
	degenmobj_t *mobj = (void*)mobj_;
	((degenmobj_t *)mobj->next)->prev = mobj->prev;
	((degenmobj_t *)mobj->prev)->next = mobj->next;
}

mobj_t *P_FindFirstMobjOfType(uint16_t type)
{
	for (mobj_t *node = mobjhead.next; node != (void*)&mobjhead; node = node->next)
    {
		if (node->type == type)
			return node;
    }

	return NULL;
}

/*
===============
=
= P_RemoveMobj
=
===============
*/
__attribute((noinline))
void P_RemoveMobj (mobj_t *mobj)
{
/* unlink from sector and block lists */
	P_UnsetThingPosition(mobj);

	if (mobj->flags & MF_RINGMOBJ)
	{
		mobj->flags = MF_RINGMOBJ|MF_NOBLOCKMAP|MF_NOSECTOR; // Remove all other flags
		return;
	}
	else if (mobj->flags & MF_STATIC)
	{
		/* unlink from mobj list */
		P_RemoveMobjFromCurrList(mobj);
		P_AddMobjToList(mobj, (void*)&freestaticmobjhead);
		return;
	}

	mobj->target = NULL;
	mobj->extradata = 0;
	mobj->latecall = LC_INVALID;	/* make sure it doesn't come back to life... */

/* unlink from mobj list */
	P_RemoveMobjFromCurrList(mobj);
/* link to limbo mobj list */
	P_AddMobjToList(mobj, (void*)&limbomobjhead);
}

void P_FreeMobj(mobj_t* mobj)
{
/* unlink from mobj list */
	P_RemoveMobjFromCurrList(mobj);
/* link to limbo mobj list */
	P_AddMobjToList(mobj, (void*)&freemobjhead);
}

void P_Attract(mobj_t *source, mobj_t *dest)
{
	fixed_t dist, ndist, speedmul;
	fixed_t tx = dest->x;
	fixed_t ty = dest->y;
	fixed_t tz = dest->z + (dest->theight << (FRACBITS-1)); // Aim for center

	// change angle
	source->angle = R_PointToAngle2(source->x, source->y, tx, ty);

	// change slope
	dist = P_AproxDistance3D(tx - source->x, ty - source->y, tz - source->z);

	if (dist < 1)
		dist = 1;
	
	if (dist > 32 * FRACUNIT)
		source->flags |= MF_NOCLIP;
	else
		source->flags &= ~MF_NOCLIP;

	speedmul = P_AproxDistance(dest->momx, dest->momy) + (mobjinfo[source->type].speed << FRACBITS);
	source->momx = FixedMul(FixedDiv(tx - source->x, dist), speedmul);
	source->momy = FixedMul(FixedDiv(ty - source->y, dist), speedmul);
	source->momz = FixedMul(FixedDiv(tz - source->z, dist), speedmul);

	// Instead of just unsetting NOCLIP like an idiot, let's check the distance to our target.
	ndist = P_AproxDistance3D(tx - (source->x+source->momx),
											ty - (source->y+source->momy),
											tz - (source->z+source->momz));

	if (ndist > dist) // gone past our target
	{
		// place us on top of them then.
		source->momx = source->momy = source->momz = 0;
		source->z = tz;
		P_SetThingPositionConditionally(source, tx, ty, dest->isubsector);
	}
}

fixed_t GetWatertopSec(const sector_t *sec)
{
	if (sec->heightsec < 0)
		return sec->floorheight - 512*FRACUNIT;

	return I_TO_SEC(sec->heightsec)->ceilingheight;
}

fixed_t GetWatertopMo(const mobj_t *mo)
{
	const sector_t *sec = I_TO_SEC(subsectors[mo->isubsector].isector);
	return GetWatertopSec(sec);
}

void P_SetObjectMomZ(mobj_t *mo, fixed_t value, boolean relative)
{
//	if (player->pflags & PF_VERTICALFLIP)
//		value = -value;

	if (relative)
		mo->momz += value;
	else
		mo->momz = value;
}

boolean P_IsObjectOnGround(mobj_t *mobj)
{
	if (mobj->flags & MF_RINGMOBJ)
		return true;
		
	if (mobj->player)
	{
		const player_t *player = &players[mobj->player - 1];
		if (player->pflags & PF_VERTICALFLIP)
		{
			if (mobj->z + Mobj_GetHeight(mobj) >= mobj->ceilingz)
				return true;
		}
		else if (mobj->z <= mobj->floorz)
			return true;
		else
			return false;
	}

	return mobj->z <= mobj->floorz;
}

int8_t P_MobjFlip(mobj_t *mo)
{
	if (mo->player && players[mo->player-1].pflags & PF_VERTICALFLIP)
		return -1;

	return 1;
}

/*
================
=
= P_SetMobjState
=
= Returns true if the mobj is still present
================
*/

boolean P_SetMobjState (mobj_t *mobj, statenum_t state)
{
	uint16_t changes = 0xf; // Only chain up to 16 states.

	if (mobj->flags & MF_RINGMOBJ)
		return true; // silently fail

	if (mobj->player)
	{
		player_t *player = &players[mobj->player - 1];
		if (P_IsReeling(player) && state != mobjinfo[mobj->type].painstate)
			player->powers[pw_flashing] = FLASHINGTICS-1;
	}

	do {
		const state_t* st;

		if (state == S_NULL)
		{
			mobj->state = S_NULL;
			P_RemoveMobj(mobj);
			return false;
		}

		st = &states[state];
		mobj->state = state;
		mobj->tics = st->tics;

		if (st->action)		/* call action functions when the state is set */
			st->action(mobj, st->var1, st->var2);

		state = st->nextstate;
	} while (!mobj->tics && --changes > 0);

	return true;
}

/* 
=================== 
= 
= P_ExplodeMissile  
=
=================== 
*/ 

void P_ExplodeMissile (mobj_t *mo)
{
	const mobjinfo_t* info = &mobjinfo[mo->type];

	mo->momx = mo->momy = mo->momz = 0;
	P_SetMobjState (mo, mobjinfo[mo->type].deathstate);
	mo->tics -= P_Random()&1;
	if (mo->tics < 1)
		mo->tics = 1;
	mo->flags2 &= ~MF2_MISSILE;
	if (info->deathsound)
		S_StartSound (mo, info->deathsound);
}


/*
===============
=
= P_SpawnMobj
=
===============
*/

mobj_t *P_SpawnMobjNoSector(fixed_t x, fixed_t y, fixed_t z, mobjtype_t type)
{
	mobj_t		*mobj;
	const state_t		*st;
	const mobjinfo_t *info = &mobjinfo[type];

/* try to reuse a previous mobj first */
	if (info->flags & MF_RINGMOBJ)
	{
		if (info->flags & MF_NOBLOCKMAP) // It's scenery
		{
			if (numscenerymobjs >= scenerymobjcount)
				I_Error("No more slots available for a scenery mobj.");
				
			scenerymobj_t *scenerymobj = &scenerymobjlist[numscenerymobjs];
			scenerymobj->type = type;
			scenerymobj->x = x >> FRACBITS;
			scenerymobj->y = y >> FRACBITS;
			scenerymobj->flags = info->flags;

			/* set subsector and/or block links */
			P_SetThingPosition2 ((mobj_t*)scenerymobj, R_PointInSubsector2(x, y));

			numscenerymobjs++;

			return (mobj_t*)scenerymobj;
		}
		else // It's a ring
		{
			if (numringmobjs >= ringmobjcount)
				I_Error("No more slots available for a ring mobj.");

			ringmobj_t *ringmobj = &ringmobjlist[numringmobjs];
			ringmobj->type = type;
			ringmobj->x = x >> FRACBITS;
			ringmobj->y = y >> FRACBITS;
			ringmobj->z = z >> FRACBITS;
			ringmobj->flags = info->flags;

			/* set subsector and/or block links */
			P_SetThingPosition2 ((mobj_t*)ringmobj, R_PointInSubsector2(x, y));

			numringmobjs++;

			return (mobj_t*)ringmobj;
		}
	}
	else if (info->flags & MF_STATIC)
	{
		if (freestaticmobjhead.next != (void *)&freestaticmobjhead)
		{
			mobj = freestaticmobjhead.next;
			P_RemoveMobjFromCurrList(mobj);
		}
		else
		{
			mobj = Z_Malloc (static_mobj_size, PU_LEVEL);
		}
		D_memset (mobj, 0, static_mobj_size);
	}
	else
	{
		if (freemobjhead.next != (void *)&freemobjhead)
		{
			mobj = freemobjhead.next;
			P_RemoveMobjFromCurrList(mobj);
		}
		else
		{
			mobj = Z_Malloc (sizeof(*mobj), PU_LEVEL);
		}
		D_memset (mobj, 0, sizeof (*mobj));
	}

	mobj->type = type;
	mobj->x = x;
	mobj->y = y;
	mobj->theight = info->height >> FRACBITS;
	mobj->flags = info->flags;
	mobj->flags2 = info->flags2;
	mobj->health = info->spawnhealth;

/* do not set the state with P_SetMobjState, because action routines can't */
/* be called yet */
	st = &states[info->spawnstate];

	mobj->state = info->spawnstate;
	mobj->tics = st->tics;

	/* */
	/* link into the mobj list */
	/* */
	P_AddMobjToList(mobj, (void *)&mobjhead);

	return mobj;
}

mobj_t *P_SpawnMobj (fixed_t x, fixed_t y, fixed_t z, mobjtype_t type)
{
	const mobjinfo_t *info = &mobjinfo[type];
	mobj_t *mobj = P_SpawnMobjNoSector(x, y, z, type);

	if (info->flags & MF_RINGMOBJ)
		return mobj;

/* set subsector and/or block links */
	P_SetThingPosition(mobj);

	const sector_t *sec = SS_SECTOR(mobj->isubsector);
	
	mobj->floorz = sec->floorheight;
	mobj->ceilingz = sec->ceilingheight;
	if (z == ONFLOORZ)
		mobj->z = mobj->floorz;
	else if (z == ONCEILINGZ)
		mobj->z = mobj->ceilingz - info->height;
	else
		mobj->z = z;

	mobj->floorz = FloorZAtPos(sec, mobj->z, info->height);
	mobj->ceilingz = CeilingZAtPos(sec, mobj->z, info->height);
	
	return mobj;
}

int thingmem = 0;
/*
================
=
= P_PreSpawnMobjs
=
================
*/
void P_PreSpawnMobjs(int count, int staticcount, int ringcount, int scenerycount)
{
	ringmobjcount = ringcount;
	scenerymobjcount = scenerycount;

	thingmem = count * sizeof(mobj_t) + staticcount * static_mobj_size + ringcount * sizeof(ringmobj_t) + scenerycount * sizeof(scenerymobj_t);

	if (scenerycount > 0)
	{
		scenerymobjlist = Z_Malloc(sizeof(scenerymobj_t)*scenerycount, PU_LEVEL);
		numscenerymobjs = 0;
	}

	if (ringcount > 0)
	{
		ringmobjlist = Z_Malloc(sizeof(ringmobj_t)*ringcount, PU_LEVEL);
		numringmobjs = 0;
	}

	numstaticmobjs = staticcount;
	if (staticcount > 0)
	{
		uint8_t *mobj = Z_Malloc (static_mobj_size*staticcount, PU_LEVEL);
		D_memset(mobj, 0, static_mobj_size*staticcount);
		for (; staticcount > 0; staticcount--) {
			P_AddMobjToList((void *)mobj, (void *)&freestaticmobjhead);
			mobj += static_mobj_size;
		}
	}

	numregmobjs = count;
	if (count > 0)
	{
		mobj_t *mobj = Z_Malloc (sizeof(*mobj)*count, PU_LEVEL);
		D_memset(mobj, 0, (sizeof(*mobj)*count));
		for (; count > 0; count--) {
			P_AddMobjToList(mobj, (void *)&freemobjhead);
			mobj++;
		}
	}
}

/*============================================================================= */


/*
============
=
= P_SpawnPlayer
=
= Called when a player is spawned on the level 
= Most of the player structure stays unchanged between levels
============
*/

void P_SpawnPlayer (mapthing_t *mthing)
{
	player_t	*p;
	fixed_t		x,y,z;
	angle_t     angle;
	mobj_t		*mobj;

	if (!playeringame[mthing->type-1])
		return;						/* not playing */
		
	p = &players[mthing->type-1];

	if (p->playerstate == PST_REBORN)
		G_PlayerReborn (mthing->type-1);

	if (p->starpostnum)
	{
		x = p->starpostx << FRACBITS;
		y = p->starposty << FRACBITS;
		z = p->starpostz << FRACBITS;
		angle = p->starpostangle << ANGLETOFINESHIFT;
	}
	else
	{
		x = mthing->x << FRACBITS;
		y = mthing->y << FRACBITS;
		z = ONFLOORZ;
		angle = ANG45 * (mthing->angle/45);
	}
	mobj = P_SpawnMobj (x,y,z, MT_PLAYER);
	
	mobj->angle = angle;

	mobj->player = p - players + 1;
	mobj->health = p->health;
	p->mo = mobj;
	p->playerstate = PST_LIVE;	
	p->whiteFlash = 0;

	if (gamemapinfo.act == 3)
		p->health = mobj->health = 11;
	else
		p->health = mobj->health = 1;

	// Set camera position
	camera.x = mobj->x;
	camera.y = mobj->y;
	if (gamemapinfo.act == 3)
		P_ThrustValues(mobj->angle, -CAM_DIST, &camera.x, &camera.y);
	else
		P_ThrustValues(mobj->angle + (ANG45 * 3), -CAM_DIST, &camera.x, &camera.y);
	camera.x = (camera.x >> FRACBITS) << FRACBITS;
	camera.y = (camera.y >> FRACBITS) << FRACBITS;
	camera.subsector = I_TO_SS(R_PointInSubsector2(camera.x, camera.y));
	camera.z = I_TO_SEC(camera.subsector->isector)->floorheight + (mobj->theight << FRACBITS);
	
	if (!netgame)
		return;
}


/*
=================
=
= P_MapThingSpawnsMobj
=
= Returns false for things that don't spawn a mobj
==================
*/
int P_MapThingSpawnsMobj (mapthing_t* mthing)
{
	int			i;

	if (mthing->type >= 600 && mthing->type <= 603)
		return 3;

/* check for players specially */
#if 0
	if (mthing->type > 4)
		return false;	/*DEBUG */
#endif

	if (mthing->type <= 4)
		return 0;

/* find which type to spawn */
	{
		for (i = 0; i < NUMMOBJTYPES; i++)
		{
			if (mthing->type == mobjinfo[i].doomednum)
			{
				if (mobjinfo[i].flags & MF_RINGMOBJ)
				{
					if (mobjinfo[i].flags & MF_NOBLOCKMAP)
						return 4;

					return 3;
				}
				if (mobjinfo[i].flags & MF_STATIC)
					return 2;
				return 1;
			}
		}
	}

	return 0;
}

/*
=================
=
= P_SpawnMapThing
=
= The fields of the mapthing should already be in host byte order
==================
*/

fixed_t P_GetMapThingSpawnHeight(const mobjtype_t mobjtype, const mapthing_t* mthing, const fixed_t x, const fixed_t y, const fixed_t z)
{
	fixed_t dz = z; // Base offset from the floor.

	if (dz == 0)
	{
		switch (mobjtype)
		{
		// Objects with a non-zero default height.
	/*	case MT_CRAWLACOMMANDER:
		case MT_DETON:
		case MT_JETTBOMBER:
		case MT_JETTGUNNER:*/
		case MT_EGGMOBILE2:
			if (!dz)
				dz = 33*FRACUNIT;
			break;
		case MT_EGGMOBILE:
			if (!dz)
				dz = 128*FRACUNIT;
			break;
		case MT_GOLDBUZZ:
		case MT_REDBUZZ:
			if (!dz)
				dz = 288*FRACUNIT;
			break;
		// Ring-like items, float additional units unless args[0] is set.
		case MT_TOKEN:
		case MT_RING:
		case MT_BLUESPHERE:
			dz += 24*FRACUNIT;
			break;
		default:
			break;
		}	
	}

/*	if (!dz) // Snap to the surfaces when there's no offset set.
	{
//		if (flip)
//			return ONCEILINGZ;
//		else
			return ONFLOORZ;
	}*/

	const sector_t *sec = SS_SECTOR(R_PointInSubsector2(x, y));

	return sec->floorheight + dz;
}

void P_SpawnMapThing (mapthing_t *mthing, int thingid)
{
	int			i;
	mobj_t		*mobj;
	fixed_t		x,y,z;
	
/* check for players specially */

#if 0
if (mthing->type > 4)
return;	/*DEBUG */
#endif

	if (mthing->type <= 4)
	{
		/* save spots for respawning in network games */
		if (mthing->type <= MAXPLAYERS)
		{
			D_memcpy (&playerstarts[mthing->type-1], mthing, sizeof(*mthing));
			if (netgame != gt_deathmatch)
				P_SpawnPlayer (mthing);
		}
		return;
	}

	if (!P_MapThingSpawnsMobj(mthing))
		return;

/* find which type to spawn */
	for (i=0 ; i< NUMMOBJTYPES ; i++)
		if (mthing->type == mobjinfo[i].doomednum)
			break;
	
	if (i==NUMMOBJTYPES)
		I_Error ("P_SpawnMapThing: Unknown type %i at (%i, %i)",mthing->type
		, mthing->x, mthing->y);
	
/* spawn it */

	x = mthing->x << FRACBITS;
	y = mthing->y << FRACBITS;

	z = (mthing->options >> 4) << FRACBITS;
	z = P_GetMapThingSpawnHeight(i, mthing, x, y, z);

	if (mthing->type == 312 && (tokenbits & (1 << totaltokens))) // MT_TOKEN
	{
		totaltokens++;
		return; // Player already has this token
	}

	mobj = P_SpawnMobj (x,y,z, i);
	if (mobj->type == MT_RING)
	{
		totalitems++;
	}
	else if (mobj->type == MT_EGGMOBILE)
	{
		mobj_t *eggmech = P_SpawnMobj(x, y, z, MT_EGGMOBILE_MECH);
		eggmech->target = mobj;
	}
	else if (mobj->type == MT_EGGMOBILE2)
	{
		mobj_t *eggmech = P_SpawnMobj(x, y, z, MT_EGGMOBILE2_MECH);
		eggmech->target = mobj;
	}
	else if (mobj->type == MT_ROBOHOOD)
	{
		if (mthing->options & MTF_AMBUSH)
			mobj->flags2 |= MF2_SPAWNEDJETS;
	}
	else if (mobj->type == MT_EGGGUARD)
	{
		mobj_t *shield = P_SpawnMobj(x, y, z, MT_EGGSHIELD);
		shield->target = mobj;

		if (mthing->options & MTF_AMBUSH)
			mobj->flags2 |= MF2_SPAWNEDJETS; // We use this as an extra flag for all kinds of fun stuff. :)

		mobj->extradata = 0;

		if (mthing->options & MTF_EXTRA)
			SETUPPER8(mobj->extradata, TMGD_RIGHT) // TMGD_RIGHT
		else if (mthing->options & MTF_OBJECTSPECIAL)
			SETUPPER8(mobj->extradata, TMGD_LEFT) // TMGD_LEFT
	}
	else if (mobj->type == MT_FACESTABBER)
	{
		mobj_t *jet = P_SpawnMobj(mobj->x, mobj->y, mobj->z, MT_JETFUME1);
		jet->target = mobj;
		jet->flags2 |= MF2_DONTDRAW;
		jet->movecount = 4; // This tells it which one it is
	}

	if (mobj->type == MT_TOKEN) {
		((ringmobj_t *)mobj)->pad = (1 << totaltokens);
		totaltokens++;
	}

	if (mobj->type == MT_YELLOWSPRING || mobj->type == MT_YELLOWDIAG || mobj->type == MT_YELLOWHORIZ
			|| mobj->type == MT_REDSPRING || mobj->type == MT_REDDIAG || mobj->type == MT_REDHORIZ)
	{
		((ringmobj_t *)mobj)->pad = (mthing->angle * ANGLE_1) >> ANGLETOFINESHIFT;
	}

	if (mobj->flags & MF_RINGMOBJ)
		return;


	// Set the angle
	if (mobj->type == MT_STARPOST)
	{
		mobj->health = (mthing->angle / 360) + 1;
		mobj->angle = (mthing->angle % 360) * ANGLE_1;
	}
	else {
		mobj->angle = mthing->angle * ANGLE_1;
	}

	if (mobj->tics > 0)
		mobj->tics = 1 + (P_Random () % mobj->tics);
}

boolean Mobj_HasFlags2(mobj_t *mo, VINT value)
{
   if (mo->flags & MF_RINGMOBJ)
      return false;

   return (mo->flags2 & value);
}

fixed_t Mobj_GetHeight(mobj_t *mo)
{
	if (mo->flags & MF_RINGMOBJ)
		return mobjinfo[mo->type].height;

	return mo->theight << FRACBITS;
}

fixed_t Mobj_GetHalfHeight(mobj_t *mo)
{
	if (mo->flags & MF_RINGMOBJ)
		return mobjinfo[mo->type].height >> 1;

	return mo->theight << (FRACBITS-1);
}


/*
===============================================================================

						GAME SPAWN FUNCTIONS

===============================================================================
*/

/*
================
=
= P_CheckMissileSpawn
=
= Moves the missile forward a bit and possibly explodes it right there
=
================
*/

void P_CheckMissileSpawn (mobj_t *th)
{
	ptrymove_t tm;

	th->x += (th->momx>>1);
	th->y += (th->momy>>1);	/* move a little forward so an angle can */
							/* be computed if it immediately explodes */
	th->z += (th->momz>>1);
	if (!P_TryMove (&tm, th, th->x, th->y))
		P_ExplodeMissile (th);
}

/*
================
=
= P_SpawnMissile
=
================
*/

mobj_t *P_SpawnMissile (mobj_t *source, mobj_t *dest, mobjtype_t type)
{
    mobj_t        *th;
    angle_t        an;
    int            speed;
    const mobjinfo_t* thinfo = &mobjinfo[type];

    th = P_SpawnMobj (source->x,source->y, source->z + 4*8*FRACUNIT, type);
    if (thinfo->seesound)
        S_StartSound (source, thinfo->seesound);
    th->target = source;        /* where it came from */
    an = R_PointToAngle2 (source->x, source->y, dest->x, dest->y);    
    th->angle = an;

    th->momx = dest->x - th->x;
    th->momy = dest->y - th->y;
    th->momz = dest->z - th->z;
	FV3_Normalize((vector3_t*)&th->momx, (vector3_t*)&th->momx);

    speed = mobjinfo[th->type].speed;
    th->momx = FixedMul(th->momx, speed);
    th->momy = FixedMul(th->momy, speed);
    th->momz = FixedMul(th->momz, speed)*2; // why do we multiply by two here I don't even

    P_CheckMissileSpawn (th);

    return th;
}

void P_MobjCheckWater(mobj_t *mo)
{
	if (mo->player)
	{
		player_t *player = &players[mo->player-1];
		VINT wasinwater = player->pflags & PF_UNDERWATER;
		player->pflags &= ~(PF_TOUCHWATER|PF_UNDERWATER);
		const sector_t *moSec = SS_SECTOR(mo->isubsector);
		fixed_t watertop = moSec->floorheight - 512*FRACUNIT;

		if (moSec->heightsec >= 0)
		{
			watertop = GetWatertopMo(mo);

			if (mo->z + mobjinfo[MT_PLAYER].height/2 < watertop)
			{
				player->pflags |= PF_UNDERWATER;

				if (player->shield == SH_ATTRACT)
				{
					player->whiteFlash = 4;
					player->shield = 0;
				}

				if (!(player->powers[pw_invulnerability]))
				{
/*					if (player->powers[pw_ringshield])
					{
						player->powers[pw_ringshield] = false;
						player->bonuscount = 1;
					}*/
				}
				if (player->powers[pw_underwater] <= 0
					&& player->shield != SH_ELEMENTAL
					&& !player->exiting)
					player->powers[pw_underwater] = UNDERWATERTICS + 1;
			}
		}

		if (player->playerstate != PST_DEAD
			&& ((player->pflags & PF_UNDERWATER) != wasinwater))
		{
			// Check to make sure you didn't just cross into a sector to jump out of
			// that has shallower water than the block you were originally in.
			if (watertop - mo->floorz <= mobjinfo[MT_PLAYER].height >> 1)
				return;

			if (wasinwater && mo->momz > 0)
				mo->momz = FixedMul(mo->momz, 111855/*1.7*/); // Give the mobj a little out-of-water boost.

			if (mo->momz < 0)
			{
				if (mo->z + (mobjinfo[MT_PLAYER].height >> 1) - mo->momz >= watertop)
				{
					// Spawn a splash
					P_SpawnMobj(mo->x, mo->y, watertop, MT_SPLISH);
				}

				// skipping stone!
				if (!(player->pflags & PF_JUMPED) && player->speed > (D_abs(mo->momz) << 1)
					&& (player->pflags & PF_SPINNING) && mo->z + mobjinfo[MT_PLAYER].height - mo->momz > watertop)
				{
					mo->momz = -mo->momz/2;

					if (mo->momz > 6*FRACUNIT)
						mo->momz = 6*FRACUNIT;

					mo->momx -= mo->momx >> 2;
					mo->momy -= mo->momy >> 2;
				}

				int bubbleCount = D_abs(mo->momz >> FRACBITS);

				// Let's not get too crazy here
				if (bubbleCount > 8)
					bubbleCount = 8;

				for (int i = 0; i < bubbleCount; i++)
				{
					mobj_t *bubble;
					uint8_t prandom[6];
					prandom[0] = P_Random();
					prandom[1] = P_Random();
					prandom[2] = P_Random();
					prandom[3] = P_Random();
					prandom[4] = P_Random();
					prandom[5] = P_Random();

					if (prandom[0] < 32)
						bubble =
						P_SpawnMobj(mo->x + (prandom[1]<<(FRACBITS-3)) * (prandom[2]&1 ? 1 : -1),
							mo->y + (prandom[3]<<(FRACBITS-3)) * (prandom[4]&1 ? 1 : -1),
							mo->z + (prandom[5]<<(FRACBITS-2)), MT_MEDIUMBUBBLE);
					else
						bubble =
						P_SpawnMobj(mo->x + (prandom[1]<<(FRACBITS-3)) * (prandom[2]&1 ? 1 : -1),
							mo->y + (prandom[3]<<(FRACBITS-3)) * (prandom[4]&1 ? 1 : -1),
							mo->z + (prandom[5]<<(FRACBITS-2)), MT_SMALLBUBBLE);

					if (bubble)
						bubble->momz = mo->momz >> 4;
				}
			}
			else if (mo->momz > 0)
			{
				if (mo->z + mobjinfo[MT_PLAYER].height / 2 - mo->momz < watertop)
				{
					// Spawn a splash
					P_SpawnMobj(mo->x, mo->y, watertop, MT_SPLISH);
				}
			}

			S_StartSound(mo, sfx_s3k_6c);
		}
	}
}