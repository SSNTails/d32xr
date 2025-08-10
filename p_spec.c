/* P_Spec.c */
#include "doomdef.h"
#include "p_local.h"

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
VINT P_FindNextSectorLine(sector_t *sector, VINT start)
{
	VINT i;
	VINT isector = sector - sectors;

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
sector_t *getNextSector(line_t *line,sector_t *sec)
{
	sector_t *front;

	if (!(line->sidenum[1] >= 0))
		return NULL;
	
	front = LD_FRONTSECTOR(line);
	if (front == sec)
		return LD_BACKSECTOR(line);

	return front;
}

/*================================================================== */
/* */
/*	FIND LOWEST FLOOR HEIGHT IN SURROUNDING SECTORS */
/* */
/*================================================================== */
fixed_t	P_FindLowestFloorSurrounding(sector_t *sec)
{
	VINT			i = -1;
	line_t		*check;
	sector_t	*other;
	fixed_t		floor = sec->floorheight;

	while ((i = P_FindNextSectorLine(sec, i)) >= 0)
	{
		check = &lines[i];
		other = getNextSector(check,sec);
		if (!other)
			continue;
		if (other->floorheight < floor)
			floor = other->floorheight;
	}
	return floor;
}

/*================================================================== */
/* */
/*	FIND HIGHEST FLOOR HEIGHT IN SURROUNDING SECTORS */
/* */
/*================================================================== */
fixed_t	P_FindHighestFloorSurrounding(sector_t *sec)
{
	VINT		i = -1;
	line_t		*check;
	sector_t	*other;
	fixed_t		floor = -32000*FRACUNIT;
	
	while ((i = P_FindNextSectorLine(sec, i)) >= 0)
	{
		check = &lines[i];
		other = getNextSector(check,sec);
		if (!other)
			continue;			
		if (other->floorheight > floor)
			floor = other->floorheight;
	}

	return floor;
}

/*================================================================== */
/* */
/*	FIND NEXT HIGHEST CEILING IN SURROUNDING SECTORS */
/* */
/*================================================================== */
fixed_t	P_FindNextHighestCeiling(sector_t *sec,int currentheight)
{
	VINT		i = -1;
	int			h = 0;
	int			min;
	line_t		*check;
	sector_t	*other;
	fixed_t		height = currentheight;
	fixed_t		heightlist[20];		/* 20 adjoining sectors max! */
	
	heightlist[0] = 0;
	while ((i = P_FindNextSectorLine(sec, i)) >= 0)
	{
		check = &lines[i];
		other = getNextSector(check,sec);
		if (!other)
			continue;
		if (other->ceilingheight > height)
			heightlist[h++] = other->ceilingheight;
		if (h == sizeof(heightlist) / sizeof(heightlist[0]))
			break;
	}
	
	if (h == 0)
		return currentheight;

	/* */
	/* Find lowest height in list */
	/* */
	min = heightlist[0];
	for (i = 1;i < h;i++)
		if (heightlist[i] < min)
			min = heightlist[i];
			
	return min;
}

/*================================================================== */
/* */
/*	FIND NEXT HIGHEST FLOOR IN SURROUNDING SECTORS */
/* */
/*================================================================== */
fixed_t	P_FindNextHighestFloor(sector_t *sec,int currentheight)
{
	VINT		i = -1;
	int			h = 0;
	int			min;
	line_t		*check;
	sector_t	*other;
	fixed_t		height = currentheight;
	fixed_t		heightlist[20];		/* 20 adjoining sectors max! */
	
	heightlist[0] = 0;
	while ((i = P_FindNextSectorLine(sec, i)) >= 0)
	{
		check = &lines[i];
		other = getNextSector(check,sec);
		if (!other)
			continue;
		if (other->floorheight > height)
			heightlist[h++] = other->floorheight;
		if (h == sizeof(heightlist) / sizeof(heightlist[0]))
			break;
	}
	
	if (h == 0)
		return currentheight;

	/* */
	/* Find lowest height in list */
	/* */
	min = heightlist[0];
	for (i = 1;i < h;i++)
		if (heightlist[i] < min)
			min = heightlist[i];
			
	return min;
}

fixed_t	P_FindNextLowestFloor(sector_t *sec,int currentheight)
{
	VINT			i = -1;
	int			h = 0;
	int			min;
	line_t		*check;
	sector_t	*other;
	fixed_t		height = currentheight;
	fixed_t		heightlist[20];		/* 20 adjoining sectors max! */

	heightlist[0] = 0;
	while ((i = P_FindNextSectorLine(sec, i)) >= 0)
	{
		check = &lines[i];
		other = getNextSector(check,sec);
		if (!other)
			continue;
		if (other->floorheight < height)
			heightlist[h++] = other->floorheight;
		if (h == sizeof(heightlist) / sizeof(heightlist[0]))
			break;
	}
	
	if (h == 0)
		return currentheight;

	/* */
	/* Find lowest height in list */
	/* */
	min = heightlist[0];
	for (i = 1;i < h;i++)
		if (heightlist[i] > min)
			min = heightlist[i];
			
	return min;
}

/*================================================================== */
/* */
/*	FIND LOWEST CEILING IN THE SURROUNDING SECTORS */
/* */
/*================================================================== */
fixed_t	P_FindLowestCeilingSurrounding(sector_t *sec)
{
	VINT		i = 0;
	line_t		*check;
	sector_t	*other;
	fixed_t		height = 32000*FRACUNIT;
	
	while ((i = P_FindNextSectorLine(sec, i)) >= 0)
	{
		check = &lines[i];
		other = getNextSector(check,sec);
		if (!other)
			continue;
		if (other->ceilingheight < height)
			height = other->ceilingheight;
	}

	return height;
}

/*================================================================== */
/* */
/*	FIND HIGHEST CEILING IN THE SURROUNDING SECTORS */
/* */
/*================================================================== */
fixed_t	P_FindHighestCeilingSurrounding(sector_t *sec)
{
	VINT	i = 0;
	line_t	*check;
	sector_t	*other;
	fixed_t	height = -32000*FRACUNIT;
	
	while ((i = P_FindNextSectorLine(sec, i)) >= 0)
	{
		check = &lines[i];
		other = getNextSector(check,sec);
		if (!other)
			continue;
		if (other->ceilingheight > height)
			height = other->ceilingheight;
	}

	return height;
}

/*================================================================== */
/* */
/*	RETURN NEXT SECTOR # THAT LINE TAG REFERS TO */
/* */
/*================================================================== */

VINT P_FindSectorWithTag(VINT tag, int start)
{
	int	i;
	
	for (i=start+1;i<numsectors;i++)
		if (sectors[i].tag == tag)
			return i;
	return -1;
}

int	P_FindSectorFromLineTag(line_t	*line,int start)
{
	return P_FindSectorFromLineTagNum(P_GetLineTag(line), start);
}

/*================================================================== */
/* */
/*	RETURN NEXT SECTOR # THAT LINE TAG REFERS TO */
/* */
/*================================================================== */
int	P_FindSectorFromLineTagNum(uint8_t tag,int start)
{
	int	i;

	for (i=start+1;i<numsectors;i++)
		if (sectors[i].tag == tag)
			return i;
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
int	P_FindMinSurroundingLight(sector_t *sector,int max)
{
	VINT			i = 0;
	int			min;
	line_t		*line;
	sector_t	*check;
	
	min = max;
	while ((i = P_FindNextSectorLine(sector, i)) >= 0)
	{
		line = &lines[i];

		check = getNextSector(line,sector);
		if (!check)
			continue;
		if (check->lightlevel < min)
			min = check->lightlevel;
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

static void P_ProcessLineSpecial(line_t *line, mobj_t *mo, sector_t *callsec)
{
	uint8_t special = P_GetLineSpecial(line);
//	uint8_t tag = P_GetLineTag(line);

	switch (special)
	{
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
					sector_t *sec = &sectors[secnum];
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
					sector_t *sec = &sectors[secnum];
					if (sec->specialdata)
						continue;

					floormove_t *floor = Z_Malloc (sizeof(*floor), PU_LEVSPEC);
					P_AddThinker (&floor->thinker);
					sec->specialdata = floor;
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
	}
}

// Only players can trigger linedef executors... this is going to come back to bite me, isn't it?
void P_LinedefExecute(player_t *player, sector_t *caller)
{

	if (player->playerstate != PST_LIVE)
		return;

	// Find linedef with this tag that is an executor linedef.
	int liStart = -1;
	VINT li;
	while ((li = P_FindNextLineWithTag(caller->tag, &liStart)) != -1)
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
			sector_t *ctrlSector = LD_FRONTSECTOR(line);
			line_t *ld;
			int16_t start = -1;
			while ((start = P_FindNextSectorLine(ctrlSector, start)) >= 0)
			{
				line_t *ld = &lines[start];
				P_ProcessLineSpecial(ld, player->mo, caller);
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

void P_PlayerInSpecialSector (player_t *player)
{
	sector_t	*sector = SS_SECTOR(player->mo->isubsector);
		
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
		case 64: // Linedef Executor: entered a sector
			P_LinedefExecute(player, sector);
			break;
		case 80: // Linedef Executor: on floor touch
			if (player->mo->z <= sector->floorheight
				|| player->mo->z + (player->mo->theight << FRACBITS) >= sector->ceilingheight)
				P_LinedefExecute(player, sector);
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
		line = linespeciallist[i]; // TODO: Should this be a key/value pair instead?
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
		bouncer->speed += GRAVITY;
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
		bouncer->watersec = &sectors[heightsec];
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

	fixed_t convDx = ldx << FRACBITS << 1;
	fixed_t convDy = ldy << FRACBITS << 1;

	for (int i = 0; i < scrollflat->numsectors; i++)
	{
		sector_t *sec = &sectors[scrollflat->sectors[i]];

		uint8_t xoff = sec->floor_xoffs >> 8;
		uint8_t yoff = sec->floor_xoffs & 0xff;

		xoff += ldx;
		yoff += ldy;

		sec->floor_xoffs = (xoff << 8) | yoff;

		if (scrollflat->carry && sec->thinglist)
		{
			mobj_t *thing = SPTR_TO_LPTR(sec->thinglist);

			while(thing) // walk sector thing list
			{
				if (thing->type == MT_PLAYER
					&& thing->z <= sec->floorheight)
				{
					player_t *player = &players[thing->player - 1];

					player->mo->momx = REALMOMX(player) + convDx;
					player->mo->momy = REALMOMY(player) + convDy;
					player->cmomx = convDx;//FixedMul(convDx, 58368);
					player->cmomy = convDy;//FixedMul(convDy, 58368);
					player->onconveyor = 4;
				}

				thing = SPTR_TO_LPTR(thing->snext);
			}
		}
	}
}

static void P_StartScrollFlat(line_t *line, sector_t *sector, boolean carry)
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

				sf->sectors[sf->numsectors++] = (VINT)(sector-sectors);
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
			if (sectors[i].tag == tag)
				numScrollflatSectors++;
		}

		scrollflat_t *scrollflat = Z_Malloc (sizeof(*scrollflat)+(sizeof(VINT)*numScrollflatSectors), PU_LEVSPEC);
		P_AddThinker (&scrollflat->thinker);
		scrollflat->thinker.function = T_ScrollFlat;
		scrollflat->ctrlLine = line;
		scrollflat->sectors = (VINT*)((uint8_t*)scrollflat + sizeof(*scrollflat));
		scrollflat->sectors[0] = (VINT)(sector - sectors);
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
		sector_t *sec = &sectors[lightningSpawner->sectorData[i]];
		sec->lightlevel = (uint8_t)(close ? 255 : (255 + (int32_t)sec->lightlevel) >> 1);
	}
}

void T_LightningFade(lightningspawn_t *spawner)
{
	if (spawner->timer < 0)
		return;

	for (int i = 0; i < spawner->numsectors*2;)
	{
		sector_t *sec = &sectors[spawner->sectorData[i++]];
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
		if (sectors[i].ceilingpic == (uint8_t)-1)
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
		if (sectors[i].ceilingpic == (uint8_t)-1)
		{
			spawner->sectorData[count++] = i;
			spawner->sectorData[count++] = sectors[i].lightlevel;
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

		paramSector = &sectors[s];

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
	VINT x, y, z;
	ringmobj_t *chain; // First item in the chain list.
	mobj_t *maceball;
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
	int16_t mphase;
	int16_t mminlength;
	int16_t mwidth;
	int16_t tag; // for debugging

	boolean sound;
	boolean swinging;

	// new idea
	vector3_t nv; // Normalized vector
	vector3_t rotation;

	VINT args[10];
} swingmace_t;

void P_SSNMaceRotate(swingmace_t *sm)
{
	// Always update movedir to prevent desync. But do we really have to?
	// Can't this be calculated from leveltime? Why yes, yes it can...
	int16_t curPos = (sm->mspeed * leveltime) & FINEMASK;

	vector4_t axis;
	vector4_t rotationDir;

	if (sm->swinging)
	{

		angle_t swingmag = FixedMul(finecosine(curPos), (sm->mspeed * sm->mspeed * sm->mspeed) << FRACBITS);
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

	ringmobj_t *link = sm->macechain.chain;
	fixed_t distAccum = 0;
	for (int i = 0; i < sm->macechain.numchain; link++, i++)
	{
		distAccum += sm->macechain.interval;

		//		P_UnsetThingPosition((mobj_t*)link);
		link->x = sm->macechain.x + ((rotVec.x * distAccum) >> FRACBITS);
		link->y = sm->macechain.y + ((rotVec.z * distAccum) >> FRACBITS);
		link->z = sm->macechain.z + ((rotVec.y * distAccum) >> FRACBITS);
		link->z -= (mobjinfo[link->type].height >> FRACBITS) >> 2;
//		P_SetThingPosition((mobj_t*)link);
	}

	distAccum += mobjinfo[sm->macechain.maceball->type].radius >> FRACBITS;
	P_UnsetThingPosition(sm->macechain.maceball);
	sm->macechain.maceball->x = (sm->macechain.x << FRACBITS) + (rotVec.x * distAccum);
	sm->macechain.maceball->y = (sm->macechain.y << FRACBITS) + (rotVec.z * distAccum);
	sm->macechain.maceball->z = (sm->macechain.z << FRACBITS) + (rotVec.y * distAccum);
	sm->macechain.maceball->z -= (sm->macechain.maceball->theight << FRACBITS) >> 1;
	sm->macechain.maceball->floorz = sm->macechain.maceball->z;
	sm->macechain.maceball->ceilingz = sm->macechain.maceball->z + (sm->macechain.maceball->theight << FRACBITS);
	P_SetThingPosition(sm->macechain.maceball);
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
	VINT msublinks = args[7];
//	VINT mminlength = D_max(0, D_min(mlength - 1, msublinks));

	swingmace_t *sm = cursorMace;
	cursorMace++;
	sm->thinker.function = T_SwingMace;
	P_AddThinker (&sm->thinker);

// 1:1 style
sm->mlength = D_abs(args[0]);
//sm->mnumspokes = args[1] + 1;
sm->mwidth = D_max(0, args[2]);
sm->mspeed = D_abs(args[3] << 4);
sm->mphase = args[4] % 360;
//sm->mnumnospokes = args[6];
sm->mminlength = D_max(0, D_min(mlength - 1, args[7]));
sm->tag = point->angle;

	sm->swinging = (args[8] & TMM_SWING);

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

	while (msublinks > 0)
	{
		dist += sm->macechain.interval << FRACBITS;
		msublinks--;
		mlength--;
	}

	sm->macechain.numchain = 0;
	boolean first = true;
	VINT count = 0;

	while (count < mlength)
	{
		const fixed_t dist = (sm->macechain.interval << FRACBITS) * (count + 1);

		const fixed_t spawnX = x + FixedMul(dist, sm->nv.x);
		const fixed_t spawnY = y + FixedMul(dist, sm->nv.y);
		const fixed_t spawnZ = (z - (mobjinfo[chainlink].height >> 2)) + FixedMul(dist, sm->nv.z);

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
	sm->macechain.maceball = P_SpawnMobj(spawnX, spawnY, spawnZ, macetype);

	sm->nv.x = axis->x;
	sm->nv.y = axis->y;
	sm->nv.z = axis->z;
	sm->rotation.x = rotation->x;
	sm->rotation.y = rotation->y;
	sm->rotation.z = rotation->z;
}

VINT		numlineanimspecials = 0;
line_t	**linespeciallist = NULL;

void P_SpawnSpecials (void)
{
	sector_t	*sector;
	int		i;

	lightningSpawner = NULL;

	/* */
	/*	Init special SECTORs */
	/* */
	sector = sectors;
	for (i=0 ; i<numsectors ; i++, sector++)
	{
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
		switch (P_GetLineSpecial(&lines[i]))
		{
		case 48:	/* EFFECT FIRSTCOL SCROLL+ */
		case 142:	/* MODERATE VERT SCROLL */
			if (numlineanimspecials >= MAXLINEANIMS)
				continue;
			break;
		case 60: // Moving platform
			EV_DoFloor(&lines[i], floorContinuous);
			break;
		case 120: // Water, but kind of boom-style
		{
			VINT sec = sides[*lines[i].sidenum].sector;
			for (int s = -1; (s = P_FindSectorFromLineTag(lines+i,s)) >= 0;)
				sectors[s].heightsec = sec;
			break;
		}
		case 100: // 'FOF' sector
		{
			VINT sec = sides[*lines[i].sidenum].sector;
			for (int s = -1; (s = P_FindSectorFromLineTag(lines+i,s)) >= 0;)
			{
				sectors[s].fofsec = sec;
				sectors[sec].specline = i;

			// A sector that has FOF collision, but for rendering it will swap the floor/ceiling
			// heights depending on the camera height.
			// Should that be the halfheight of the control sector?
			// Or maybe even configurable somehow, by using the control sector's texture offset value...
				if (ldflags[i] & ML_BLOCKMONSTERS)
					sectors[s].flags |= SF_FOF_SWAPHEIGHTS;
			}
			break;
		}
		case 105: // FOF that is invisible but solid
		{
			VINT sec = sides[*lines[i].sidenum].sector;
			for (int s = -1; (s = P_FindSectorFromLineTag(lines+i,s)) >= 0;)
			{
				sectors[s].fofsec = sec;
				sectors[sec].specline = sec;
				sectors[s].flags |= SF_FOF_INVISIBLE_TANGIBLE;
			}
			break;
		}
		case 160: // Water bobbing FOF
		{
			VINT sec = sides[*lines[i].sidenum].sector;
			for (int s = -1; (s = P_FindSectorFromLineTag(lines+i,s)) >= 0;)
			{
				sectors[s].fofsec = sec;
				sectors[sec].specline = i;

				// These are always SF_FOF_SWAPHEIGHTS
				sectors[s].flags |= SF_FOF_SWAPHEIGHTS;
				sectors[s].flags |= SF_FLOATBOB;
			}
			break;
		}
		case 178: // Crumbling, respawn, floating
		{
			VINT sec = sides[*lines[i].sidenum].sector;
			for (int s = -1; (s = P_FindSectorFromLineTag(lines+i,s)) >= 0;)
			{
				sectors[s].flags |= SF_CRUMBLE;
				sectors[s].flags |= SF_FLOATBOB;
				sectors[s].flags |= SF_RESPAWN;
				sectors[sec].specline = sec;
			}

			break;
		}
		case 179: // Crumbling, no-respawn, floating
		{
			VINT sec = sides[*lines[i].sidenum].sector;
			for (int s = -1; (s = P_FindSectorFromLineTag(lines+i,s)) >= 0;)
			{
				sectors[s].flags |= SF_CRUMBLE;
				sectors[s].flags |= SF_FLOATBOB;
				sectors[sec].specline = sec;
			}

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
				P_StartScrollFlat(&lines[i], &sectors[s], false);

			break;
		}
		case 251: // Scroll floor and carry
		{
			for (int s = -1; (s = P_FindSectorFromLineTag(lines+i,s)) >= 0;)
				P_StartScrollFlat(&lines[i], &sectors[s], true);

			break;
		}
		}
	}

	if (effects_flags & EFFECTS_COPPER_ENABLED)
		P_InitLightning();

	/* */
	/*	Init other misc stuff */
	/* */
	D_memset(activeceilings, 0, sizeof(*activeceilings)*MAXCEILINGS);
	D_memset(activeplats, 0, sizeof(*activeplats)*MAXCEILINGS);
}
