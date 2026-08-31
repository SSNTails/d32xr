/* P_Spec.c */
#include "doomdef.h"
#include "p_local.h"
#include "p_camera.h"

/*
===================
=
= P_InitPicAnims
=
===================
*/

static const animdef_t	animdefs[] =
{
	{false,	"BWATER08",	"BWATER01"},
	{false, "CHEMG04",  "CHEMG01"},
	{false, "DWATER08", "DWATER01"},
	{false,	"FWATER4",	"FWATER1"},
	{false, "LITER3",   "LITER1"},
	{false, "THZELF2",  "THZELF1"},

	{true,  "DOWN3D",   "DOWN3A"},
	{true,  "DOWN5C",   "DOWN5A"},
	{true,	"GFALL4",	"GFALL1"},
	{true,  "RVZFALL4", "RVZFALL1"},
	{true,  "TFALL4",   "TFALL1"},
	{true,  "UP3D",     "UP3A"},
	{true,  "UP5C",     "UP5A"},
	{true, "REDEGG2", "STATIC2"},
	{true, "REDEGG3", "REDEGG2"},

	{-1}
};

anim_t	*anims/*[MAXANIMS]*/, * lastanim;


void P_InitPicAnims (void)
{
	int		i;
	
/* */
/*	Init animation */
/* */
	lastanim = anims;
	for (i=0 ; animdefs[i].istexture != -1 ; i++)
	{
		if (animdefs[i].istexture)
		{
			if (R_CheckTextureNumForName(animdefs[i].startname) == -1)
				continue;
			lastanim->picnum = R_TextureNumForName (animdefs[i].endname);
			lastanim->basepic = R_TextureNumForName (animdefs[i].startname);
		}
		else
		{
			if (W_CheckNumForName(animdefs[i].startname) == -1)
				continue;
			lastanim->picnum = R_FlatNumForName (animdefs[i].endname);
			lastanim->basepic = R_FlatNumForName (animdefs[i].startname);
		}
		lastanim->current = 0;
		lastanim->istexture = animdefs[i].istexture;
		lastanim->numpics = lastanim->picnum - lastanim->basepic + 1;
#if 0
/* FIXME */
		if (lastanim->numpics < 2)
			I_Error ("P_InitPicAnims: bad cycle from %s to %s"
			, animdefs[i].startname, animdefs[i].endname);
#endif
		lastanim++;
	}
	
}


/*
==============================================================================

							UTILITIES

==============================================================================
*/

__attribute((noinline))
VINT P_FindNextSectorLine(VINT isector, VINT start)
{
	VINT i;

	for (i = start + 1; i < numlines; i++)
	{
		const line_t *line = &lines[i];
		const side_t *firstSide = &sides[line->sidenum[0]];

		if (firstSide->sector == isector)
			return i;

		const side_t *secondSide = line->sidenum[1] == -1 ? NULL : &sides[line->sidenum[1]];

		if (secondSide && secondSide->sector == isector)
			return i;
	}

	return -1;
}

/*================================================================== */
/* */
/*	Return sector_t * of sector next to current. NULL if not two-sided line */
/* */
/*================================================================== */
VINT getNextSector(line_t *line, VINT sec)
{
	if (!(line->sidenum[1] >= 0))
		return -1;
	
	VINT front = LD_IFRONTSECTOR(line);
	if (front == sec)
		return LD_IBACKSECTOR(line);

	return front;
}

/*================================================================== */
/* */
/*	FIND LOWEST FLOOR HEIGHT IN SURROUNDING SECTORS */
/* */
/*================================================================== */
VINT P_FindLowestFloorSurrounding(VINT sec)
{
	VINT			i = -1;
	line_t		*check;
	VINT other;
	VINT lowest = sec;

	while ((i = P_FindNextSectorLine(sec, i)) >= 0)
	{
		check = &lines[i];
		other = getNextSector(check,sec);

		if (other < 0)
			continue;

		if (I_TO_SEC(other)->floorheight < I_TO_SEC(lowest)->floorheight)
			lowest = other;
	}

	return lowest;
}

/*================================================================== */
/* */
/*	FIND HIGHEST FLOOR HEIGHT IN SURROUNDING SECTORS */
/* */
/*================================================================== */
VINT P_FindHighestFloorSurrounding(VINT sec)
{
	VINT			i = -1;
	line_t		*check;
	VINT other;
	VINT highest = sec;

	while ((i = P_FindNextSectorLine(sec, i)) >= 0)
	{
		check = &lines[i];
		other = getNextSector(check,sec);

		if (other < 0)
			continue;

		if (I_TO_SEC(other)->floorheight < I_TO_SEC(highest)->floorheight)
			highest = other;
	}

	return highest;
}

/*================================================================== */
/* */
/*	FIND NEXT HIGHEST CEILING IN SURROUNDING SECTORS */
/* */
/*================================================================== */
VINT P_FindNextHighestCeiling(VINT sec, fixed_t currentheight)
{
	VINT		i = -1;
	int			h = 0;
	VINT min;
	line_t		*check;
	VINT other;
	VINT heightlist[20];		/* 20 adjoining sectors max! */
	
	heightlist[0] = -1;
	while ((i = P_FindNextSectorLine(sec, i)) >= 0)
	{
		check = &lines[i];
		other = getNextSector(check,sec);

		if (other < 0)
			continue;

		if (I_TO_SEC(other)->ceilingheight > currentheight)
			heightlist[h++] = other;

		if (h == sizeof(heightlist) / sizeof(heightlist[0]))
			break;
	}
	
	if (h == 0)
		return sec;

	/* */
	/* Find lowest height in list */
	/* */
	min = heightlist[0];
	for (i = 1;i < h;i++)
		if (I_TO_SEC(heightlist[i])->ceilingheight < I_TO_SEC(min)->ceilingheight)
			min = heightlist[i];
			
	return min;
}

/*================================================================== */
/* */
/*	FIND NEXT HIGHEST FLOOR IN SURROUNDING SECTORS */
/* */
/*================================================================== */
VINT P_FindNextHighestFloor(VINT sec, fixed_t currentheight)
{
	VINT		i = -1;
	int			h = 0;
	VINT min;
	line_t		*check;
	VINT other;
	VINT heightlist[20];		/* 20 adjoining sectors max! */
	
	heightlist[0] = -1;
	while ((i = P_FindNextSectorLine(sec, i)) >= 0)
	{
		check = &lines[i];
		other = getNextSector(check,sec);

		if (other < 0)
			continue;

		if (I_TO_SEC(other)->floorheight > currentheight)
			heightlist[h++] = other;

		if (h == sizeof(heightlist) / sizeof(heightlist[0]))
			break;
	}
	
	if (h == 0)
		return sec;

	/* */
	/* Find lowest height in list */
	/* */
	min = heightlist[0];
	for (i = 1;i < h;i++)
		if (I_TO_SEC(heightlist[i])->floorheight < I_TO_SEC(min)->floorheight)
			min = heightlist[i];
			
	return min;
}

VINT P_FindNextLowestFloor(VINT sec, fixed_t currentheight)
{
	VINT		i = -1;
	int			h = 0;
	VINT min;
	line_t		*check;
	VINT other;
	VINT	    heightlist[20];		/* 20 adjoining sectors max! */

	heightlist[0] = -1;
	while ((i = P_FindNextSectorLine(sec, i)) >= 0)
	{
		check = &lines[i];
		other = getNextSector(check,sec);

		if (other < 0)
			continue;

		if (I_TO_SEC(other)->floorheight < currentheight)
			heightlist[h++] = other;

		if (h == sizeof(heightlist) / sizeof(heightlist[0]))
			break;
	}
	
	if (h == 0)
		return sec;

	/* */
	/* Find lowest height in list */
	/* */
	min = heightlist[0];
	for (i = 1;i < h;i++)
		if (I_TO_SEC(heightlist[i])->floorheight > I_TO_SEC(min)->floorheight)
			min = heightlist[i];
			
	return min;
}

/*================================================================== */
/* */
/*	FIND LOWEST CEILING IN THE SURROUNDING SECTORS */
/* */
/*================================================================== */
VINT P_FindLowestCeilingSurrounding(VINT sec)
{
	VINT		i = 0;
	line_t		*check;
	VINT other;
	VINT lowest = -1;
	
	while ((i = P_FindNextSectorLine(sec, i)) >= 0)
	{
		check = &lines[i];
		other = getNextSector(check,sec);

		if (other < 0)
			continue;

		if (lowest < 0 || I_TO_SEC(other)->ceilingheight < I_TO_SEC(lowest)->ceilingheight)
			lowest = other;
	}

	return lowest;
}

/*================================================================== */
/* */
/*	FIND HIGHEST CEILING IN THE SURROUNDING SECTORS */
/* */
/*================================================================== */
VINT P_FindHighestCeilingSurrounding(VINT sec)
{
	VINT	i = 0;
	line_t	*check;
	VINT other;
	VINT highest = -1;
	
	while ((i = P_FindNextSectorLine(sec, i)) >= 0)
	{
		check = &lines[i];
		other = getNextSector(check,sec);

		if (other < 0)
			continue;

		if (highest < 0 || I_TO_SEC(other)->ceilingheight > I_TO_SEC(highest)->ceilingheight)
			highest = other;
	}

	return highest;
}

/*================================================================== */
/* */
/*	RETURN NEXT SECTOR # THAT LINE TAG REFERS TO */
/* */
/*================================================================== */

VINT P_FindSectorWithTag(VINT tag, int start)
{
	if (start < 0)
		start = numstaticsectors - 1; // Start at the first dynamic sector element

	start++;

	for (int i = start; i < numsectors; i++)
	{
		if (I_TO_SEC(i)->tag == tag)
			return i;
	}

	return -1;
}

int	P_FindSectorFromLineTag(line_t *line, int start)
{
	return P_FindSectorFromLineTagNum(P_GetLineTag(line), start);
}

/*================================================================== */
/* */
/*	RETURN NEXT SECTOR # THAT LINE TAG REFERS TO */
/* */
/*================================================================== */
int	P_FindSectorFromLineTagNum(uint8_t tag, int start)
{
	if (start < 0)
		start = numstaticsectors - 1;

	start++;

	for (int i = start; i < numsectors; i++)
	{
		if (I_TO_SEC(i)->tag == tag)
			return i;
	}

	return -1;
}

// Pass '-1' to this to start
VINT P_FindNextLineWithTag(uint8_t tag, int *start)
{
	for (int i = (*start) + 1; i < numlineinfos; i++)
	{
		if (lineinfos[i].tag == tag)
		{
			*start = i;
			return lineinfos[i].line;
		}
	}

	return -1;
}

