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
	for (int i = *start + 2; i < numlinetags*2; i += 2)
	{
		if (linetags[i] == tag)
		{
			*start = i;
			return linetags[i - 1];
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
			side_t *side = &sides[line->sidenum[0]];
			int16_t textureoffset = side->textureoffset & 0xfff;
	    	textureoffset <<= 4; // sign extend
    	  	textureoffset >>= 4; // sign extend
			int16_t rowoffset = (side->textureoffset & 0xf000) | ((unsigned)side->rowoffset << 4);
      		rowoffset >>= 4; // sign extend

			if (ldflags[line-lines] & ML_NOCLIMB)
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
					sec->specialdata = LPTR_TO_SPTR(floor);
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
			side_t *side = &sides[line->sidenum[0]];
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
			side_t *side = &sides[line->sidenum[0]];
			int16_t textureoffset = side->textureoffset & 0xfff;
	    	textureoffset <<= 4; // sign extend
    	  	textureoffset >>= 4; // sign extend
			int16_t rowoffset = (side->textureoffset & 0xf000) | ((unsigned)side->rowoffset << 4);
      		rowoffset >>= 4; // sign extend

			if (ldflags[line-lines] & ML_NOCLIMB)
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
					sec->specialdata = LPTR_TO_SPTR(ceiling);
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
		if (!(ldflags[li] & ML_HAS_SPECIAL_OR_TAG))
			continue;

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
			ldflags[li] &= ~ML_HAS_SPECIAL_OR_TAG;
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
			sec->specialdata = LPTR_TO_SPTR(scrollflat);
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

typedef struct
{
	VINT x, y, z;
	ringmobj_t *chain; // First item in the chain list.
	ringmobj_t *maceball;
	VINT numchain;
	VINT interval; // The diameter (in FRACUNITs) to space out the links
} macechain_t;

typedef struct
{
	thinker_t thinker;
	macechain_t macechain;

	// Old style
	int16_t mlength;
	int16_t mspeed;
//	int16_t mphase;
//	int16_t mminlength;
//	int16_t mwidth;
//	int16_t tag; // for debugging
	int16_t msublinks; // # of links from the inside to subtract
	int16_t swingSpeed;

	boolean sound;

	// new idea
	vector3_t nv; // Normalized vector
	vector3_t rotation;

//	VINT args[10];
} swingmace_t;

void P_SSNMaceRotate(swingmace_t *sm)
{
	// Always update movedir to prevent desync. But do we really have to?
	// Can't this be calculated from leveltime? Why yes, yes it can...
	int16_t curPos = (sm->mspeed * leveltime) & FINEMASK;

	vector4_t axis;
	vector4_t rotationDir;

	if (sm->swingSpeed)
	{
		angle_t swingmag = FixedMul(finecosine(curPos), sm->swingSpeed << FRACBITS);
		angle_t fa = swingmag >> ANGLETOFINESHIFT;
//		CONS_Printf("fa: %d", fa);
		curPos = fa;
	}

//		CONS_Printf("a: %d, %d, %d; r: %d, %d, %d", sm->nv.x, sm->nv.y, sm->nv.z, sm->rotation.x, sm->rotation.y, sm->rotation.z);

	// int8_t to fixed_t
	axis.x = sm->nv.x << 9;
	axis.y = sm->nv.y << 9;
	axis.z = sm->nv.z << 9;
	rotationDir.x = sm->rotation.x << 9;
	rotationDir.y = sm->rotation.y << 9;
	rotationDir.z = sm->rotation.z << 9;

	const fixed_t ux = FixedMul(axis.x, rotationDir.x);
	const fixed_t uy = FixedMul(axis.x, rotationDir.y);
	const fixed_t uz = FixedMul(axis.x, rotationDir.z);
	const fixed_t vx = FixedMul(axis.y, rotationDir.x);
	const fixed_t vy = FixedMul(axis.y, rotationDir.y);
	const fixed_t vz = FixedMul(axis.y, rotationDir.z);
	const fixed_t wx = FixedMul(axis.z, rotationDir.x);
	const fixed_t wy = FixedMul(axis.z, rotationDir.y);
	const fixed_t wz = FixedMul(axis.z, rotationDir.z);
	fixed_t sa = finesine(curPos);
	fixed_t ca = finecosine(curPos);

	vector4_t rotVec;
	rotVec.x = FixedMul(axis.x,(ux+vy+wz))
				+ FixedMul((FixedMul(rotationDir.x,(FixedMul(axis.y,axis.y)+FixedMul(axis.z,axis.z)))-FixedMul(axis.x,(vy+wz))), ca)
				+ FixedMul((-wy+vz),sa);
	rotVec.y = FixedMul(axis.z,(ux+vy+wz))
				+ FixedMul((FixedMul(rotationDir.z,(FixedMul(axis.x,axis.x)+FixedMul(axis.y,axis.y)))-FixedMul(axis.z,(ux+vy))), ca)
				+ FixedMul((-vx+uy),sa);
	rotVec.z = FixedMul(axis.y,(ux+vy+wz))
				+ FixedMul((FixedMul(rotationDir.y,(FixedMul(axis.x,axis.x)+FixedMul(axis.z,axis.z)))-FixedMul(axis.y,(ux+wz))), ca)
				+ FixedMul((wx-uz),sa);

//	CONS_Printf("%d, %d, %d", rotVec.x, rotVec.y, rotVec.z);

	int16_t msublinks = sm->msublinks;
	int16_t mnumchain = sm->macechain.numchain;
	fixed_t dist = sm->macechain.interval * msublinks;

	ringmobj_t *link = sm->macechain.chain;
	int count = 0;
	while (count < mnumchain)
	{
		fixed_t distAccum = dist + sm->macechain.interval * count;
		//		P_UnsetThingPosition((mobj_t*)link);
		link->x = sm->macechain.x + ((rotVec.x * distAccum) >> FRACBITS);
		link->y = sm->macechain.y + ((rotVec.z * distAccum) >> FRACBITS);
		link->z = sm->macechain.z + ((rotVec.y * distAccum) >> FRACBITS);
		link->z -= (mobjinfo[link->type].height >> FRACBITS) >> 2;
//		P_SetThingPosition((mobj_t*)link);
		link++;
		count++;
	}

	dist += sm->macechain.interval * (count-1);
	dist += mobjinfo[sm->macechain.maceball->type].radius >> FRACBITS;
	P_UnsetThingPosition((mobj_t*)sm->macechain.maceball);
	sm->macechain.maceball->x = sm->macechain.x + ((rotVec.x * dist) >> FRACBITS);
	sm->macechain.maceball->y = sm->macechain.y + ((rotVec.z * dist) >> FRACBITS);
	sm->macechain.maceball->z = sm->macechain.z + ((rotVec.y * dist) >> FRACBITS);
	sm->macechain.maceball->z -= (mobjinfo[sm->macechain.maceball->type].height >> FRACBITS) >> 1;
	P_SetThingPosition2((mobj_t*)sm->macechain.maceball, R_PointInSubsector2(sm->macechain.maceball->x << FRACBITS, sm->macechain.maceball->y << FRACBITS));

	// Is a player attached?
	for (count = 0; count < MAXPLAYERS; count++)
	{
		if (!playeringame[count])
			continue;

		const player_t *player = &players[count];

		if ((player->pflags & PF_MACESPIN)
			&& player->mo->target == (mobj_t*)sm->macechain.maceball)
		{
			vector3_t newPos;
			newPos.x = (sm->macechain.x << FRACBITS) + (rotVec.x * dist);
			newPos.y = (sm->macechain.y << FRACBITS) + (rotVec.z * dist);
			newPos.z = (sm->macechain.z << FRACBITS) + (rotVec.y * dist) - (P_GetPlayerSpinHeight() >> 1) - (P_GetPlayerSpinHeight() >> 2);
			player->mo->momx = (newPos.x - player->mo->x) ;
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

// TODO:
// Support for creating rows of chains (ceilingheight?)
// Support for the 'mphase' (offset in the rotation)
void P_AddMaceChain(mapthing_t *point, vector3_t *axis, vector3_t *rotation, VINT *args)
{
	// First, determine the # of items in the chain
	VINT mlength = D_abs(args[0]);
//	VINT mminlength = D_max(0, D_min(mlength - 1, msublinks));

	swingmace_t *sm = cursorMace;
	cursorMace++;
	sm->thinker.function = T_SwingMace;
	P_AddThinker (&sm->thinker);

// 1:1 style
sm->mlength = D_abs(args[0]);
//sm->mnumspokes = args[1] + 1;
//sm->mwidth = D_max(0, args[2]);
sm->mspeed = D_abs(args[3] << 4);
//sm->mphase = args[4] % 360;
//sm->mnumnospokes = args[6];
sm->msublinks = args[7]; // chain links to remove from the inside
if (sm->msublinks > sm->mlength)
	sm->msublinks = sm->mlength;

//sm->mminlength = D_max(0, D_min(mlength - 1, args[7]));
//sm->tag = point->angle;

	if (args[8] & TMM_SWING)
	{
		sm->swingSpeed = args[9] << 8;
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

	dist += mobjinfo[macetype].radius;

	// Now spawn the end piece
	const fixed_t spawnX = x + FixedMul(dist, sm->nv.x);
	const fixed_t spawnY = y + FixedMul(dist, sm->nv.y);
	const fixed_t spawnZ = (z - (mobjinfo[macetype].height >> 1)) + FixedMul(dist, sm->nv.z);
	sm->macechain.maceball = (ringmobj_t*)P_SpawnMobj(spawnX, spawnY, spawnZ, macetype);

	sm->nv.x = axis->x;
	sm->nv.y = axis->y;
	sm->nv.z = axis->z;
	sm->rotation.x = rotation->x;
	sm->rotation.y = rotation->y;
	sm->rotation.z = rotation->z;
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
	sector_t	**dpsector;
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
			if (ldflags[i] & ML_DONTPEGBOTTOM)
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
				if (ldflags[i] & ML_BLOCKMONSTERS)
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
