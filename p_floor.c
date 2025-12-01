#include "doomdef.h"
#include "p_local.h"
#include "p_camera.h"

//e6y
#define STAIRS_UNINITIALIZED_CRUSH_FIELD_VALUE 10

/*================================================================== */
/*================================================================== */
/* */
/*								FLOORS */
/* */
/*================================================================== */
/*================================================================== */



/*================================================================== */
/* */
/*	Move a plane (floor or ceiling) and check for crushing */
/* */
/*================================================================== */
result_e	T_MovePlane(sector_t *sector,fixed_t speed,
			fixed_t dest,boolean changeSector,int floorOrCeiling,int direction)
{
	boolean	flag;
	fixed_t	lastpos;
	
	switch(floorOrCeiling)
	{
		case 0:		/* FLOOR */
			switch(direction)
			{
				case -1:	/* DOWN */
					if (sector->floorheight - speed < dest)
					{
						lastpos = sector->floorheight;
						sector->floorheight = dest;

						if (changeSector)
						{
							flag = P_ChangeSector(sector,false);
							if (flag == true)
							{
								sector->floorheight = lastpos;
								P_ChangeSector(sector,false);
							}
						}
						return pastdest;
					}
					else
					{
						lastpos = sector->floorheight;
						sector->floorheight -= speed;

						if (changeSector)
						{
							flag = P_ChangeSector(sector,false);
							if (flag == true)
							{
								sector->floorheight = lastpos;
								P_ChangeSector(sector,false);
							}
						}
					}
					break;
						
				case 1:		/* UP */
					if (sector->floorheight + speed > dest)
					{
						lastpos = sector->floorheight;
						sector->floorheight = dest;
						if (changeSector)
						{
							flag = P_ChangeSector(sector,false);
							if (flag == true)
							{
								sector->floorheight = lastpos;
								P_ChangeSector(sector,false);
							}
						}
						return pastdest;
					}
					else	/* COULD GET CRUSHED */
					{
						lastpos = sector->floorheight;
						sector->floorheight += speed;
						if (changeSector)
						{
							flag = P_ChangeSector(sector,false);
							if (flag == true)
							{
								sector->floorheight = lastpos;
								P_ChangeSector(sector,false);
							}
						}
					}
					break;
			}
			break;
									
		case 1:		/* CEILING */
			switch(direction)
			{
				case -1:	/* DOWN */
					if (sector->ceilingheight - speed < dest)
					{
						lastpos = sector->ceilingheight;
						sector->ceilingheight = dest;
						if (changeSector)
						{
							flag = P_ChangeSector(sector,false);
							if (flag == true)
							{
								sector->ceilingheight = lastpos;
								P_ChangeSector(sector,false);
							}
						}
						return pastdest;
					}
					else	/* COULD GET CRUSHED */
					{
						lastpos = sector->ceilingheight;
						sector->ceilingheight -= speed;

						if (changeSector)
						{
							flag = P_ChangeSector(sector,false);
							if (flag == true)
							{
								sector->ceilingheight = lastpos;
								P_ChangeSector(sector,false);
							}
						}
					}
					break;
						
				case 1:		/* UP */
					if (sector->ceilingheight + speed > dest)
					{
						lastpos = sector->ceilingheight;
						sector->ceilingheight = dest;
						if (changeSector)
						{
							flag = P_ChangeSector(sector,false);
							if (flag == true)
							{
								sector->ceilingheight = lastpos;
								P_ChangeSector(sector,false);
							}
						}
						return pastdest;
					}
					else
					{
						lastpos = sector->ceilingheight;
						sector->ceilingheight += speed;
						if (changeSector)
						{
							flag = P_ChangeSector(sector,false);
							if (flag == true)
							{
								sector->ceilingheight = lastpos;
								P_ChangeSector(sector,false);
							}
						}
					}
					break;
			}
			break;
		
	}
	return ok;
}