/*================================================================== */
/* */
/*	Find minimum light from an adjacent sector */
/* */
/*================================================================== */
int	P_FindMinSurroundingLight(VINT sector,int max)
{
	VINT			i = 0;
	int			min;
	line_t		*line;
	VINT check;
	
	min = max;
	while ((i = P_FindNextSectorLine(sector, i)) >= 0)
	{
		line = &lines[i];

		check = getNextSector(line,sector);
		if (check < 0)
			continue;
		if (I_TO_SEC(check)->lightlevel < min)
			min = I_TO_SEC(check)->lightlevel;
	}

	return min;
}

/*
==============================================================================

							EVENTS

Events are operations triggered by using, crossing, or shooting special lines, or by timed thinkers

==============================================================================
*/

typedef enum
{
	CF_RETURN   = 1,    // Return after crumbling
	CF_FLOATBOB = 2,    // Float on water
	CF_REVERSE  = 4,    // Reverse gravity
} crumbleflag_t;

typedef struct
{
	thinker_t thinker;
	line_t *sourceline;
	sector_t *sector;
	sector_t *actionsector; // The sector the rover action is taking place in.
	player_t *player; // Player who initiated the thinker (used for airbob)
	int16_t direction;
	int16_t timer;
	fixed_t speed;
	fixed_t floorwasheight; // Height the floor WAS at
	fixed_t ceilingwasheight; // Height the ceiling WAS at
	uint8_t flags;
} crumble_t;

// Warning! Both mo and callsec can be NULL
static void P_ProcessLineSpecial(line_t *line, mobj_t *mo, sector_t *callsec)
{
	uint8_t special = P_GetLineSpecial(line);
//	uint8_t tag = P_GetLineTag(line);

	switch (special)
	{
		case 215: // Drop block on turret (THZ2)
			EV_DoFloor(line, thz2DropBlock);
			break;
		case 218: // Move floor according to front sector
		case 219: // Move ceiling according to front sector
		{
			boolean ceiling = special - 218;
			EV_DoFloor(line, ceiling ? moveCeilingByFrontSector : moveFloorByFrontSector);
			break;
		}
		case 220: // Move Floor According to Front Texture Offsets
		{
			const side_t *side = &sides[line->sidenum[0]];
			int16_t textureoffset = side->textureoffset & 0xfff;
	    	textureoffset <<= 4; // sign extend
    	  	textureoffset >>= 4; // sign extend
			int16_t rowoffset = (side->textureoffset & 0xf000) | ((unsigned)side->rowoffset << 4);
      		rowoffset >>= 4; // sign extend

			if (line->flags & ML_NOCLIMB)
			{
				// Instant
				int secnum = -1;
				while ((secnum = P_FindSectorFromLineTag(line, secnum)) >= 0)
				{
					sector_t *sec = I_TO_SEC(secnum);
					sec->floorheight += rowoffset << FRACBITS;
					P_ChangeSector(sec, false);
				}
			}
			else
			{
				// Initiate movement
				int secnum = -1;
				while ((secnum = P_FindSectorFromLineTag(line, secnum)) >= 0)
				{
					sector_t *sec = I_TO_SEC(secnum);
					if (sec->specialdata)
						continue;

					floormove_t *floor = Z_Calloc (sizeof(*floor), PU_LEVSPEC);
					P_AddThinker (&floor->thinker);
					sec->specialdata = LPTR_TO_SPTR_NN(floor);
					floor->thinker.function = T_MoveFloor;
					floor->type = lowerFloor;
					floor->crush = false;
					floor->dontChangeSector = false;

					floor->direction = rowoffset > 0 ? 1 : -1;
					floor->sector = sec;
					floor->speed = textureoffset << (FRACBITS - 3); // each unit is 1/8th
					floor->floordestheight = 
						(sec->floorheight >> FRACBITS) + rowoffset;
				}
			}
		}
			break;
		case 221: // Focus on point
		{
			// Find object with angle matching tag
			uint8_t tag = P_GetLineTag(line);
			const side_t *side = &sides[line->sidenum[0]];
			int16_t textureoffset = side->textureoffset & 0xfff;
	    	textureoffset <<= 4; // sign extend
    	  	textureoffset >>= 4; // sign extend
			int16_t rowoffset = (side->textureoffset & 0xf000) | ((unsigned)side->rowoffset << 4);
      		rowoffset >>= 4; // sign extend

			if (rowoffset == 0)
			{
				camBossMobj = NULL;
				camBossMobjCounter = 0;
				break;
			}

			for (mobj_t *node = mobjhead.next; node != (void*)&mobjhead; node = node->next)
			{
				if (node->type == MT_ALTVIEWMAN && node->angle / ANGLE_1 == tag)
				{
					camBossMobj = node;

					if (rowoffset > 0)
						camBossMobjCounter = rowoffset;
					break;
				}
			}
			break;
		}
		case 222: // Move ceiling according to front texture offsets
		{
			const side_t *side = &sides[line->sidenum[0]];
			int16_t textureoffset = side->textureoffset & 0xfff;
	    	textureoffset <<= 4; // sign extend
    	  	textureoffset >>= 4; // sign extend
			int16_t rowoffset = (side->textureoffset & 0xf000) | ((unsigned)side->rowoffset << 4);
      		rowoffset >>= 4; // sign extend

			if (line->flags & ML_NOCLIMB)
			{
				// Instant
				int secnum = -1;
				while ((secnum = P_FindSectorFromLineTag(line, secnum)) >= 0)
				{
					sector_t *sec = I_TO_SEC(secnum);
					sec->ceilingheight += textureoffset << FRACBITS;
					P_ChangeSector(sec, false);
				}
			}
			else
			{
				// Initiate movement
				int secnum = -1;
				while ((secnum = P_FindSectorFromLineTag(line, secnum)) >= 0)
				{
					sector_t *sec = I_TO_SEC(secnum);
					if (sec->specialdata)
						continue;

					ceiling_t *ceiling = Z_Calloc(sizeof(*ceiling), PU_LEVSPEC);
					P_AddThinker (&ceiling->thinker);
					sec->specialdata = LPTR_TO_SPTR_NN(ceiling);
					ceiling->thinker.function = T_MoveCeiling;
					ceiling->type = raiseCeiling;
					ceiling->crush = true;
//					ceiling->dontChangeSector = false;

					ceiling->direction = rowoffset > 0 ? 1 : -1;
					ceiling->sector = sec;
					ceiling->upspeed = ceiling->downspeed = textureoffset << (FRACBITS - 3); // each unit is 1/8th
					ceiling->topheight = ceiling->bottomheight = 
						(sec->floorheight >> FRACBITS) + rowoffset;
				}
			}
		}
			break;
	}
}

// Only players can trigger linedef executors... this is going to come back to bite me, isn't it?
// Why yes, yes it will. When we implement executors calling other executors.. which will shall do so now.
void P_LinedefExecute(uint8_t tag, player_t *player, sector_t *caller)
{
	if (player && player->playerstate != PST_LIVE)
		return;

	// Find linedef with this tag that is an executor linedef.
	int liStart = -1;
	VINT li;
	while ((li = P_FindNextLineWithTag(tag, &liStart)) != -1)
	{
		line_t *line = &lines[li];
		uint8_t special = P_GetLineSpecial(line);

		// Ten options for linedef execution. (Conversion: v2.2 special - 70)
		if (special < 230
			|| special > 239)
			continue;

//		CONS_Printf("P_LinedefExecute: l: %d tag %d\n", li, caller->tag);

		if (special == 230 // Continuous
//			|| special == 231 // Each Time
			|| special == 232) // Once
		{
			// Traverse the linedefs, finding other linedefs that belong to the same sector
			VINT ctrlSector = LD_IFRONTSECTOR(line);
			int16_t start = -1;
			while ((start = P_FindNextSectorLine(ctrlSector, start)) >= 0)
			{
				line_t *ld = &lines[start];
				P_ProcessLineSpecial(ld, player ? player->mo : NULL, caller);
			}
		}

		if (special == 232) // Only execute once
			P_SetHasSpecialOrTag(li, false);
	}
}

/*
===============================================================================
=
= P_PlayerInSpecialSector
=
= Called every tic frame that the player origin is in a special sector
=
===============================================================================
*/

static void P_PlayerOnSpecial3DFloor(player_t *player, sector_t *originalSector)
{
	sector_t *fofsec;

	if (originalSector->fofsec < 0)
		return;

	fofsec = I_TO_SEC(originalSector->fofsec);

	switch (fofsec->special)
	{
		case 255: // ignore
			break;
		case 1: // Clear the map
			P_DoPlayerExit(player);
			break;
		case 2:
			if (player->mo->z == fofsec->ceilingheight && !fofsec->specialdata)
				P_DoPlayerExit(player);
			break;
		case 4: // Damage (electrical)
			if (player->mo->z == fofsec->ceilingheight)
				P_DamageMobj(player->mo, NULL, NULL, 1);
			break;
		case 5: // Brambles
			if (player->mo->z == fofsec->ceilingheight)
				P_DamageMobj(player->mo, NULL, NULL, 5);
			break;
		case 6:
			if (player->mo->z == fofsec->ceilingheight)
				P_DamageMobj(player->mo, NULL, NULL, 10000);
			break;
		case 64: // Linedef Executor: entered a sector
			P_LinedefExecute(fofsec->tag, player, fofsec);
			break;
		case 80: // Linedef Executor: on floor touch
			if (player->mo->z == fofsec->ceilingheight
				|| ((player->pflags & PF_VERTICALFLIP) && player->mo->z + (player->mo->theight << FRACBITS) == fofsec->floorheight))
				P_LinedefExecute(fofsec->tag, player, fofsec);
			break;
			
		default:
			break;
	};
}

void P_PlayerInSpecialSector (player_t *player)
{
	sector_t	*sector = SS_SECTOR(player->mo->isubsector);

	P_PlayerOnSpecial3DFloor(player, sector);
		
	switch (sector->special)
	{
		case 255: // ignore
			break;
		case 1: // Clear the map
			P_DoPlayerExit(player);
			break;
		case 2:
			if (player->mo->z <= sector->floorheight && !sector->specialdata)
				P_DoPlayerExit(player);
			break;
		case 4: // Damage (electrical)
			if (player->mo->z <= sector->floorheight)
				P_DamageMobj(player->mo, NULL, NULL, 1);
			break;
		case 5: // Brambles
			if (player->mo->z <= sector->floorheight)
				P_DamageMobj(player->mo, NULL, NULL, 5);
			break;
		case 6:
			if (player->mo->z <= sector->floorheight)
				P_DamageMobj(player->mo, NULL, NULL, 10000);
			break;
		case 64: // Linedef Executor: entered a sector
			P_LinedefExecute(sector->tag, player, sector);
			break;
		case 80: // Linedef Executor: on floor touch
			if (player->mo->z <= sector->floorheight
				|| ((player->pflags & PF_VERTICALFLIP) && player->mo->z + (player->mo->theight << FRACBITS) >= sector->ceilingheight))
				P_LinedefExecute(sector->tag, player, sector);
			break;
			
		default:
			break;
	};
}


