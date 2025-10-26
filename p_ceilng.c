#include "doomdef.h"
#include "p_local.h"

/*================================================================== */
/*================================================================== */
/* */
/*							CEILINGS */
/* */
/*================================================================== */
/*================================================================== */

/*================================================================== */
/* */
/*	T_MoveCeiling */
/* */
/*================================================================== */
void T_MoveCeiling (ceiling_t *ceiling)
{
	result_e	res;

	switch(ceiling->direction)
	{
		case 0:		/* IN STASIS */
			break;
		case 1:		/* UP */
			res = T_MovePlane(ceiling->sector,ceiling->upspeed,
					ceiling->topheight << FRACBITS,false,1,ceiling->direction);

			if (res == pastdest)
				switch(ceiling->type)
				{
					case raiseToHighest:
						break;
					case silentCrushAndRaise:
					case fastCrushAndRaise:
					case crushAndRaise:
						ceiling->direction = -1;
						break;
					default:
						break;
				}
			break;
		case -1:	/* DOWN */
			res = T_MovePlane(ceiling->sector,ceiling->downspeed,
				ceiling->bottomheight << FRACBITS,ceiling->crush,1,ceiling->direction);

			if (res == pastdest)
				switch(ceiling->type)
				{
					case silentCrushAndRaise:
					case crushAndRaise:
					case raiseAndCrush:
					case fastCrushAndRaise:
						ceiling->direction = 1;
						break;
					case lowerAndCrush:
					case lowerToFloor:
						break;
					default:
						break;
				}
			break;
	}
}

/*================================================================== */
/* */
/*		EV_DoCeiling */
/*		Move a ceiling up/down and all around! */
/* */
/*================================================================== */
int EV_DoCeiling (line_t *line, ceiling_e  type)
{
	int			secnum,rtn;
	sector_t		*sec;
	ceiling_t		*ceiling;
	
	secnum = -1;
	rtn = 0;
	
	uint8_t tag = P_GetLineTag(line);
	while ((secnum = P_FindSectorFromLineTagNum(tag,secnum)) >= 0)
	{
		sec = &sectors[secnum];
		if (sec->specialdata)
			continue;
		
		/* */
		/* new door thinker */
		/* */
		rtn = 1;
		ceiling = Z_Malloc (sizeof(*ceiling), PU_LEVSPEC);
		P_AddThinker (&ceiling->thinker);
		sec->specialdata = LPTR_TO_SPTR(ceiling);
		ceiling->thinker.function = T_MoveCeiling;
		ceiling->sector = sec;
		ceiling->crush = false;
		switch(type)
		{
			case raiseAndCrush:
			case crushAndRaise:
			{
				mapvertex_t *v1 = &vertexes[line->v1];
				mapvertex_t *v2 = &vertexes[line->v2];
				fixed_t arg2, arg3;
				if (ldflags[line-lines] & ML_MIDTEXTUREBLOCK)
				{
					arg2 = D_abs(v2->x - v1->x);
					arg3 = arg2;
				}
				else
				{
					arg2 = R_PointToDist((v2->x << FRACBITS) - (v1->x << FRACBITS), (v2->y << FRACBITS) - (v1->y << FRACBITS)) >> (FRACBITS + 1);
					arg3 = arg2 >> 2;
				}

				// Easier on a console. :)
				arg2 >>= 1;
				arg3 >>= 1;

				ceiling->upspeed = arg2 << (FRACBITS - 2);
				ceiling->crush = true;
				ceiling->bottomheight = (sec->floorheight >> FRACBITS) + 1;
				ceiling->downspeed = arg3 << (FRACBITS - 2);

				if (type == raiseAndCrush)
				{
					ceiling->topheight = P_FindHighestCeilingSurrounding(sec)->ceilingheight >> FRACBITS;
					ceiling->direction = 1;
				}
				else
				{
					ceiling->topheight = sec->ceilingheight >> FRACBITS;
					ceiling->direction = -1;
				}
			}
			break;
			case fastCrushAndRaise:
				ceiling->crush = true;
				ceiling->topheight = sec->ceilingheight;
				ceiling->bottomheight = sec->floorheight;
				ceiling->direction = -1;
				ceiling->speed = CEILSPEED * 2;
				break;
			case silentCrushAndRaise:
			case lowerAndCrush:
			case lowerToFloor:
				ceiling->bottomheight = sec->floorheight;
				ceiling->direction = -1;
				ceiling->speed = CEILSPEED;
				break;
			case raiseToHighest:
				ceiling->topheight = P_FindHighestCeilingSurrounding(sec)->ceilingheight;
				ceiling->direction = 1;
				ceiling->speed = CEILSPEED;
				break;
		}
		
		ceiling->tag = sec->tag;
		ceiling->type = type;
	}
	return rtn;
}

/*================================================================== */
/* */
/*		EV_CeilingCrushStop */
/*		Stop a ceiling from crushing! */
/* */
/*================================================================== */
int	EV_CeilingCrushStop(line_t	*line)
{
	int		i;
	int		rtn;
	uint8_t tag = P_GetLineTag(line);
	
	rtn = 0;
	for (i = 0;i < MAXCEILINGS;i++)
		if (activeceilings[i] && (activeceilings[i]->tag == tag) &&
			(activeceilings[i]->direction != 0))
		{
			activeceilings[i]->olddirection = activeceilings[i]->direction;
			activeceilings[i]->thinker.function = NULL;
			activeceilings[i]->direction = 0;		/* in-stasis */
			rtn = 1;
		}

	return rtn;
}
