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
			res = T_MovePlane(ceiling->sector,ceiling->speed,
					ceiling->topheight,false,1,ceiling->direction);
			if (!(gametic&7))
			{
				switch (ceiling->type)
				{
				case silentCrushAndRaise:
					break;
				default:
					break;
				}
				}
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
			res = T_MovePlane(ceiling->sector,ceiling->speed,
				ceiling->bottomheight,ceiling->crush,1,ceiling->direction);
			if (!(gametic&7))
			{
				switch (ceiling->type)
				{
				case silentCrushAndRaise:
					break;
				default:
					break;
				}
			}
			if (res == pastdest)
				switch(ceiling->type)
				{
					case silentCrushAndRaise:
					case crushAndRaise:
						ceiling->speed = CEILSPEED;
					case fastCrushAndRaise:
						ceiling->direction = 1;
						break;
					case lowerAndCrush:
					case lowerToFloor:
						break;
					default:
						break;
				}
			else
			if (res == crushed)
				switch(ceiling->type)
				{
					case silentCrushAndRaise:
					case crushAndRaise:
					case lowerAndCrush:
						ceiling->speed = CEILSPEED / 8;
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
			case fastCrushAndRaise:
				ceiling->crush = true;
				ceiling->topheight = sec->ceilingheight;
				ceiling->bottomheight = sec->floorheight;
				ceiling->direction = -1;
				ceiling->speed = CEILSPEED * 2;
				break;
			case silentCrushAndRaise:
			case crushAndRaise:
				ceiling->crush = true;
				ceiling->topheight = sec->ceilingheight;
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