/*
===============================================================================
=
= P_UpdateSpecials
=
= Animate planes, scroll walls, etc
===============================================================================
*/

void P_UpdateSpecials (int8_t numframes)
{
	anim_t	*anim;
	int		i;
	line_t	*line;
	
	/* */
	/*	ANIMATE FLATS AND TEXTURES GLOBALY */
	/* */
	if (! (ticon&3) )
	{
		for (anim = anims ; anim < lastanim ; anim++)
		{
			int pic;

			anim->current++;
			if (anim->current < 0)
				anim->current = 0;
			if (anim->current >= anim->numpics)
				anim->current = 0;

			pic = anim->basepic + anim->current;
			for (i = 0; i < anim->numpics; i++)
			{
				if (anim->istexture)
					texturetranslation[anim->basepic+i] = pic;
				else
					flattranslation[anim->basepic+i] = pic;

				pic++;
				if (pic > anim->picnum)
					pic -= anim->numpics;
			}
		}
	}
	
	/* */
	/*	ANIMATE LINE SPECIALS */
	/* */
	for (i = 0; i < numlineanimspecials; i++)
	{
		side_t *side;
		int16_t textureoffset, rowoffset;
		line = &lines[linespeciallist[i]];
		side = &sides[line->sidenum[0]];
		switch(P_GetLineSpecial(line))
		{
			case 48:	/* EFFECT FIRSTCOL SCROLL + */
				// 12-bit texture offset + 4-bit rowoffset
				textureoffset = side->textureoffset;
				rowoffset = textureoffset & 0xf000;
				textureoffset <<= 4;
				textureoffset += numframes<<4;
				textureoffset >>= 4;
				textureoffset |= rowoffset;
				side->textureoffset = textureoffset;
				break;

			case 142:	/* MODERATE VERT SCROLL */
				// 12-bit texture offset + 4-bit rowoffset
				textureoffset = side->textureoffset;
				rowoffset = ((textureoffset & 0xf000)>>4) | side->rowoffset;
				rowoffset -= numframes * 3;
				side->rowoffset = rowoffset & 0xff;
				side->textureoffset = (textureoffset & 0xfff) | (rowoffset & 0xf00);
				break;
		}
	}	
}

/*
==============================================================================

							SPECIAL SPAWNING

==============================================================================
*/
/*
================================================================================
= P_SpawnSpecials
=
= After the map has been loaded, scan for specials that
= spawn thinkers
=
===============================================================================
*/

//////////////////////////////////////////////////
// T_BounceCheese ////////////////////////////////
//////////////////////////////////////////////////
// Bounces a floating cheese
// Fun fact: I originally called it this
// because the floating block in Labyrinth Zone
// looks like a giant block of swiss cheese. :)

void T_BounceCheese(bouncecheese_t *bouncer)
{
	fixed_t sectorheight;
	fixed_t halfheight;
	fixed_t waterheight;
	fixed_t floorheight;
	sector_t *actionsector;
	boolean remove;

	actionsector = bouncer->targetSector;

	sectorheight = D_abs(bouncer->fofSector->ceilingheight - bouncer->fofSector->floorheight);
	halfheight = sectorheight >> 1;

	waterheight = bouncer->watersec->ceilingheight;

	floorheight = bouncer->targetSector->floorheight;

	remove = false;

	// Water level is up to the ceiling.
	if (waterheight > bouncer->fofSector->ceilingheight - halfheight && bouncer->fofSector->ceilingheight >= actionsector->ceilingheight) // Tails 01-08-2004
	{
		bouncer->fofSector->ceilingheight = actionsector->ceilingheight;
		bouncer->fofSector->floorheight = actionsector->ceilingheight - sectorheight;
		remove = true;
	}
	// Water level is too shallow.
	else if (waterheight < bouncer->fofSector->floorheight + halfheight && bouncer->fofSector->floorheight <= floorheight)
	{
		bouncer->fofSector->ceilingheight = floorheight + sectorheight;
		bouncer->fofSector->floorheight = floorheight;
		remove = true;
	}
	else
	{
		bouncer->ceilingwasheight = waterheight + halfheight;
		bouncer->floorwasheight = waterheight - halfheight;
	}

	if (remove)
	{
		T_MovePlane(bouncer->fofSector, 0, bouncer->fofSector->ceilingheight, false, true, -1); // update things on ceiling
		T_MovePlane(bouncer->fofSector, 0, bouncer->fofSector->floorheight, false, false, -1); // update things on floor
		P_ChangeSector(actionsector, false);
		P_RemoveThinker(&bouncer->thinker); // remove bouncer from actives
		return;
	}

	if (bouncer->speed >= 0) // move floor first to fix height desync and any bizarre bugs following that
	{
		T_MovePlane(bouncer->fofSector, bouncer->speed/2, bouncer->fofSector->floorheight - 70*FRACUNIT,
				false, false, -1); // move floor
		T_MovePlane(bouncer->fofSector, bouncer->speed/2, bouncer->fofSector->ceilingheight -
				70*FRACUNIT, false, true, -1); // move ceiling
	}
	else
	{
		T_MovePlane(bouncer->fofSector, bouncer->speed/2, bouncer->fofSector->ceilingheight -
				70*FRACUNIT, false, true, -1); // move ceiling
		T_MovePlane(bouncer->fofSector, bouncer->speed/2, bouncer->fofSector->floorheight - 70*FRACUNIT,
				false, false, -1); // move floor
	}

	P_ChangeSector(actionsector, false);

	if ((bouncer->fofSector->ceilingheight < bouncer->ceilingwasheight && !bouncer->low) // Down
		|| (bouncer->fofSector->ceilingheight > bouncer->ceilingwasheight && bouncer->low)) // Up
	{
		if (D_abs(bouncer->speed) < 6*FRACUNIT)
			bouncer->speed -= bouncer->speed/3;
		else
			bouncer->speed -= bouncer->speed/2;

		bouncer->low = !bouncer->low;
		if (D_abs(bouncer->speed) > 6*FRACUNIT)
		{
//			mobj_t *mp = (void *)&actionsector->soundorg;
//			actionsector->soundorg.z = bouncer->sector->floorheight;
//			S_StartSound(mp, sfx_splash);
		}
	}

	if (bouncer->fofSector->ceilingheight < bouncer->ceilingwasheight) // Down
	{
		bouncer->speed -= bouncer->distance;
	}
	else if (bouncer->fofSector->ceilingheight > bouncer->ceilingwasheight) // Up
	{
		bouncer->speed += GRAVITY/2;
	}

	if (D_abs(bouncer->speed) < 2*FRACUNIT
		&& D_abs(bouncer->fofSector->ceilingheight-bouncer->ceilingwasheight) < FRACUNIT/4)
	{
		bouncer->fofSector->floorheight = bouncer->floorwasheight;
		bouncer->fofSector->ceilingheight = bouncer->ceilingwasheight;
		T_MovePlane(bouncer->fofSector, 0, bouncer->fofSector->ceilingheight, false, true, -1); // update things on ceiling
		T_MovePlane(bouncer->fofSector, 0, bouncer->fofSector->floorheight, false, false, -1); // update things on floor
		P_ChangeSector(actionsector, false);
		P_RemoveThinker(&bouncer->thinker);    // remove bouncer from actives
	}

	if (bouncer->distance > 0)
		bouncer->distance--;
}

void EV_BounceSector(sector_t *fofsec, sector_t *targetSector, fixed_t momz, VINT heightsec)
{
	bouncecheese_t *bouncer;

	thinker_t *currentthinker = thinkercap.next;
	while (currentthinker != &thinkercap)
	{
		if (currentthinker->function == T_BounceCheese)
		{
			bouncer = (bouncecheese_t*)currentthinker;
			if (bouncer->fofSector == fofsec)
				return; // This sector already has an active T_BounceCheese, so don't go any further.
		}
		currentthinker = currentthinker->next;
	}

	if (currentthinker == &thinkercap) // Hit the end of the list, so this is totally new
	{
		bouncer = Z_Malloc(sizeof(*bouncer), PU_LEVSPEC);
		P_AddThinker(&bouncer->thinker);
		bouncer->thinker.function = T_BounceCheese;

		bouncer->targetSector = targetSector;
		bouncer->fofSector = fofsec;
		bouncer->watersec = I_TO_SEC(heightsec);
		bouncer->speed = momz >> 1;
		bouncer->distance = FRACUNIT;
		bouncer->low = true;
	}
}

#define CARRYFACTOR (10240 + 4096 + 224) // Pretty darn close...
#define SCROLL_SHIFT 4

void T_ScrollFlat (scrollflat_t *scrollflat)
{
	const mapvertex_t *v1 = &vertexes[scrollflat->ctrlLine->v1];
	const mapvertex_t *v2 = &vertexes[scrollflat->ctrlLine->v2];

	fixed_t ldx = (v2->x - v1->x);
	fixed_t ldy = (v2->y - v1->y);

	for (int i = 0; i < scrollflat->numsectors; i++)
	{
		sector_t *sec = I_TO_SEC(scrollflat->sectors[i]);

		uint8_t xoff = sec->floor_xoffs >> 8;
		uint8_t yoff = sec->floor_xoffs & 0xff;

		xoff += ldx;
		yoff += ldy;

		sec->floor_xoffs = (xoff << 8) | yoff;

		if (scrollflat->carry)
		{
			sec->specialdata = LPTR_TO_SPTR_NN(scrollflat);
			sec->flags |= SF_CONVEYOR;
		}
	}
}

static void P_StartScrollFlat(line_t *line, VINT isector, boolean carry)
{
	thinker_t	*currentthinker;
	uint8_t tag = P_GetLineTag(line);

	if (tag == 0)
		return;
	
	currentthinker = thinkercap.next;
	while (currentthinker != &thinkercap)
	{
		if (currentthinker->function == T_ScrollFlat)
		{
			scrollflat_t *sf = (scrollflat_t*)currentthinker;
			uint8_t stTag = P_GetLineTag(sf->ctrlLine);

			if (stTag == tag && sf->ctrlLine == line)
			{
				if (sf->numsectors > sf->totalSectors) // Bounds check
					return;

				sf->sectors[sf->numsectors] = isector;
				sf->numsectors++;
				return;
			}
		}
		currentthinker = currentthinker->next;
	}

	if (currentthinker == &thinkercap) // Hit the end of the list, so this is totally new
	{
		int numScrollflatSectors = 0;
		for (int i = 0; i < numsectors; i++)
		{
			if (I_TO_SEC(i)->tag == tag)
				numScrollflatSectors++;
		}

		scrollflat_t *scrollflat = Z_Malloc (sizeof(*scrollflat)+(sizeof(VINT)*numScrollflatSectors), PU_LEVSPEC);
		P_AddThinker (&scrollflat->thinker);
		scrollflat->thinker.function = T_ScrollFlat;
		scrollflat->ctrlLine = line;
		scrollflat->sectors = (VINT*)((uint8_t*)scrollflat + sizeof(*scrollflat));
		
		scrollflat->sectors[0] = isector;
		scrollflat->numsectors = 1;
		scrollflat->totalSectors = numScrollflatSectors;
		scrollflat->carry = carry;
	}
}