/*================================================================== */
/* */
/*	MOVE A FLOOR TO IT'S DESTINATION (UP OR DOWN) */
/* */
/*================================================================== */
#define ELEVATORSPEED (FRACUNIT / 4)
void T_MoveFloor(floormove_t *floor)
{
	result_e	res = ok;

	if (floor->delayTimer)
	{
		floor->delayTimer--;
		return;
	}
	
	if (floor->type == thz2DropBlock)
	{
		floor->speed += GRAVITY/2;
		res = T_MovePlane(floor->sector,floor->speed,
				(floor->floordestheight << FRACBITS) + (floor->sector->ceilingheight - floor->sector->floorheight), !floor->dontChangeSector, 1, floor->direction);

		res = T_MovePlane(floor->sector,floor->speed,
			floor->floordestheight << FRACBITS, !floor->dontChangeSector, 0, floor->direction);		
	}
	else if (floor->type == floorContinuous || floor->type == bothContinuous)
	{
		const fixed_t wh = D_abs(floor->sector->floorheight - (floor->floorwasheight << FRACBITS));
		const fixed_t dh = D_abs(floor->sector->floorheight - (floor->floordestheight << FRACBITS));

		// Slow down when reaching destination Tails 12-06-2000
		if (wh < dh)
			floor->speed = FixedDiv(wh, 25*FRACUNIT) + FRACUNIT/4;
		else
			floor->speed = FixedDiv(dh, 25*FRACUNIT) + FRACUNIT/4;

		if (floor->origSpeed)
		{
			floor->speed = FixedMul(floor->speed, floor->origSpeed);
			if (floor->speed > floor->origSpeed)
				floor->speed = (floor->origSpeed);
			if (floor->speed < 1)
				floor->speed = 1;
		}
		else
		{
			if (floor->speed > 3*FRACUNIT)
				floor->speed = 3*FRACUNIT;
			if (floor->speed < 1)
				floor->speed = 1;
		}

		res = T_MovePlane(floor->sector,floor->speed,
				floor->floordestheight << FRACBITS, !floor->dontChangeSector, 0, floor->direction);

		if (floor->type == bothContinuous)
			T_MovePlane(floor->sector, floor->speed, (floor->floordestheight + floor->ceilDiff) << FRACBITS, !floor->dontChangeSector, 1, floor->direction);
	}
	else if (floor->type == eggCapsuleInner || floor->type == eggCapsuleOuter
		|| floor->type == eggCapsuleOuterPop || floor->type == eggCapsuleInnerPop)
	{
		res = T_MovePlane(floor->sector,floor->speed,
				floor->floordestheight << FRACBITS,floor->crush,0,floor->direction);
	}
	else if (floor->type == moveCeilingByFrontSector || floor->type == continuousMoverCeiling)
	{
		res = T_MovePlane(floor->sector,floor->speed,
				floor->floordestheight << FRACBITS,true,1,floor->direction);
	}
	else
	{
		res = T_MovePlane(floor->sector,floor->speed,
				floor->floordestheight << FRACBITS,true,0,floor->direction);
	}

	if (floor->type == continuousMoverFloor)
	{
		const line_t *sourceline = &lines[floor->sourceline];
		const sector_t *frontsector = LD_FRONTSECTOR(sourceline);
		const sector_t *backsector = LD_BACKSECTOR(sourceline);
		if (frontsector && backsector)
		{
			const fixed_t origspeed = FixedMul(floor->origSpeed, (FRACUNIT/2));
			const fixed_t fs = D_abs(floor->sector->floorheight - (frontsector->floorheight));
			const fixed_t bs = D_abs(floor->sector->floorheight - (backsector->floorheight));
			if (fs < bs)
				floor->speed = FixedDiv(fs,25*FRACUNIT) + FRACUNIT/4;
			else
				floor->speed = FixedDiv(bs,25*FRACUNIT) + FRACUNIT/4;

			floor->speed = FixedMul(floor->speed, origspeed);
		}
	}
	else if (floor->type == continuousMoverCeiling)
	{
		const line_t *sourceline = &lines[floor->sourceline];
		const sector_t *frontsector = LD_FRONTSECTOR(sourceline);
		const sector_t *backsector = LD_BACKSECTOR(sourceline);
		const fixed_t origspeed = FixedMul(floor->origSpeed, (FRACUNIT/2));
		if (frontsector && backsector)
		{
			const fixed_t fs = D_abs(floor->sector->floorheight - (frontsector->ceilingheight));
			const fixed_t bs = D_abs(floor->sector->floorheight - (backsector->ceilingheight));
			if (fs < bs)
				floor->speed = FixedDiv(fs,25*FRACUNIT) + FRACUNIT/4;
			else
				floor->speed = FixedDiv(bs,25*FRACUNIT) + FRACUNIT/4;

			floor->speed = FixedMul(floor->speed, origspeed);
		}
	}

	if (res == pastdest)
	{
		if (floor->type == floorContinuous || floor->type == bothContinuous)
		{
			if (floor->direction > 0)
			{
				floor->direction = -1;
				floor->speed = floor->origSpeed;
				floor->floorwasheight = floor->floordestheight;
				floor->floordestheight = sectors[floor->lowestSector].floorheight >> FRACBITS;
			}
			else
			{
				floor->direction = 1;
				floor->speed = floor->origSpeed;
				floor->floorwasheight = floor->floordestheight;
				floor->floordestheight = sectors[floor->highestSector].floorheight >> FRACBITS;
			}
		}
		else if (floor->type == continuousMoverFloor)
		{
			const sector_t *frontsector = LD_FRONTSECTOR(&lines[floor->sourceline]);
			const sector_t *backsector = LD_BACKSECTOR((&lines[floor->sourceline]));
			if ((floor->floordestheight << FRACBITS) == frontsector->floorheight)
				floor->floordestheight = backsector->floorheight >> FRACBITS;
			else
				floor->floordestheight = frontsector->floorheight >> FRACBITS;

			floor->direction = ((floor->floordestheight << FRACBITS) < floor->sector->floorheight) ? -1 : 1;
			floor->delayTimer = floor->delay;
		}
		else if (floor->type == continuousMoverCeiling)
		{
			const sector_t *frontsector = LD_FRONTSECTOR(&lines[floor->sourceline]);
			const sector_t *backsector = LD_BACKSECTOR((&lines[floor->sourceline]));
			if ((floor->floordestheight << FRACBITS) == frontsector->ceilingheight)
				floor->floordestheight = backsector->ceilingheight >> FRACBITS;
			else
				floor->floordestheight = frontsector->ceilingheight >> FRACBITS;

			floor->direction = ((floor->floordestheight << FRACBITS) < floor->sector->ceilingheight) ? -1 : 1;
			floor->delayTimer = floor->delay;
		}
		else
		{
			floor->sector->specialdata = (SPTR)0;
			switch(floor->type)
			{
				case thz2DropBlock:
				{
					// Find the turret, destroy it
					for (mobj_t *node = mobjhead.next; node != (void*)&mobjhead; node = node->next)
					{
						if (node->type == MT_TURRET && node->health > 0)
						{
							// Spawn MT_EXPLODE at every 45
							for (VINT ang = 0; ang < 8192; ang += 1024)
							{
								fixed_t outX = node->x;
								fixed_t outY = node->y;
								//P_ThrustValues(ang << ANGLETOFINESHIFT, 112*FRACUNIT, &outX, &outY);
								P_SpawnMobj(outX, outY, node->z, MT_EXPLODE);
							}

							P_SetMobjState(node, S_XPLD1);
							S_StartSound(node, sfx_s3k_3d);
							break;
						}
					}
					break;
				}
				case moveFloorByFrontSector:
					if (floor->texture < 255)
						floor->sector->floorpic = (uint8_t)floor->texture;
					if (floor->tag)
						P_LinedefExecute(floor->tag, NULL, NULL);
					break;
				case moveCeilingByFrontSector:
					if (floor->texture < 255)
						floor->sector->ceilingpic = (uint8_t)floor->texture;
					if (floor->tag)
						P_LinedefExecute(floor->tag, NULL, NULL);
					break;
				case eggCapsuleInner:
						floor->sector->special = 2;
						break;
			}

			P_RemoveThinker(&floor->thinker);
		}
	}
}

