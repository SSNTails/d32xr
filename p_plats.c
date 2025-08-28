/*================================================================== */
/*================================================================== */
/* */
/*							PLATFORM RAISING */
/* */
/*================================================================== */
/*================================================================== */
#include "doomdef.h"
#include "p_local.h"

/*================================================================== */
/* */
/*	Move a plat up and down */
/* */
/*================================================================== */
void	T_PlatRaise(plat_t	*plat)
{
	result_e	res;
	
	switch(plat->status)
	{
		case	up:
			res = T_MovePlane(plat->sector,plat->speed,
					plat->high,plat->crush,0,1);
			
			if (res == crushed && (!plat->crush))
			{
				plat->count = plat->wait;
				plat->status = down;
			}
			else
			if (res == pastdest)
			{
				plat->count = plat->wait;
				plat->status = waiting;
			}
			break;
		case	down:
			res = T_MovePlane(plat->sector,plat->speed,plat->low,false,0,-1);
			if (res == pastdest)
			{
				plat->count = plat->wait;
				plat->status = waiting;
			}
			break;
		case	waiting:
			if (!--plat->count)
			{
				if (plat->sector->floorheight == plat->low)
					plat->status = up;
				else
					plat->status = down;
			}
		case	in_stasis:
			break;
	}
}

/*================================================================== */
/* */
/*	Do Platforms */
/*	"amount" is only used for SOME platforms. */
/* */
/*================================================================== */
int	EV_DoPlat(line_t *line,plattype_e type,int amount)
{
	plat_t		*plat;
	int			secnum;
	int			rtn;
	sector_t	*sec;
	uint8_t tag = P_GetLineTag(line);
	
	secnum = -1;
	rtn = 0;
	
	while ((secnum = P_FindSectorFromLineTagNum(tag,secnum)) >= 0)
	{
		sec = &sectors[secnum];
		if (sec->specialdata)
			continue;
	
		/* */
		/* Find lowest & highest floors around sector */
		/* */
		rtn = 1;
		plat = Z_Malloc( sizeof(*plat), PU_LEVSPEC);
		P_AddThinker(&plat->thinker);
		
		plat->type = type;
		plat->sector = sec;
		plat->sector->specialdata = LPTR_TO_SPTR(plat);
		plat->thinker.function = T_PlatRaise;
		plat->crush = false;
		plat->tag = tag;
		switch(type)
		{
			case raiseToNearestAndChange:
				plat->speed = PLATSPEED/2;
				sec->floorpic = sectors[sides[line->sidenum[0]].sector].floorpic;
				plat->high = P_FindNextHighestFloor(sec,sec->floorheight);
				plat->wait = 0;
				plat->status = up;
				sec->special = 0;		/* NO MORE DAMAGE, IF APPLICABLE */
				break;
			case raiseAndChange:
				plat->speed = PLATSPEED/2;
				sec->floorpic = sectors[sides[line->sidenum[0]].sector].floorpic;
				plat->high = sec->floorheight + amount*FRACUNIT;
				plat->wait = 0;
				plat->status = up;
				break;
			case downWaitUpStay:
			case blazeDWUS:
				plat->speed = PLATSPEED * 4 * (type == blazeDWUS ? 2 : 1);
				plat->low = P_FindLowestFloorSurrounding(sec);
				if (plat->low > sec->floorheight)
					plat->low = sec->floorheight;
				plat->high = sec->floorheight;
				plat->wait = 15*PLATWAIT;
				plat->status = down;
				break;
			case perpetualRaise:
				plat->speed = PLATSPEED;
				plat->low = P_FindLowestFloorSurrounding(sec);
				if (plat->low > sec->floorheight)
					plat->low = sec->floorheight;
				plat->high = P_FindHighestFloorSurrounding(sec);
				if (plat->high < sec->floorheight)
					plat->high = sec->floorheight;
				plat->wait = 15*PLATWAIT;
				plat->status = P_Random()&1;
				break;
		}
	}
	return rtn;
}