lightningspawn_t *lightningSpawner = NULL;

void P_SpawnLightningStrike(boolean close)
{
	// Amazing logic goes here.
	if (!lightningSpawner)
		return; // Should never happen

	lightningSpawner->timer = 0;

	for (int i = 0; i < lightningSpawner->numsectors*2; i += 2)
	{
		sector_t *sec = I_TO_SEC(lightningSpawner->sectorData[i]);
		sec->lightlevel = (uint8_t)(close ? 255 : (255 + (int32_t)sec->lightlevel) >> 1);
	}
}

void T_LightningFade(lightningspawn_t *spawner)
{
	if (spawner->timer < 0)
		return;

	for (int i = 0; i < spawner->numsectors*2;)
	{
		sector_t *sec = I_TO_SEC(spawner->sectorData[i++]);
		const VINT origLightLevel = spawner->sectorData[i++];

		sec->lightlevel -= 6;

		if (sec->lightlevel <= origLightLevel)
		{
			sec->lightlevel = (uint8_t)origLightLevel;
			spawner->timer++;
		}
	}

	if (spawner->timer >= spawner->numsectors)
		spawner->timer = -1;
}

static void P_InitLightning()
{
	VINT numskysectors = 0;

	for (int i = 0; i < numsectors; i++)
	{
		if (I_TO_SEC(i)->ceilingpic == (uint8_t)-1)
			numskysectors++;
	}

	lightningspawn_t *spawner = Z_Malloc(sizeof(*spawner) + (sizeof(VINT) * 2 * numskysectors), PU_LEVSPEC);
	spawner->sectorData = (VINT*)((uint8_t*)spawner + sizeof(*spawner));
	spawner->numsectors = numskysectors;
	spawner->thinker.function = T_LightningFade;
	P_AddThinker(&spawner->thinker);
	spawner->timer = -1;

	int count = 0;
	for (int i = 0; i < numsectors; i++)
	{
		if (I_TO_SEC(i)->ceilingpic == (uint8_t)-1)
		{
			spawner->sectorData[count++] = i;
			spawner->sectorData[count++] = I_TO_SEC(i)->lightlevel;
		}
	}

	lightningSpawner = spawner;
}

void T_ScrollTex (scrolltex_t *scrolltex)
{
	fixed_t xSpeed = scrolltex->ctrlSector->floorheight >> FRACBITS;
	fixed_t ySpeed = scrolltex->ctrlSector->ceilingheight >> FRACBITS;

	for (int i = 0; i < scrolltex->numlines; i++)
	{
		int16_t textureoffset, rowoffset;
		line_t *line = &lines[scrolltex->lines[i]];
		side_t *side = &sides[line->sidenum[0]];

		if (xSpeed != 0)
		{
			textureoffset = side->textureoffset;
			rowoffset = textureoffset & 0xf000;
			textureoffset <<= 4;
			textureoffset += xSpeed<<4;
			textureoffset >>= 4;
			textureoffset |= rowoffset;
			side->textureoffset = textureoffset;
		}
		if (ySpeed != 0)
		{
			textureoffset = side->textureoffset;
			rowoffset = ((textureoffset & 0xf000)>>4) | side->rowoffset;
			rowoffset += ySpeed;
			side->rowoffset = rowoffset & 0xff;
			side->textureoffset = (textureoffset & 0xfff) | (rowoffset & 0xf00);
		}
	}
}

static void P_StartScrollTex(line_t *line)
{
	thinker_t	*currentthinker;
	uint8_t tag = P_GetLineTag(line);

	if (tag == 0)
		return;
	
	currentthinker = thinkercap.next;
	while (currentthinker != &thinkercap)
	{
		if (currentthinker->function == T_ScrollTex)
		{
			scrolltex_t *st = (scrolltex_t*)currentthinker;
			uint8_t stTag = P_GetLineTag(&lines[st->lines[0]]);

			if (stTag == tag)
			{
				if (st->numlines >= st->totalLines) // Bounds check
					return;

				st->lines[st->numlines++] = (VINT)(line-lines);
				return;
			}
		}
		currentthinker = currentthinker->next;
	}

	if (currentthinker == &thinkercap) // Hit the end of the list, so this is totally new
	{
		sector_t *paramSector;
		int s = P_FindSectorFromLineTagNum(tag, -1);

		if (s < 0)
			return;

		paramSector = I_TO_SEC(s);

		int numScrolltexLines = 0;
		for (int i = 0; i < numlines; i++)
		{
			if (P_GetLineSpecial(&lines[i]) == 249
				&& P_GetLineTag(&lines[i]) == P_GetLineTag(line))
				numScrolltexLines++;
		}

		scrolltex_t *scrolltex = Z_Malloc (sizeof(*scrolltex)+(sizeof(VINT)*numScrolltexLines), PU_LEVSPEC);
		P_AddThinker (&scrolltex->thinker);
		scrolltex->thinker.function = T_ScrollTex;
		scrolltex->ctrlSector = paramSector;
		scrolltex->lines = (VINT*)((uint8_t*)scrolltex + sizeof(*scrolltex));
		scrolltex->lines[0] = (VINT)(line - lines);
		scrolltex->totalLines = numScrolltexLines;
		scrolltex->numlines = 1;
	}
}

typedef struct
{
	thinker_t thinker;
	sector_t *sector;
	fixed_t extraspeed; //For dynamically sinking platform
	int16_t tag;
	int16_t ceilingbottom;
	int16_t ceilingtop;
	int16_t basespeed;

	int16_t *sectors;
	int16_t numsectors;

	uint8_t shaketimer; //For dynamically sinking platform
	uint8_t flags;
} raise_t;

void T_RaiseSector (raise_t *raise)
{
	player_t *playerOnMe = NULL;

	for (int i = 0; i < MAXPLAYERS; i++)
	{
		if (!playeringame[i])
			continue;

		player_t *player = &players[i];

		// First the easy case
		for (int k = 0; k < raise->numsectors; k++)
		{
			if (subsectors[player->mo->isubsector].isector == raise->sectors[k]
				&& player->mo->z == raise->sector->ceilingheight)
			{
				playerOnMe = player;
				goto playerIsOnMe;
			}
		}

		for (int j = 0; j < player->num_touching_sectors; j++)
		{
			for (int k = 0; k < raise->numsectors; k++)
			{
				if (player->touching_sectorlist[j] == raise->sectors[k])
				{
					if (player->mo->z == raise->sector->ceilingheight)
					{
						playerOnMe = player;
						goto playerIsOnMe;
					}
				}
			}
		}
	}

playerIsOnMe:
; // Keep the compiler happy
	// Player is standing on the FOF
	VINT direction = playerOnMe ? 1 : -1;
	fixed_t ceilingdestination = direction > 0 ? raise->ceilingtop << FRACBITS : raise->ceilingbottom << FRACBITS;
	fixed_t floordestination = ceilingdestination - (raise->sector->ceilingheight - raise->sector->floorheight);

	if ((direction > 0 && raise->sector->ceilingheight >= ceilingdestination)
		|| (direction < 0 && raise->sector->ceilingheight <= ceilingdestination))
	{
		raise->sector->floorheight = floordestination;
		raise->sector->ceilingheight = ceilingdestination;
		return;
	}

	fixed_t origspeed = raise->basespeed << FRACBITS;
	if (!playerOnMe)
		origspeed >>= 1;

	// Speed up as you get closer to the middle, then slow down again
	fixed_t distToNearestEndpoint = D_min(raise->sector->ceilingheight - (raise->ceilingbottom << FRACBITS), (raise->ceilingtop << FRACBITS) - raise->sector->ceilingheight);
	fixed_t speed = FixedMul(origspeed, FixedDiv(distToNearestEndpoint, (raise->ceilingtop - raise->ceilingbottom) << (FRACBITS-5)));

	if (speed <= origspeed >> 4)
		speed = origspeed >> 4;
	else if (speed > origspeed)
		speed = origspeed;

	result_e res = T_MovePlane(raise->sector, speed, ceilingdestination, true, 1, direction);

	if (res == ok || res == pastdest)
		T_MovePlane(raise->sector, speed, floordestination, true, 0, direction);

	if (playerOnMe)
		playerOnMe->mo->z = playerOnMe->mo->floorz = raise->sector->ceilingheight;

	for (int k = 0; k < raise->numsectors; k++)
		P_ChangeSector(I_TO_SEC(raise->sectors[k]), true);
}

static void P_AddRaiseThinker(VINT fofSector, line_t *fofLine)
{
	mapvertex_t *v1 = &vertexes[fofLine->v1];
	mapvertex_t *v2 = &vertexes[fofLine->v2];
	int16_t numTaggedSectors = 0;

	for (int s = -1; (s = P_FindSectorFromLineTag(fofLine,s)) >= 0;)
		numTaggedSectors++;

	if (numTaggedSectors <= 0)
		return; // Gotta have target sectors...

	raise_t *raise = Z_Malloc (sizeof(*raise)+(sizeof(int16_t)*numTaggedSectors), PU_LEVSPEC);
	P_AddThinker(&raise->thinker);
	raise->thinker.function = T_RaiseSector;

	raise->tag = P_GetLineTag(fofLine);
	raise->sector = I_TO_SEC(fofSector);
	raise->ceilingtop = I_TO_SEC(P_FindHighestCeilingSurrounding(fofSector))->ceilingheight >> FRACBITS;
	raise->ceilingbottom = I_TO_SEC(P_FindLowestCeilingSurrounding(fofSector))->ceilingheight >> FRACBITS;
	raise->basespeed = P_AproxDistance(v2->x - v1->x, v2->y - v1->y) >> 2;
	raise->sectors = (int16_t*)((uint8_t*)raise + sizeof(*raise));
	raise->numsectors = 0;

	for (int s = -1; (s = P_FindSectorFromLineTag(fofLine,s)) >= 0;)
		raise->sectors[raise->numsectors++] = s;
}

