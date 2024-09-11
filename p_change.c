/* p_change.c */

#include "doomdef.h"
#include "p_local.h"

/* 
============================================================================== 

						SECTOR HEIGHT CHANGING

= After modifying a sectors floor or ceiling height, call this
= routine to adjust the positions of all things that touch the
= sector.
=
= If anything doesn't fit anymore, true will be returned.
= If crunch is true, they will take damage as they are being crushed
= If Crunch is false, you should set the sector height back the way it
= was and call P_ChangeSector again to undo the changes
============================================================================== 
*/ 

typedef struct
{
	boolean		crushchange;
	boolean		nofit;
} changetest_t;

/*
==================
=
= P_ThingHeightClip
=
= Takes a valid thing and adjusts the thing->floorz, thing->ceilingz,
= anf possibly thing->z
=
= This is called for all nearby monsters whenever a sector changes height
=
= If the thing doesn't fit, the z will be set to the lowest value and
= false will be returned
==================
*/

boolean P_ThingHeightClip (mobj_t *thing)
{
	boolean		onfloor;
	ptrymove_t	tm;
	
	onfloor = (thing->z == thing->floorz);
	
	P_CheckPosition (&tm, thing, thing->x, thing->y);	
	/* what about stranding a monster partially off an edge? */
	
	thing->floorz = tm.tmfloorz;
	thing->ceilingz = tm.tmceilingz;
	
	if (onfloor)
	/* walking monsters rise and fall with the floor */
		thing->z = thing->floorz;
	else
	{	/* don't adjust a floating monster unless forced to */
		if (thing->z+(thing->theight<<FRACBITS) > thing->ceilingz)
			thing->z = thing->ceilingz - (thing->theight<<FRACBITS);
	}
	
	if (thing->ceilingz - thing->floorz < (thing->theight<<FRACBITS))
		return false;
		
	return true;
}


/*
===============
=
= PIT_ChangeSector
=
===============
*/

boolean PIT_ChangeSector (mobj_t *thing, changetest_t *ct)
{
	if (P_ThingHeightClip (thing))
		return true;		/* keep checking */

	if (! (thing->flags & MF_SHOOTABLE) )
		return true;				/* assume it is bloody gibs or something */
		
	ct->nofit = true;
	if (ct->crushchange && !(gametic&3))
	{
		P_DamageMobj(thing,NULL,NULL,10);
	}
		
	return true;		/* keep checking (crush other things)	 */
}

void CalculateSectorBlockBox(sector_t *sector, VINT blockbox[4])
{
	fixed_t		bbox[4];
	int         block;

	M_ClearBox(bbox);

	for (int j = 0; j < sector->linecount; j++)
	{
		const line_t *li = lines + sector->lines[j];
		M_AddToBox(bbox, vertexes[li->v1].x, vertexes[li->v1].y);
		M_AddToBox(bbox, vertexes[li->v2].x, vertexes[li->v2].y);
	}

	/* adjust bounding box to map blocks */
	block = (bbox[BOXTOP]-bmaporgy+MAXRADIUS)>>MAPBLOCKSHIFT;
	block = block >= bmapheight ? bmapheight-1 : block;
	blockbox[BOXTOP]=block;

	block = (bbox[BOXBOTTOM]-bmaporgy-MAXRADIUS)>>MAPBLOCKSHIFT;
	block = block < 0 ? 0 : block;
	blockbox[BOXBOTTOM]=block;

	block = (bbox[BOXRIGHT]-bmaporgx+MAXRADIUS)>>MAPBLOCKSHIFT;
	block = block >= bmapwidth ? bmapwidth-1 : block;
	blockbox[BOXRIGHT]=block;

	block = (bbox[BOXLEFT]-bmaporgx-MAXRADIUS)>>MAPBLOCKSHIFT;
	block = block < 0 ? 0 : block;
	blockbox[BOXLEFT]=block;
}

/*
===============
=
= P_ChangeSector
=
===============
*/

boolean P_ChangeSector (sector_t *sector, boolean crunch)
{
	int			x,y;
	int			i;
	changetest_t ct;
	
/* force next sound to reflood */
	for (i=0 ; i<MAXPLAYERS ; i++)
		players[i].lastsoundsector = NULL;
		
	ct.nofit = false;
	ct.crushchange = crunch;
	
/* recheck heights for all things near the moving sector */
	VINT blockbox[4];
	CalculateSectorBlockBox(sector, blockbox);

	for (x=blockbox[BOXLEFT]; x<=blockbox[BOXRIGHT]; x++)
		for (y=blockbox[BOXBOTTOM]; y<=blockbox[BOXTOP]; y++)
			P_BlockThingsIterator (x, y, (blockthingsiter_t)PIT_ChangeSector, &ct);
	
	return ct.nofit;
}