/*================================================================== */
/* */
/*	HANDLE FLOOR TYPES */
/* */
/*================================================================== */
int EV_DoFloorTag(line_t *line,floor_e floortype, uint8_t tag)
{
	int			secnum;
	int			rtn;
	sector_t	*sec;
	floormove_t	*floor;

	secnum = -1;
	rtn = 0;
	while ((secnum = P_FindSectorFromLineTagNum(tag,secnum)) >= 0)
	{
		sec = &sectors[secnum];
		
		/*	ALREADY MOVING?  IF SO, KEEP GOING... */
//		if (sec->specialdata)
//			continue;
			
		/* */
		/*	new floor thinker */
		/* */
		rtn = 1;
		floor = Z_Calloc (sizeof(*floor), PU_LEVSPEC);
		P_AddThinker (&floor->thinker);
		sec->specialdata = LPTR_TO_SPTR(floor);
		floor->thinker.function = T_MoveFloor;
		floor->type = floortype;
		floor->crush = false;
		floor->dontChangeSector = false;
		floor->texture = (uint8_t)-1;
		floor->tag = 0;
		floor->sector = sec;
		floor->delayTimer = floor->delay = 0;
		floor->sourceline = line - lines;
		switch(floortype)
		{
			case thz2DropBlock:
			{
				side_t *side = &sides[line->sidenum[0]];
				int16_t rowoffset = (side->textureoffset & 0xf000) | ((unsigned)side->rowoffset << 4);
      			rowoffset >>= 4; // sign extend

				floor->floordestheight = rowoffset;
				floor->origSpeed = floor->speed = 1;
				floor->direction = -1;
				break;
			}
			case continuousMoverFloor:
			case continuousMoverCeiling:
			{
				mapvertex_t *v1 = &vertexes[line->v1];
				mapvertex_t *v2 = &vertexes[line->v2];
				side_t *side = &sides[line->sidenum[0]];
				int16_t textureoffset = side->textureoffset & 0xfff;
				textureoffset <<= 4; // sign extend
				textureoffset >>= 4; // sign extend
				int16_t rowoffset = (side->textureoffset & 0xf000) | ((unsigned)side->rowoffset << 4);
      			rowoffset >>= 4; // sign extend

				// args3 is same as args2
				// args4 is rowoffset
				// args5 is textureoffset
				floor->speed = P_AproxDistance((v1->x - v2->x) << FRACBITS, (v1->y - v2->y) << FRACBITS) >> 2;
				floor->origSpeed = floor->speed;

				fixed_t destHeight = floortype == continuousMoverCeiling ? LD_FRONTSECTOR(line)->ceilingheight >> FRACBITS
					: LD_FRONTSECTOR(line)->floorheight >> FRACBITS;
				fixed_t curHeight = floortype == continuousMoverCeiling ? sec->ceilingheight >> FRACBITS : sec->floorheight >> FRACBITS;

				floor->floordestheight = destHeight;

				if (destHeight >= curHeight)
					floor->direction = 1; // up
				else
					floor->direction = -1; // down

				// Any delay?
				floor->delay = textureoffset;
				floor->delayTimer = rowoffset;
				break;
			}
			case moveCeilingByFrontSector:
			case moveFloorByFrontSector:
			{
				mapvertex_t *v1 = &vertexes[line->v1];
				mapvertex_t *v2 = &vertexes[line->v2];
				side_t *side = &sides[line->sidenum[0]];
				int16_t textureoffset = side->textureoffset & 0xfff;
				textureoffset <<= 4; // sign extend
				textureoffset >>= 4; // sign extend

				VINT args3 = (ldflags[line-lines] & ML_BLOCKMONSTERS) ? textureoffset : 0; // tag of linedef executor to run on completion
				VINT args4 = ldflags[line-lines] & ML_NOCLIMB; // change flat?

				floor->speed = P_AproxDistance((v1->x - v2->x) << FRACBITS, (v1->y - v2->y) << FRACBITS) >> 3;

				// chained linedef executing ability
				floor->tag = args3;

				if (floortype == moveCeilingByFrontSector)
				{
					floor->floordestheight = LD_FRONTSECTOR(line)->ceilingheight >> FRACBITS;

					if (floor->floordestheight >= sec->ceilingheight >> FRACBITS)
						floor->direction = 1; // up
					else
						floor->direction = -1; // down

					// Optionally change flat
					floor->texture = args4 ? LD_FRONTSECTOR(line)->ceilingpic : (uint8_t)-1;
				}
				else
				{
					floor->floordestheight = LD_FRONTSECTOR(line)->floorheight >> FRACBITS;

					if (floor->floordestheight >= sec->floorheight >> FRACBITS)
						floor->direction = 1; // up
					else
						floor->direction = -1; // down

					// Optionally change flat
					floor->texture = args4 ? LD_FRONTSECTOR(line)->floorpic : (uint8_t)-1;
				}
				break;
			}
			case bothContinuous:
				floor->ceilDiff = (sec->ceilingheight - sec->floorheight) >> FRACBITS;
			case floorContinuous:
				floor->controlSector = &sectors[sides[line->sidenum[0]].sector];
				floor->origSpeed = P_AproxDistance((vertexes[line->v1].x - vertexes[line->v2].x) << FRACBITS,
												(vertexes[line->v1].y - vertexes[line->v2].y) << FRACBITS) >> 2;
				floor->speed = floor->origSpeed;
				if (ldflags[line-lines] & ML_BLOCKMONSTERS)
					floor->dontChangeSector = true;

				floor->lowestSector = P_FindNextLowestFloor(floor->controlSector, sec->floorheight) - sectors;
				floor->highestSector = P_FindNextHighestFloor(floor->controlSector, sec->floorheight) - sectors;

				if (ldflags[line-lines] & ML_NOCLIMB)
				{
					floor->direction = 1;
					floor->floordestheight = sectors[floor->highestSector].floorheight >> FRACBITS;
				}
				else
				{
					floor->direction = -1;
					floor->floordestheight = sectors[floor->lowestSector].floorheight >> FRACBITS;
				}

				floor->floorwasheight = sec->floorheight >> FRACBITS;
				floor->crush = sides[line->sidenum[0]].rowoffset; // initial delay
				break;
			case lowerFloor:
				floor->direction = -1;
				floor->speed = FLOORSPEED;
				floor->floordestheight = 
					P_FindHighestFloorSurrounding(sec)->floorheight >> FRACBITS;
				break;
			case lowerFloorToLowest:
				floor->direction = -1;
				floor->speed = FLOORSPEED;
				floor->floordestheight = 
					P_FindLowestFloorSurrounding(sec)->floorheight >> FRACBITS;
				break;
			case turboLower:
				floor->direction = -1;
				floor->speed = FLOORSPEED * 4;
				floor->floordestheight = (8) + 
						(P_FindHighestFloorSurrounding(sec)->floorheight >> FRACBITS);
				break;
			case raiseFloor:
				floor->direction = 1;
				floor->speed = FLOORSPEED;
				floor->floordestheight = 
					P_FindLowestCeilingSurrounding(sec)->ceilingheight >> FRACBITS;
				if (floor->floordestheight > sec->ceilingheight)
					floor->floordestheight = sec->ceilingheight;
				break;
			case raiseFloorToNearest:
				floor->direction = 1;
				floor->speed = FLOORSPEED;
				floor->floordestheight = 
					P_FindNextHighestFloor(sec,sec->floorheight)->floorheight >> FRACBITS;
				break;
			default:
				break;
		}
	}
	return rtn;
}

int EV_DoFloor(line_t *line,floor_e floortype)
{
	return EV_DoFloorTag(line, floortype, P_GetLineTag(line));
}