void P_SSNMaceRotate(swingmace_t *sm)
{
	// Always update movedir to prevent desync. But do we really have to?
	// Can't this be calculated from leveltime? Why yes, yes it can...
	int16_t curPos = (sm->mspeed * (leveltime + sm->mphase)) & FINEMASK;

	vector3_t axis;
	vector3_t rotationDir;

	if (sm->swingSpeed)
	{
		angle_t swingmag = FixedMul(finecosine(curPos), sm->swingSpeed << 8 << FRACBITS);
		angle_t fa = swingmag >> ANGLETOFINESHIFT;
//		CONS_Printf("fa: %d", fa);
		curPos = fa;
	}

//		CONS_Printf("a: %d, %d, %d; r: %d, %d, %d", sm->nv.x, sm->nv.y, sm->nv.z, sm->rotation.x, sm->rotation.y, sm->rotation.z);

	// int8_t to fixed_t
	axis.x = (fixed_t)sm->nv.x << 9;
	axis.y = (fixed_t)sm->nv.y << 9;
	axis.z = (fixed_t)sm->nv.z << 9;
	rotationDir.x = (fixed_t)sm->rotation.x << 9;
	rotationDir.y = (fixed_t)sm->rotation.y << 9;
	rotationDir.z = (fixed_t)sm->rotation.z << 9;

	vector4_t rotVec = FV3_RotateVector(&rotationDir, &axis, curPos);

//	CONS_Printf("%d, %d, %d", rotVec.x, rotVec.y, rotVec.z);

	int16_t msublinks = sm->msublinks;
	int16_t mnumchain = sm->macechain.numchain;
	fixed_t dist = sm->macechain.interval * msublinks;

	ringmobj_t *link = sm->macechain.chain;
	fixed_t distAccum = dist;
	if (sm->flags & TMM_MACELINKS)
	{
		while (mnumchain > 0)
		{
			P_UnsetThingPosition((mobj_t*)link);
			link->x = sm->macechain.x + ((rotVec.x * distAccum) >> FRACBITS);
			link->y = sm->macechain.y + ((rotVec.y * distAccum) >> FRACBITS);
			link->z = sm->macechain.z + ((rotVec.z * distAccum) >> FRACBITS);
			link->z -= (mobjinfo[link->type].height >> FRACBITS) >> 1;
			P_SetThingPosition2((mobj_t*)link, R_PointInSubsector2(link->x << FRACBITS, link->y << FRACBITS));
			link++;
			mnumchain--;
			distAccum += sm->macechain.interval;
		}
	}
	else
	{
		while (mnumchain > 0)
		{
			//		P_UnsetThingPosition((mobj_t*)link);
			link->x = sm->macechain.x + ((rotVec.x * distAccum) >> FRACBITS);
			link->y = sm->macechain.y + ((rotVec.y * distAccum) >> FRACBITS);
			link->z = sm->macechain.z + ((rotVec.z * distAccum) >> FRACBITS);
			link->z -= (mobjinfo[link->type].height >> FRACBITS) >> 2;
	//		P_SetThingPosition((mobj_t*)link);
			link++;
			mnumchain--;
			distAccum += sm->macechain.interval;
		}
	}

	dist = distAccum - sm->macechain.interval;

	if (sm->flags & TMM_DOUBLESIZE)
	{
		if (sm->macechain.maceball->type == MT_BIGMACE)
			dist += sm->macechain.interval;
		dist += mobjinfo[sm->macechain.maceball->type].radius >> FRACBITS;
	}
	else if (sm->flags & TMM_MACELINKS)
		dist += (mobjinfo[sm->macechain.maceball->type].radius >> FRACBITS) * 3;
	else
		dist += mobjinfo[sm->macechain.maceball->type].radius >> FRACBITS;

	P_UnsetThingPosition((mobj_t*)sm->macechain.maceball);
	sm->macechain.maceball->x = sm->macechain.x + ((rotVec.x * dist) >> FRACBITS);
	sm->macechain.maceball->y = sm->macechain.y + ((rotVec.y * dist) >> FRACBITS);
	sm->macechain.maceball->z = sm->macechain.z + ((rotVec.z * dist) >> FRACBITS);
	sm->macechain.maceball->z -= (mobjinfo[sm->macechain.maceball->type].height >> FRACBITS) >> 1;
	P_SetThingPosition2((mobj_t*)sm->macechain.maceball, R_PointInSubsector2(sm->macechain.maceball->x << FRACBITS, sm->macechain.maceball->y << FRACBITS));

	// Is a player attached?
	for (int count = 0; count < MAXPLAYERS; count++)
	{
		if (!playeringame[count])
			continue;

		const player_t *player = &players[count];

		if ((player->pflags & PF_MACESPIN)
			&& player->mo->target == (mobj_t*)sm->macechain.maceball)
		{
			vector3_t newPos;
			newPos.x = (sm->macechain.x << FRACBITS) + (rotVec.x * dist);
			newPos.y = (sm->macechain.y << FRACBITS) + (rotVec.y * dist);
			newPos.z = (sm->macechain.z << FRACBITS) + (rotVec.z * dist) - (P_GetPlayerSpinHeight() >> 1) - (P_GetPlayerSpinHeight() >> 2);

			player->mo->momx = (newPos.x - player->mo->x) << 1;
			player->mo->momy = (newPos.y - player->mo->y) << 1;
			player->mo->momz = (newPos.z - player->mo->z) << 1;
			P_UnsetThingPosition(player->mo);
			player->mo->x = newPos.x;
			player->mo->y = newPos.y;
			player->mo->z = newPos.z;
			P_SetThingPosition(player->mo);
		}
	}
}

void T_SwingMace(swingmace_t *sm)
{
	boolean nearSomebody = false;

	// Are you near a player? Otherwise don't bother
	for (int i = 0; i < MAXPLAYERS; i++)
	{
		if (!playeringame[i])
			continue;

		const mobj_t *playermo = players[i].mo;

		if (D_abs((sm->macechain.x << FRACBITS) - playermo->x) > 2048*FRACUNIT
			|| D_abs((sm->macechain.y << FRACBITS) - playermo->y) > 2048*FRACUNIT)
			continue;

		nearSomebody = true;
	}

	if (nearSomebody)
		P_SSNMaceRotate(sm);
}

static swingmace_t *cursorMace = NULL;
void P_PreallocateMaces(int numMaces)
{
	if (numMaces <= 0)
	{
		cursorMace = NULL;
		return;
	}

	size_t allocSize = sizeof(swingmace_t) * numMaces;
	cursorMace = Z_Malloc(allocSize, PU_LEVSPEC);
	D_memset(cursorMace, 0, allocSize);
}

static swinghang_t *cursorSwing = NULL;
void P_PreallocateSwings(int numSwings)
{
	if (numSwings <= 0)
	{
		cursorSwing = NULL;
		return;
	}

	size_t allocSize = sizeof(swinghang_t) * numSwings;
	cursorSwing = Z_Malloc(allocSize, PU_LEVSPEC);
	D_memset(cursorSwing, 0, allocSize);
}

/* ================================================================
 * Constant-speed Bezier path following (16.16 fixed-point)
 * ================================================================ */

 typedef struct
 {
	int16_t id;
	int16_t numSegments;
	int16_t startAddr;
 } bezier_header_t;

 typedef struct
 {
	int16_t numPaths;
 } bezier_lump_t;

 bezier_segment_t *GetPathFromLump(bezier_lump_t *lump, int16_t id, int16_t *outSegments)
 {
	int16_t *lumpS = (int16_t*)lump;
	lumpS++;

	bezier_header_t *headers = (bezier_header_t*)lumpS;

	for (int i = 0; i < lump->numPaths; i++)
	{
		if (headers[i].id == id)
		{
			if (outSegments)
				*outSegments = headers[i].numSegments;

			return (bezier_segment_t*)((byte*)lump + headers[i].startAddr);
		}
	}

	I_Error("Bezier path %d not found in lump", id);
	return NULL;
 }

typedef struct
{
	fixed_t x0, y0; // start
    fixed_t x1, y1; // control 1
    fixed_t x2, y2; // control 2
    fixed_t x3, y3; // end
} bezier_segment32_t;

// Evaluate cubic Bezier at parameter t (0..FRACUNIT)
static void BezierEval(const bezier_segment_t *s, fixed_t t,
                       fixed_t *outx, fixed_t *outy)
{
	bezier_segment32_t fullSeg;
	fullSeg.x0 = s->x0 << FRACBITS;
	fullSeg.y0 = s->y0 << FRACBITS;
	fullSeg.x1 = s->x1 << FRACBITS;
	fullSeg.y1 = s->y1 << FRACBITS;
	fullSeg.x2 = s->x2 << FRACBITS;
	fullSeg.y2 = s->y2 << FRACBITS;
	fullSeg.x3 = s->x3 << FRACBITS;
	fullSeg.y3 = s->y3 << FRACBITS;

    fixed_t u  = FRACUNIT - t;
    fixed_t tt = FixedMul(t, t);
    fixed_t uu = FixedMul(u, u);
    fixed_t uuu = FixedMul(uu, u);
    fixed_t ttt = FixedMul(tt, t);
    fixed_t uut = FixedMul(uu, t);
    fixed_t utt = FixedMul(u, tt);

    /* B(t) = (1-t)^3 P0 + 3(1-t)^2 t P1 + 3(1-t)t^2 P2 + t^3 P3 */
    *outx = FixedMul(uuu, fullSeg.x0)
          + FixedMul(3 * uut, fullSeg.x1)
          + FixedMul(3 * utt, fullSeg.x2)
          + FixedMul(ttt, fullSeg.x3);

    *outy = FixedMul(uuu, fullSeg.y0)
          + FixedMul(3 * uut, fullSeg.y1)
          + FixedMul(3 * utt, fullSeg.y2)
          + FixedMul(ttt, fullSeg.y3);
}

// Approximate derivative magnitude ||B'(t)|| (for length estimation)
static fixed_t BezierSpeed(const bezier_segment_t *s, fixed_t t)
{
    // B'(t) = 3(1-t)^2 (P1-P0) + 6(1-t)t (P2-P1) + 3t^2 (P3-P2)
    fixed_t u = FRACUNIT - t;
    fixed_t uu = FixedMul(u, u);
    fixed_t tt = FixedMul(t, t);
    fixed_t ut = FixedMul(u, t);

    fixed_t dx = FixedMul(3 * uu, (s->x1 << FRACBITS) - (s->x0 << FRACBITS))
               + FixedMul(6 * ut, (s->x2 << FRACBITS) - (s->x1 << FRACBITS))
               + FixedMul(3 * tt, (s->x3 << FRACBITS) - (s->x2 << FRACBITS));

    fixed_t dy = FixedMul(3 * uu, (s->y1 << FRACBITS) - (s->y0 << FRACBITS))
               + FixedMul(6 * ut, (s->y2 << FRACBITS) - (s->y1 << FRACBITS))
               + FixedMul(3 * tt, (s->y3 << FRACBITS) - (s->y2 << FRACBITS));

    return FixedSqrt(FixedMul(dx, dx) + FixedMul(dy, dy));
}

/* ----------------------------------------------------------------
 * Preprocessing – build the LUT
 * ---------------------------------------------------------------- */

void Bezier_ProcessSegment(bezier_processed_t *out, const bezier_segment_t *in)
{
    int i;
    fixed_t t, prevx, prevy, x, y, d;
    fixed_t cumulative = 0;

    out->seg = *in;

    // first sample
    BezierEval(in, 0, &prevx, &prevy);
    out->lut[0].t    = 0;
    out->lut[0].dist = 0;
    out->lut[0].x    = prevx >> FRACBITS;
    out->lut[0].y    = prevy >> FRACBITS;

    for (i = 1; i < BEZIER_LUT_SIZE; i++)
    {
        t = (i * FRACUNIT) / (BEZIER_LUT_SIZE - 1);
        BezierEval(in, t, &x, &y);

        d = FixedSqrt(FixedMul(x - prevx, x - prevx) +
                      FixedMul(y - prevy, y - prevy));
        cumulative += d;

        out->lut[i].t = t;
        out->lut[i].dist = cumulative;
        out->lut[i].x = x >> FRACBITS;
        out->lut[i].y = y >> FRACBITS;

        prevx = x;
        prevy = y;
    }

    out->length = cumulative;
}

// Build a complete path from an array of raw segments
bezier_path_t *Bezier_CreatePath(const bezier_segment_t *segs, int count)
{
    bezier_path_t *path;
    int i;

    if (count <= 0 || count > BEZIER_MAX_SEGMENTS)
        return NULL;

    path = Z_Malloc(sizeof(*path), PU_LEVEL);
    path->segs = Z_Malloc(count * sizeof(bezier_processed_t), PU_LEVEL);
    path->num_segs = count;
    path->total_length = 0;

    for (i = 0; i < count; i++)
    {
        Bezier_ProcessSegment(&path->segs[i], &segs[i]);
        path->total_length += path->segs[i].length;
    }
    return path;
}
/*
void Bezier_DestroyPath(bezier_path_t *path)
{
    if (!path)
		return;

    Z_Free(path->segs);
    Z_Free(path);
}*/

/* ----------------------------------------------------------------
 * Lookup: given distance inside one segment -> position
 * ---------------------------------------------------------------- */
static void Bezier_PointAtDistance(const bezier_processed_t *seg,
                                   fixed_t dist,
                                   fixed_t *x, fixed_t *y)
{
    int lo = 0, hi = BEZIER_LUT_SIZE - 1, mid;
    fixed_t frac;
    const bezier_lut_entry_t *a, *b;

    if (dist <= 0)
    {
        *x = seg->lut[0].x << FRACBITS;
        *y = seg->lut[0].y << FRACBITS;
        return;
    }
    if (dist >= seg->length)
    {
        *x = seg->lut[BEZIER_LUT_SIZE-1].x << FRACBITS;
        *y = seg->lut[BEZIER_LUT_SIZE-1].y << FRACBITS;
        return;
    }

    // binary search for the bracketing samples
    while (lo < hi - 1)
    {
        mid = (lo + hi) >> 1;
        if (seg->lut[mid].dist < dist)
            lo = mid;
        else
            hi = mid;
    }

    a = &seg->lut[lo];
    b = &seg->lut[hi];

    if (b->dist == a->dist)
    {
        *x = a->x << FRACBITS;
        *y = a->y << FRACBITS;
        return;
    }

    frac = FixedDiv(dist - a->dist, b->dist - a->dist);
    *x = (a->x << FRACBITS) + FixedMul(frac, (b->x << FRACBITS) - (a->x << FRACBITS));
    *y = (a->y << FRACBITS) + FixedMul(frac, (b->y << FRACBITS) - (a->y << FRACBITS));
}

/* ----------------------------------------------------------------
 * Follower API
 * ---------------------------------------------------------------- */

void Bezier_InitFollower(bezier_follower_t *f, bezier_path_t *path, fixed_t speed)
{
    f->path           = path;
    f->dist_travelled = 0;
    f->current_seg    = 0;
    f->speed          = speed;
    f->active         = true;
}

// Advance the follower by its speed. Returns false when the path ends.
boolean Bezier_UpdateFollower(bezier_follower_t *f, mobj_t *mobj, VINT flags)
{
    fixed_t remaining;
    fixed_t x, y;
	int elapsed;

    if (!f->active || !f->path)
	{
        return false;
	}

	elapsed = leveltime - f->start_tic;

	if (flags & SHF_SINEWAVE)
	{
		if (flags & SHF_REVERSE)
		{
			/* Ping-pong: full round-trip period = 2 * duration */
			int period = f->duration_tics * 2;
			if (period > 0)
			{
				int phase = elapsed % period;
				if (phase < 0)
					phase += period;

				/* Generate a triangular 0 → 1 → 0 wave */
				if (phase <= f->duration_tics)
				{
					/* Going forward */
					elapsed = phase;                    /* 0 … duration */
					f->speed = D_abs(f->speed);         /* positive */
				}
				else
				{
					/* Coming back */
					elapsed = period - phase;           /* duration … 0 */
					f->speed = -D_abs(f->speed);        /* negative */
				}
			}
		}
		else if (flags & SHF_LOOP)
		{
			if (f->duration_tics > 0)
			{
				elapsed %= f->duration_tics;
				if (elapsed < 0)
					elapsed += f->duration_tics;
			}
		}
		else
		{
			/* Once – clamp and possibly deactivate */
			if (elapsed >= f->duration_tics)
			{
				elapsed = f->duration_tics;
				f->dist_travelled = f->path->total_length;
				f->active = false;
			}
			else if (elapsed <= 0)
			{
				elapsed = 0;
				f->dist_travelled = 0;
				f->active = false;
			}
		}

		/* Store for debugging / queries */
		f->elapsed_tics = elapsed;

		/* u ∈ [0 … 1] */
		fixed_t u = FixedDiv(elapsed << FRACBITS,
								f->duration_tics << FRACBITS);

		if (u < 0)        u = 0;
		if (u > FRACUNIT) u = FRACUNIT;

		/* BAM angle = u * π */
		angle_t bam = (angle_t)FixedMul(u, ANG180);
		int fine = bam >> ANGLETOFINESHIFT;
		fixed_t cos_term = finecosine(fine);

		/* Raised-cosine: s = L/2 * (1 - cos(π u)) */
		f->dist_travelled = FixedMul(f->path->total_length >> 1,
										FRACUNIT - cos_term);

		/* Mirror when travelling backwards */
//		if (f->speed < 0)
//			f->dist_travelled = f->path->total_length - f->dist_travelled;
	}
	else
	{
		/* -------------------------------------------------
         * Constant-speed traversal
         * ------------------------------------------------- */

        /* Absolute distance from global time */
        f->dist_travelled = FixedMul(f->speed, (fixed_t)elapsed << FRACBITS);

        /* Optional: if the motion should not always start at distance 0,
           store a start_dist in the follower and do:
           f->dist_travelled = f->start_dist + FixedMul(...)
        */

        if (f->dist_travelled <= 0)
        {
            if (flags & SHF_REVERSE)
            {
                f->speed = -f->speed;
                /* Re-base the timer so the motion stays continuous */
                f->start_tic = leveltime;
                f->dist_travelled = 0;
            }
            else if (flags & SHF_LOOP)
            {
                /* Keep the fractional part after wrapping */
                if (f->path->total_length > 0)
                    f->dist_travelled %= f->path->total_length;
                if (f->dist_travelled < 0)
                    f->dist_travelled += f->path->total_length;
            }
            else
            {
                f->active = false;
                f->dist_travelled = 0;
            }
        }
        else if (f->dist_travelled >= f->path->total_length)
        {
            if (flags & SHF_REVERSE)
            {
                f->speed = -f->speed;
                f->start_tic = leveltime;
                f->dist_travelled = f->path->total_length;
            }
            else if (flags & SHF_LOOP)
            {
                if (f->path->total_length > 0)
                    f->dist_travelled %= f->path->total_length;
            }
            else
            {
                f->active = false;
                f->dist_travelled = f->path->total_length;
            }
        }
	}

    // find which segment we are on
    remaining = f->dist_travelled;
    f->current_seg = 0;
    while (f->current_seg < f->path->num_segs - 1 &&
           remaining >= f->path->segs[f->current_seg].length)
    {
        remaining -= f->path->segs[f->current_seg].length;
        f->current_seg++;
    }

    Bezier_PointAtDistance(&f->path->segs[f->current_seg], remaining, &x, &y);

	P_UnsetThingPosition(mobj);
	mobj->x = x;
	mobj->y = y;
	P_SetThingPosition2(mobj, R_PointInSubsector2(mobj->x, mobj->y));

    return f->active;
}

static void SetPlayerPositionFromSwing(swinghang_t *sh, player_t *player)
{
	/*
	// This could also be processed as 'hit a wall'. What should we do then?
	if (sh->z < (player->mo->floorz >> FRACBITS) + 64)
		sh->z = (player->mo->floorz >> FRACBITS) + 64;
	else if (sh->z > (player->mo->ceilingz >> FRACBITS) - 64)
		sh->z = (player->mo->ceilingz >> FRACBITS) - 64;*/

	vector3_t newPos;
	newPos.x = sh->maceball->x;
	newPos.y = sh->maceball->y;
	newPos.z = sh->maceball->z;
	newPos.z -= (mobjinfo[MT_PLAYER].height) + 8*FRACUNIT;

	P_SetMobjState(player->mo, S_PLAY_HANG);

	player->mo->momx = 0;//(newPos.x - player->mo->x);
	player->mo->momy = 0;//(newPos.y - player->mo->y);
	player->mo->momz = 0;//(newPos.z - player->mo->z) << 1;
	P_UnsetThingPosition(player->mo);
	player->mo->x = newPos.x;
	player->mo->y = newPos.y;
	player->mo->z = newPos.z;
	P_SetThingPosition(player->mo);
}

void T_SwingBezier(swinghang_t *sh)
{
	if (!sh->bezier_follower.active)
		return;

	// Are you near a player? Otherwise don't bother
	boolean nearSomebody = false;
	for (int i = 0; i < MAXPLAYERS; i++)
	{
		if (!playeringame[i])
			continue;

		const mobj_t *playermo = players[i].mo;

		if (D_abs(sh->maceball->x - playermo->x) > 2048*FRACUNIT
			|| D_abs(sh->maceball->y - playermo->y) > 2048*FRACUNIT)
			continue;

		nearSomebody = true;
	}

	if (!nearSomebody)
		return;

	if (!Bezier_UpdateFollower(&sh->bezier_follower, sh->maceball, sh->flags))
	{
//		CONS_Printf("Done!");
		return;
	}

	sh->maceball->z = (sh->z + sh->deltaZ) << FRACBITS;

	boolean controlsPressed = false;
	player_t *player = NULL;

	// Is a player attached?
	for (int count = 0; count < MAXPLAYERS; count++)
	{
		if (!playeringame[count])
			continue;

		player_t *plr = &players[count];

		if ((plr->pflags & PF_MACESPIN)
			&& plr->mo->target == (mobj_t*)sh->maceball)
		{
			player = plr;
			controlsPressed = ((sh->flags & SHF_ALLOWUP) && player->forwardmove > 0)
				|| ((sh->flags & SHF_ALLOWDOWN) && player->forwardmove < 0);
			break;
		}
	}

	if (!controlsPressed)
	{
		if (sh->deltaZ > 0)
			sh->deltaZ--;
		else if (sh->deltaZ < 0)
			sh->deltaZ++;
	}

	boolean pressing = false;

	if (player)
	{
		if (((sh->flags & SHF_ALLOWUP) && player->forwardmove > 0)
			|| ((sh->flags & SHF_ALLOWDOWN) && player->forwardmove < 0))
		{
			pressing = true;
			sh->deltaZ += player->forwardmove >> (FRACBITS-3);

			if (player->forwardmove > 0 && sh->maceball->state >= S_HOOK1 && sh->maceball->state <= S_HOOK2)
				P_SetMobjState(sh->maceball, S_HOOK3);
		}
	}

	if (!pressing && sh->maceball->state >= S_HOOK3 && sh->maceball->state <= S_HOOK4)
		P_SetMobjState(sh->maceball, S_HOOK1);

	if (sh->deltaZ > sh->aboveDelta)
		sh->deltaZ = sh->aboveDelta;
	else if (sh->deltaZ < sh->belowDelta)
		sh->deltaZ = sh->belowDelta;

	if (player)
		SetPlayerPositionFromSwing(sh, player);
}

void T_SwingHang(swinghang_t *sh)
{
	boolean nearSomebody = false;

	vector3_t rotatePoint;
	rotatePoint.x = sh->x << FRACBITS;
	rotatePoint.y = sh->y << FRACBITS;
	rotatePoint.z = 0; // Will set z later

	// Are you near a player? Otherwise don't bother
	for (int i = 0; i < MAXPLAYERS; i++)
	{
		if (!playeringame[i])
			continue;

		const mobj_t *playermo = players[i].mo;

		if (D_abs(rotatePoint.x - playermo->x) > 2048*FRACUNIT
			|| D_abs(rotatePoint.y - playermo->y) > 2048*FRACUNIT)
			continue;

		nearSomebody = true;
	}

	if (!nearSomebody)
		return;

	boolean controlsPressed = false;
	player_t *player = NULL;

	// Is a player attached?
	for (int count = 0; count < MAXPLAYERS; count++)
	{
		if (!playeringame[count])
			continue;

		player_t *plr = &players[count];

		if ((plr->pflags & PF_MACESPIN)
			&& plr->mo->target == (mobj_t*)sh->maceball)
		{
			player = plr;
			controlsPressed = ((sh->flags & SHF_ALLOWUP) && player->forwardmove > 0)
				|| ((sh->flags & SHF_ALLOWDOWN) && player->forwardmove < 0);
			break;
		}
	}

	if (!controlsPressed)
	{
		if (sh->deltaZ > 0)
			sh->deltaZ--;
		else if (sh->deltaZ < 0)
			sh->deltaZ++;

		if (sh->maceball->state >= S_HOOK3 && sh->maceball->state <= S_HOOK4)
			P_SetMobjState(sh->maceball, S_HOOK1);
	}
	else
	{
		if (player && player->forwardmove > 0 && sh->maceball->state >= S_HOOK1 && sh->maceball->state <= S_HOOK2)
			P_SetMobjState(sh->maceball, S_HOOK3);
	}

	// Always update movedir to prevent desync. But do we really have to?
	// Can't this be calculated from leveltime? Why yes, yes it can...
	int16_t curPos = (sh->mspeed * (leveltime + sh->mphase)) & FINEMASK;

	vector3_t axis;
	vector3_t rotationDir;

//		CONS_Printf("a: %d, %d, %d; r: %d, %d, %d", sm->nv.x, sm->nv.y, sm->nv.z, sm->rotation.x, sm->rotation.y, sm->rotation.z);

	// int8_t to fixed_t
	axis.x = (fixed_t)sh->nv.x << 9;
	axis.y = (fixed_t)sh->nv.y << 9;
	axis.z = (fixed_t)sh->nv.z << 9;
	rotationDir.x = (fixed_t)sh->rotation.x << 9;
	rotationDir.y = (fixed_t)sh->rotation.y << 9;
	rotationDir.z = (fixed_t)sh->rotation.z << 9;

	vector4_t rotVec = FV3_RotateVector(&rotationDir, &axis, curPos);

	if (player)
	{
		if (((sh->flags & SHF_ALLOWUP) && player->forwardmove > 0)
			|| ((sh->flags & SHF_ALLOWDOWN) && player->forwardmove < 0))
		{
			sh->deltaZ += player->forwardmove >> (FRACBITS-3);
		}
	}

	if (sh->deltaZ > sh->aboveDelta)
		sh->deltaZ = sh->aboveDelta;
	else if (sh->deltaZ < sh->belowDelta)
		sh->deltaZ = sh->belowDelta;

//	CONS_Printf("%d, %d, %d", rotVec.x, rotVec.y, rotVec.z);

	fixed_t dist = sh->length;

	P_UnsetThingPosition((mobj_t*)sh->maceball);
	sh->maceball->x = rotatePoint.x + (rotVec.x * dist);
	sh->maceball->y = rotatePoint.y + (rotVec.y * dist);
	sh->maceball->z = (sh->z + sh->deltaZ) << FRACBITS;
	P_SetThingPosition2(sh->maceball, R_PointInSubsector2(sh->maceball->x, sh->maceball->y));

	if (player)
		SetPlayerPositionFromSwing(sh, player);
}

void P_AddSwingHang(mapthing_t *point, vector3b_t *axis, vector3b_t *rotation, VINT *args)
{
	swinghang_t *sh = cursorSwing;
	cursorSwing++;

	sh->thinker.function = T_SwingHang;
	P_AddThinker(&sh->thinker);

	sh->flags = args[11];
	sh->aboveDelta = args[12];
	sh->belowDelta = args[13];
	sh->length = D_abs(args[0]);
	sh->mspeed = D_abs(args[3]);
	sh->mphase = args[10];

	fixed_t x = point->x << FRACBITS;
	fixed_t y = point->y << FRACBITS;
	fixed_t z = (point->options >> 4) << FRACBITS;
	int i;
	for (i=0 ; i< NUMMOBJTYPES ; i++)
		if (point->type == mobjinfo[i].doomednum)
			break;
	z = P_GetMapThingSpawnHeight(i, point, x, y, z);
	sh->x = x >> FRACBITS;
	sh->y = y >> FRACBITS;
	sh->z = z >> FRACBITS;

	// We don't get fancy here. We just need one object.
	sh->maceball = P_SpawnMobj(x, y, z, MT_HOOK);

	if (args[8] & TMM_SWING)
	{
		sh->thinker.function = T_SwingBezier;
		//sh->flags |= SHF_SYNC; // DOES NOT WORK
		// Find the curve, load it in
		char lumpName[9];
		D_snprintf(lumpName, sizeof(lumpName), "MAP%02dC", gamemapinfo.mapNumber);
		int lumpNum = W_GetNumForName(lumpName);
		if (lumpNum >= 0)
		{
			bezier_lump_t *lump = (bezier_lump_t*)W_POINTLUMPNUM(lumpNum);
			int16_t numSegments;
			bezier_segment_t *segment = GetPathFromLump(lump, args[14], &numSegments);
			sh->bezierPath = Bezier_CreatePath(segment, numSegments);
			Bezier_InitFollower(&sh->bezier_follower, sh->bezierPath, (sh->flags & SHF_STARTINREVERSE) ? sh->mspeed : -sh->mspeed);

			sh->bezier_follower.duration_tics = (int16_t)axis->x * TICRATE;
			sh->bezier_follower.elapsed_tics = (int16_t)axis->y * TICRATE;
			sh->bezier_follower.start_tic = leveltime;

			sh->bezier_follower.active = (sh->flags & SHF_LOOP);
			sh->tag = args[14];
		}
		else
			I_Error("Lump not found: %s", lumpName);
	}
	else
	{
		sh->nv.x = axis->x;
		sh->nv.y = axis->y;
		sh->nv.z = axis->z;
		sh->rotation.x = rotation->x;
		sh->rotation.y = rotation->y;
		sh->rotation.z = rotation->z;
	}
}

// TODO:
// Support for creating rows of chains (ceilingheight?) - ehh, not sure about this yet
void P_AddMaceChain(mapthing_t *point, vector3b_t *axis, vector3b_t *rotation, VINT *args)
{
	// First, determine the # of items in the chain
	VINT mlength = D_abs(args[0]);
//	VINT mminlength = D_max(0, D_min(mlength - 1, msublinks));

	swingmace_t *sm = cursorMace;
	cursorMace++;
	sm->thinker.function = T_SwingMace;
	P_AddThinker(&sm->thinker);

	// 1:1 style
	sm->mlength = D_abs(args[0]);

	// Remove one link near the mace part when it's double size, it's excessive.
	if (args[8] & TMM_DOUBLESIZE)
		sm->mlength--;

	//sm->mnumspokes = args[1] + 1;
	//sm->mwidth = D_max(0, args[2]);
	sm->mspeed = D_abs(args[3] << 4);
	sm->mphase = args[10];
	//sm->mnumnospokes = args[6];
	sm->msublinks = args[7]; // chain links to remove from the inside
	if (sm->msublinks > sm->mlength)
		sm->msublinks = sm->mlength;

	//sm->mminlength = D_max(0, D_min(mlength - 1, args[7]));
	//sm->tag = point->angle;

	if (args[8] & TMM_SWING)
	{
		sm->swingSpeed = args[9];
	}

	fixed_t x = point->x << FRACBITS;
	fixed_t y = point->y << FRACBITS;
	fixed_t z = (point->options >> 4) << FRACBITS;
	int i;
	for (i=0 ; i< NUMMOBJTYPES ; i++)
		if (point->type == mobjinfo[i].doomednum)
			break;
	z = P_GetMapThingSpawnHeight(i, point, x, y, z);
	sm->macechain.x = x >> FRACBITS;
	sm->macechain.y = y >> FRACBITS;
	sm->macechain.z = z >> FRACBITS;

	sm->nv.x = axis->x;
	sm->nv.y = axis->y;
	sm->nv.z = axis->z;
	sm->rotation.x = rotation->x;
	sm->rotation.y = rotation->y;
	sm->rotation.z = rotation->z;

	mobjtype_t macetype;
	mobjtype_t chainlink;
	boolean mchainlike = false;

	if (i == MT_CHAINPOINT)
	{
		if (args[8] & TMM_DOUBLESIZE)
		{
			macetype = MT_BIGGRABCHAIN;
			chainlink = MT_BIGMACECHAIN;
		}
		else
		{
			macetype = MT_SMALLGRABCHAIN;
			chainlink = MT_SMALLMACECHAIN;
		}
		mchainlike = true;
	}
	else
	{
		if (args[8] & TMM_DOUBLESIZE)
		{
			macetype = MT_BIGMACE;
			chainlink = MT_BIGMACECHAIN;
		}
		else
		{
			macetype = MT_SMALLMACE;
			chainlink = MT_SMALLMACECHAIN;
		}
	}

	sm->sound = (mchainlike ? 0 : 1);
//	VINT radiusfactor = 1;
//	VINT widthfactor = 2;

//	VINT mmaxlength = mlength;

	fixed_t dist = mobjinfo[chainlink].radius;
	sm->macechain.interval = (dist >> FRACBITS) << 1;

	if (args[8] & TMM_MACELINKS)
	{
		sm->flags |= TMM_MACELINKS;
		chainlink = macetype;
		sm->macechain.interval <<= 1;
	}

	dist = (sm->macechain.interval * sm->msublinks) << FRACBITS;
	mlength -= sm->msublinks;

	sm->macechain.chain = NULL;
	sm->macechain.numchain = 0;
	boolean first = true;
	VINT count = 0;
	while (count < mlength)
	{
		const fixed_t distAccum = dist + ((sm->macechain.interval << FRACBITS) * count);

		const fixed_t spawnX = x + FixedMul(distAccum, sm->nv.x);
		const fixed_t spawnY = y + FixedMul(distAccum, sm->nv.y);
		const fixed_t spawnZ = (z - (mobjinfo[chainlink].height >> 2)) + FixedMul(distAccum, sm->nv.z);

		ringmobj_t *link = (ringmobj_t*)P_SpawnMobj(spawnX, spawnY, spawnZ, chainlink);
		if (first)
		{
			first = false;
			sm->macechain.chain = link;
		}
		sm->macechain.numchain++;
		count++;
	}

	if (args[8] & TMM_DOUBLESIZE)
	{
		sm->flags |= TMM_DOUBLESIZE;
		dist += sm->macechain.interval << FRACBITS;
	}

	dist += mobjinfo[macetype].radius;

	// Now spawn the end piece
	const fixed_t spawnX = x + FixedMul(dist, sm->nv.x);
	const fixed_t spawnY = y + FixedMul(dist, sm->nv.y);
	const fixed_t spawnZ = (z - (mobjinfo[macetype].height >> 1)) + FixedMul(dist, sm->nv.z);
	sm->macechain.maceball = (ringmobj_t*)P_SpawnMobj(spawnX, spawnY, spawnZ, macetype);
/*
	if (sm->tag == 137 || sm->tag == 138)
	{
		I_Error("%d, %d, %d\n%d, %d, %d\n%d, %d, %d\n%d, %d, %d", sm->nv.x, sm->nv.y, sm->nv.z, sm->rotation.x, sm->rotation.y, sm->rotation.z,
		sm->nv.x << 9, sm->nv.y << 9, sm->nv.z << 9, sm->rotation.x << 9, sm->rotation.y << 9, sm->rotation.z << 9);
	}*/
}

VINT		numlineanimspecials = 0;
VINT	*linespeciallist = NULL;
/*
sector_t *I_TO_SEC(VINT i)
{
	if (i < 0)
		I_Error("i is %d", i);

	return dpsectors[i];
}*/

void P_SpawnSpecials (void)
{
	int		i;

	lightningSpawner = NULL;

	/* */
	/*	Init special SECTORs */
	/* */
	for (i=0 ; i<numsectors ; i++)
	{
		sector_t *sector = I_TO_SEC(i);

		if (!sector->special)
			continue;
		switch (sector->special)
		{
			default:
				break;
		}
	}
		
	/* */
	/*	Init line EFFECTs */
	/* */
	numlineanimspecials = 0;
	for (i = 0; i < numlines; i++)
	{
		const uint8_t special = P_GetLineSpecial(&lines[i]);
		switch (special)
		{
		case 48:	// EFFECT FIRSTCOL SCROLL
		case 142:	// MODERATE VERT SCROLL
			if (numlineanimspecials >= MAXLINEANIMS)
				continue;
			break;
		case 53: //Continuous floor/ceiling mover
			EV_DoFloor(&lines[i], continuousMoverFloor);
			EV_DoFloor(&lines[i], continuousMoverCeiling);
			break;
		case 54: //Continuous floor mover
			EV_DoFloor(&lines[i], continuousMoverFloor);
			break;
		case 55: //Continuous ceiling mover
			EV_DoFloor(&lines[i], continuousMoverCeiling);
			break;
		case 60: // Moving platform
			if (lines[i].flags & ML_DONTPEGBOTTOM)
				EV_DoFloor(&lines[i], floorContinuous);
			else
				EV_DoFloor(&lines[i], bothContinuous);
			break;
		case 61: // Crusher (Ceiling to floor)
		case 62: // Crusher (Floor to ceiling)
			EV_DoCeiling(&lines[i], special == 62 ? raiseAndCrush : crushAndRaise);
			break;
		case 120: // Water, but kind of boom-style
		{
			VINT sec = sides[*lines[i].sidenum].sector;
			for (int s = -1; (s = P_FindSectorFromLineTag(lines+i,s)) >= 0;)
				I_TO_SEC(s)->heightsec = sec;
			break;
		}
		case 100: // 'FOF' sector
		case 170: // Crumbling (respawn)
		{
			VINT sec = sides[*lines[i].sidenum].sector;
			I_TO_SEC(sec)->flags |= SF_FOF_CONTROLSECTOR;

			for (int s = -1; (s = P_FindSectorFromLineTag(lines+i,s)) >= 0;)
			{
				I_TO_SEC(s)->fofsec = sec;
				I_TO_SEC(sec)->specline = i;

			// A sector that has FOF collision, but for rendering it will swap the floor/ceiling
			// heights depending on the camera height.
			// Should that be the halfheight of the control sector?
			// Or maybe even configurable somehow, by using the control sector's texture offset value...
				if (lines[i].flags & ML_BLOCKMONSTERS)
					I_TO_SEC(s)->flags |= SF_FOF_SWAPHEIGHTS;
			}
			break;
		}
		case 105: // FOF that is invisible but solid
		{
			VINT sec = sides[*lines[i].sidenum].sector;
			I_TO_SEC(sec)->flags |= SF_FOF_CONTROLSECTOR;
			for (int s = -1; (s = P_FindSectorFromLineTag(lines+i,s)) >= 0;)
			{
				I_TO_SEC(s)->fofsec = sec;
				I_TO_SEC(sec)->specline = i;
				I_TO_SEC(s)->flags |= SF_FOF_INVISIBLE_TANGIBLE;
			}
			break;
		}
		case 160: // Water bobbing FOF
		{
			VINT sec = sides[*lines[i].sidenum].sector;
			I_TO_SEC(sec)->flags |= SF_FOF_CONTROLSECTOR;
			for (int s = -1; (s = P_FindSectorFromLineTag(lines+i,s)) >= 0;)
			{
				I_TO_SEC(s)->fofsec = sec;
				I_TO_SEC(sec)->specline = i;

				// These are always SF_FOF_SWAPHEIGHTS
				I_TO_SEC(s)->flags |= SF_FOF_SWAPHEIGHTS;
				I_TO_SEC(s)->flags |= SF_FLOATBOB;
			}
			break;
		}
		case 178: // Crumbling, respawn, floating
		{
			VINT sec = sides[*lines[i].sidenum].sector;
			I_TO_SEC(sec)->flags |= SF_FOF_CONTROLSECTOR;
			for (int s = -1; (s = P_FindSectorFromLineTag(lines+i,s)) >= 0;)
			{
				I_TO_SEC(s)->fofsec = sec;
				I_TO_SEC(s)->flags |= SF_CRUMBLE;
				I_TO_SEC(s)->flags |= SF_FLOATBOB;
				I_TO_SEC(s)->flags |= SF_RESPAWN;
				I_TO_SEC(sec)->specline = i;
			}
			break;
		}
		case 179: // Crumbling, no-respawn, floating
		{
			VINT sec = sides[*lines[i].sidenum].sector;
			I_TO_SEC(sec)->flags |= SF_FOF_CONTROLSECTOR;
			for (int s = -1; (s = P_FindSectorFromLineTag(lines+i,s)) >= 0;)
			{
				I_TO_SEC(s)->fofsec = sec;
				I_TO_SEC(s)->flags |= SF_CRUMBLE;
				I_TO_SEC(s)->flags |= SF_FLOATBOB;
				I_TO_SEC(sec)->specline = i;
			}
			break;
		}
		case 190: // Rising Platform
		{
			VINT sec = sides[lines[i].sidenum[0]].sector;
			I_TO_SEC(sec)->flags |= SF_FOF_CONTROLSECTOR;
	
			for (int s = -1; (s = P_FindSectorFromLineTag(lines+i,s)) >= 0;)
			{
				I_TO_SEC(s)->fofsec = sec;
				I_TO_SEC(sec)->specline = i;
			}

			P_AddRaiseThinker(sec, &lines[i]);
			break;
		}
		case 249: // Scroll line texture by tagged sector floor (X) and ceiling (Y)
		{
			P_StartScrollTex(&lines[i]);
			break;
		}
		case 250: // Scroll floor
		{
			for (int s = -1; (s = P_FindSectorFromLineTag(lines+i,s)) >= 0;)
				P_StartScrollFlat(&lines[i], s, false);

			break;
		}
		case 251: // Scroll floor and carry
		{
			for (int s = -1; (s = P_FindSectorFromLineTag(lines+i,s)) >= 0;)
				P_StartScrollFlat(&lines[i], s, true);

			break;
		}
		}
	}

	if (effects_flags & EFFECTS_COPPER_ENABLED)
		P_InitLightning();
}
