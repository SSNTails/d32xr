
/* P_maputl.c */

#include "doomdef.h"
#include "p_local.h"

fixed_t P_AproxDistance(fixed_t dx, fixed_t dy) ATTR_DATA_CACHE_ALIGN;
int P_PointOnLineSide(fixed_t x, fixed_t y, line_t* line) ATTR_DATA_CACHE_ALIGN;
int P_PointOnDivlineSide(fixed_t x, fixed_t y, divline_t* line) ATTR_DATA_CACHE_ALIGN;
int P_DivlineSide(fixed_t x, fixed_t y, divline_t *node) ATTR_DATA_CACHE_ALIGN;
fixed_t P_LineOpening(line_t* linedef) ATTR_DATA_CACHE_ALIGN;
void P_LineBBox(line_t* ld, fixed_t* bbox) ATTR_DATA_CACHE_ALIGN;
void P_UnsetThingPosition(mobj_t* thing) ATTR_DATA_CACHE_ALIGN;
void P_SetThingPosition(mobj_t* thing) ATTR_DATA_CACHE_ALIGN;
void P_SetThingPosition2(mobj_t* thing, VINT iss) ATTR_DATA_CACHE_ALIGN;
void P_SetThingPositionConditionally(mobj_t *thing, fixed_t x, fixed_t y, VINT iss) ATTR_DATA_CACHE_ALIGN;
boolean P_BlockLinesIterator(int x, int y, boolean(*func)(line_t*, void*), void *userp) ATTR_DATA_CACHE_ALIGN;
boolean P_BlockThingsIterator(int x, int y, boolean(*func)(mobj_t*, void*), void *userp) ATTR_DATA_CACHE_ALIGN;

vector4_t *FV4_Load(vector4_t *vec, fixed_t x, fixed_t y, fixed_t z, fixed_t a) ATTR_DATA_CACHE_ALIGN;
matrix_t *FM_RotateX(matrix_t *dest, angle_t rad) ATTR_DATA_CACHE_ALIGN;
matrix_t *FM_RotateZ(matrix_t *dest, angle_t rad) ATTR_DATA_CACHE_ALIGN;
const vector4_t *FM_MultMatrixVec4(const matrix_t *matrix, const vector4_t *vec, vector4_t *out) ATTR_DATA_CACHE_ALIGN;
vector4_t *FV4_Copy(vector4_t *a_o, const vector4_t *a_i) ATTR_DATA_CACHE_ALIGN;
vector3_t *FV3_Cross(const vector3_t *a_1, const vector3_t *a_2, vector3_t *a_o) ATTR_DATA_CACHE_ALIGN;
fixed_t FV3_Normalize(const vector3_t *a_normal, vector3_t *a_o); // Don't need this to be fast

vector4_t *FV4_Load(vector4_t *vec, fixed_t x, fixed_t y, fixed_t z, fixed_t a)
{
	vec->x = x;
	vec->y = y;
	vec->z = z;
	vec->a = a;
	return vec;
}

void FM_LoadIdentity(matrix_t* matrix)
{
#define M(row,col)  matrix->m[col * 4 + row]
	D_memset(matrix, 0, sizeof(matrix_t));

	M(0, 0) = FRACUNIT;
	M(1, 1) = FRACUNIT;
	M(2, 2) = FRACUNIT;
	M(3, 3) = FRACUNIT;
#undef M
}

#define M(row,col) dest->m[row * 4 + col]
matrix_t *FM_RotateX(matrix_t *dest, angle_t rad)
{
	const angle_t fa = rad>>ANGLETOFINESHIFT;
	const fixed_t cosrad = finecosine(fa), sinrad = finesine(fa);

	M(0, 0) = FRACUNIT;
	M(0, 1) = 0;
	M(0, 2) = 0;
	M(0, 3) = 0;
	M(1, 0) = 0;
	M(1, 1) = cosrad;
	M(1, 2) = sinrad;
	M(1, 3) = 0;
	M(2, 0) = 0;
	M(2, 1) = -sinrad;
	M(2, 2) = cosrad;
	M(2, 3) = 0;
	M(3, 0) = 0;
	M(3, 1) = 0;
	M(3, 2) = 0;
	M(3, 3) = FRACUNIT;

	return dest;
}

matrix_t *FM_RotateZ(matrix_t *dest, angle_t rad)
{
	const angle_t fa = rad>>ANGLETOFINESHIFT;
	const fixed_t cosrad = finecosine(fa), sinrad = finesine(fa);

	M(0, 0) = cosrad;
	M(0, 1) = sinrad;
	M(0, 2) = 0;
	M(0, 3) = 0;
	M(1, 0) = -sinrad;
	M(1, 1) = cosrad;
	M(1, 2) = 0;
	M(1, 3) = 0;
	M(2, 0) = 0;
	M(2, 1) = 0;
	M(2, 2) = FRACUNIT;
	M(2, 3) = 0;
	M(3, 0) = 0;
	M(3, 1) = 0;
	M(3, 2) = 0;
	M(3, 3) = FRACUNIT;

	return dest;
}
#undef M

const vector4_t *FM_MultMatrixVec4(const matrix_t *matrix, const vector4_t *vec, vector4_t *out)
{
#define M(row,col)  matrix->m[col * 4 + row]
	out->x = FixedMul(vec->x, M(0, 0))
		+ FixedMul(vec->y, M(0, 1))
		+ FixedMul(vec->z, M(0, 2))
		+ FixedMul(vec->a, M(0, 3));

	out->y = FixedMul(vec->x, M(1, 0))
		+ FixedMul(vec->y, M(1, 1))
		+ FixedMul(vec->z, M(1, 2))
		+ FixedMul(vec->a, M(1, 3));

	out->z = FixedMul(vec->x, M(2, 0))
		+ FixedMul(vec->y, M(2, 1))
		+ FixedMul(vec->z, M(2, 2))
		+ FixedMul(vec->a, M(2, 3));


	out->a = FixedMul(vec->x, M(3, 0))
		+ FixedMul(vec->y, M(3, 1))
		+ FixedMul(vec->z, M(3, 2))
		+ FixedMul(vec->a, M(3, 3));
#undef M
	return out;
}

vector4_t *FV4_Copy(vector4_t *a_o, const vector4_t *a_i)
{
	D_memcpy(a_o, a_i, sizeof(vector4_t));
	return a_o;
}

vector3_t *FV3_Cross(const vector3_t *a_1, const vector3_t *a_2, vector3_t *a_o)
{
	a_o->x = FixedMul(a_1->y, a_2->z) - FixedMul(a_1->z, a_2->y);
	a_o->y = FixedMul(a_1->z, a_2->x) - FixedMul(a_1->x, a_2->z);
	a_o->z = FixedMul(a_1->x, a_2->y) - FixedMul(a_1->y, a_2->x);
	return a_o;
}

fixed_t FixedSqrt(fixed_t x)
{
	// The neglected art of Fixed Point arithmetic
	// Jetro Lauha
	// Seminar Presentation
	// Assembly 2006, 3rd- 6th August 2006
	// (Revised: September 13, 2006)
	// URL: http://jet.ro/files/The_neglected_art_of_Fixed_Point_arithmetic_20060913.pdf
	uint32_t root, remHi, remLo, testDiv, count;
	root = 0;         /* Clear root */
	remHi = 0;        /* Clear high part of partial remainder */
	remLo = x;        /* Get argument into low part of partial remainder */
	count = (15 + (FRACBITS >> 1));    /* Load loop counter */
	do
	{
		remHi = (remHi << 2) | (remLo >> 30); remLo <<= 2;  /* get 2 bits of arg */
		root <<= 1;   /* Get ready for the next bit in the root */
		testDiv = (root << 1) + 1;    /* Test radical */
		if (remHi >= testDiv)
		{
			remHi -= testDiv;
			root += 1;
		}
	} while (count-- != 0);
	return root;
}

fixed_t FV3_Magnitude(const vector3_t *a_normal)
{
	fixed_t xs = FixedMul(a_normal->x, a_normal->x);
	fixed_t ys = FixedMul(a_normal->y, a_normal->y);
	fixed_t zs = FixedMul(a_normal->z, a_normal->z);
	return FixedSqrt(xs + ys + zs);
}

fixed_t FV3_Normalize(const vector3_t *a_normal, vector3_t *a_o)
{
	fixed_t magnitude = FV3_Magnitude(a_normal);
	a_o->x = FixedDiv(a_normal->x, magnitude);
	a_o->y = FixedDiv(a_normal->y, magnitude);
	a_o->z = FixedDiv(a_normal->z, magnitude);
	return magnitude;
}

fixed_t FV3_Dot(const vector3_t *a_1, const vector3_t *a_2)
{
	return (FixedMul(a_1->x, a_2->x) + FixedMul(a_1->y, a_2->y) + FixedMul(a_1->z, a_2->z));
}

/*
===================
=
= P_AproxDistance
=
= Gives an estimation of distance (not exact)
=
===================
*/

fixed_t P_AproxDistance (fixed_t dx, fixed_t dy)
{
	dx = D_abs(dx);
	dy = D_abs(dy);
	if (dx < dy)
		return dx+dy-(dx>>1);
	return dx+dy-(dy>>1);
}

fixed_t P_AproxDistance3D(fixed_t dx, fixed_t dy, fixed_t dz)
{
    dx = D_abs(dx);
    dy = D_abs(dy);
    dz = D_abs(dz);

    // First step: Compute P_AproxDistance(dx, dy)
    dx = dx < dy ? dx + dy - (dx >> 1) : dx + dy - (dy >> 1);

    // Second step: Compute P_AproxDistance(temp, dz)
    return dx < dz ? dx + dz - (dx >> 1) : dx + dz - (dz >> 1);
}

/*
==================
=
= P_PointOnLineSide
=
= Returns 0 or 1
==================
*/

int P_PointOnLineSide (fixed_t x, fixed_t y, line_t *line)
{
	fixed_t	dx,dy;
	fixed_t	left, right;
	fixed_t ldx,ldy;

	ldx = vertexes[line->v2].x - vertexes[line->v1].x;
	ldy = vertexes[line->v2].y - vertexes[line->v1].y;

	dx = x - (vertexes[line->v1].x << FRACBITS);
	dy = y - (vertexes[line->v1].y << FRACBITS);

#ifdef MARS
	dx = (unsigned)dx >> FRACBITS;
	__asm volatile(
		"muls.w %0,%1\n\t"
		: : "r"(ldy), "r"(dx) : "macl", "mach");
#else
	left = (ldy) * (dx>>16);
#endif

#ifdef MARS
	dy = (unsigned)dy >> FRACBITS;
	__asm volatile(
		"sts macl, %0\n\t"
		"muls.w %2,%3\n\t"
		"sts macl, %1\n\t"
		: "=&r"(left), "=&r"(right) : "r"(dy), "r"(ldx) : "macl", "mach");
#else
	right = (dy>>16) * (ldx);
#endif

	if (right < left)
		return 0;		/* front side */
	return 1;			/* back side */
}


/*
==================
=
= P_PointOnDivlineSide
=
= Returns 0 or 1
==================
*/

int P_PointOnDivlineSide (fixed_t x, fixed_t y, divline_t *line)
{
	fixed_t	dx,dy;
	fixed_t	left, right;
	
	dx = (x - line->x);
	dy = (y - line->y);
	
#ifndef MARS
/* try to quickly decide by looking at sign bits */
	if ( (line->dy ^ line->dx ^ dx ^ dy)&0x80000000 )
	{
		if ( (line->dy ^ dx) & 0x80000000 )
			return 1;	/* (left is negative) */
		return 0;
	}
#endif

	left = FixedMul(line->dy>>8, dx>>8);
	right = FixedMul(dy>>8, line->dx>>8);
	
	if (right < left)
		return 0;		/* front side */
	return 1;			/* back side */
}

//
// Returns side 0 (front), 1 (back), or 2 (on).
//
int P_DivlineSide(fixed_t x, fixed_t y, divline_t *node)
{
   fixed_t dx;
   fixed_t dy;
   fixed_t left;
   fixed_t right;
   dx = x - node->x;
   dy = y - node->y;
   left  = (node->dy>>FRACBITS) * (dx>>FRACBITS);
   right = (dy>>FRACBITS) * (node->dx>>FRACBITS);
   return (left <= right) + (left == right);
}

/*
==================
=
= P_LineOpening
=
= Sets opentop and openbottom to the window through a two sided line
= OPTIMIZE: keep this precalculated
==================
*/

fixed_t P_LineOpening (line_t *linedef)
{
	sector_t	*front, *back;
	fixed_t opentop, openbottom;
	
	if (linedef->sidenum[1] == -1)
	{	/* single sided line */
		return 0;
	}
	 
	front = LD_FRONTSECTOR(linedef);
	back = LD_BACKSECTOR(linedef);
	
	if (front->ceilingheight < back->ceilingheight)
		opentop = front->ceilingheight;
	else
		opentop = back->ceilingheight;
	if (front->floorheight > back->floorheight)
		openbottom = front->floorheight;
	else
		openbottom = back->floorheight;
	
	return opentop - openbottom;
}

void P_LineBBox(line_t* ld, fixed_t *bbox)
{
	mapvertex_t* v1 = &vertexes[ld->v1], * v2 = &vertexes[ld->v2];

	if (v1->x < v2->x)
	{
		bbox[BOXLEFT] = v1->x;
		bbox[BOXRIGHT] = v2->x;
	}
	else
	{
		bbox[BOXLEFT] = v2->x;
		bbox[BOXRIGHT] = v1->x;
	}
	if (v1->y < v2->y)
	{
		bbox[BOXBOTTOM] = v1->y;
		bbox[BOXTOP] = v2->y;
	}
	else
	{
		bbox[BOXBOTTOM] = v2->y;
		bbox[BOXTOP] = v1->y;
	}

	bbox[BOXTOP] <<= FRACBITS;
	bbox[BOXBOTTOM] <<= FRACBITS;
	bbox[BOXLEFT] <<= FRACBITS;
	bbox[BOXRIGHT] <<= FRACBITS;
}

/*
===============================================================================

						THING POSITION SETTING

===============================================================================
*/


static void P_UnlinkSubsector(mobj_t *thing)
{
	/* inert things don't need to be in blockmap */
	/* unlink from subsector */
	mobj_t *snext = SPTR_TO_LPTR(thing->snext);
	mobj_t *sprev = SPTR_TO_LPTR(thing->sprev);
	if (snext && !(((snext->flags & (MF_RINGMOBJ|MF_NOBLOCKMAP)) == (MF_RINGMOBJ|MF_NOBLOCKMAP))))
		snext->sprev = thing->sprev;
	if (sprev)
		sprev->snext = thing->snext;
	else
		sector_thinglist[subsectors[thing->isubsector].isector] = thing->snext;
}

void P_LinkSubsector(mobj_t *thing, VINT iss)
{
	VINT isector = subsectors[iss].isector;

	// re-link to new subsector
	if (!(((thing->flags & (MF_RINGMOBJ|MF_NOBLOCKMAP)) == (MF_RINGMOBJ|MF_NOBLOCKMAP))))
		thing->sprev = (SPTR)0;

	thing->snext = sector_thinglist[isector];
	mobj_t *firstOne = (mobj_t *)SPTR_TO_LPTR(sector_thinglist[isector]);
	if (sector_thinglist[isector] && !(((firstOne->flags & (MF_RINGMOBJ|MF_NOBLOCKMAP)) == (MF_RINGMOBJ|MF_NOBLOCKMAP))))
		firstOne->sprev = LPTR_TO_SPTR(thing);
	sector_thinglist[isector] = LPTR_TO_SPTR(thing);
}

static void P_UnlinkBlockmap(mobj_t *thing)
{
	/* inert things don't need to be in blockmap */
	/* unlink from block map */
	mobj_t *bnext = SPTR_TO_LPTR(thing->bnext);
	mobj_t *bprev = SPTR_TO_LPTR(thing->bprev);

	if (bnext)
		bnext->bprev = thing->bprev;
	if (bprev)
		bprev->bnext = thing->bnext;
	else
	{
		int blockx, blocky;
		
		if (thing->flags & MF_RINGMOBJ)
		{
			ringmobj_t *ring = (ringmobj_t*)thing;
			blockx = (ring->x << FRACBITS) - bmaporgx;
			blocky = (ring->y << FRACBITS) - bmaporgy;
		}
		else
		{
			blockx = thing->x - bmaporgx;
			blocky = thing->y - bmaporgy;
		}
		
		if (blockx >= 0 && blocky >= 0)
		{
			blockx = (unsigned)blockx >> MAPBLOCKSHIFT;
			blocky = (unsigned)blocky >> MAPBLOCKSHIFT;
			if (blockx < bmapwidth && blocky <bmapheight)
				blocklinks[blocky*bmapwidth+blockx] = thing->bnext;
		}
	}
}

static void P_LinkBlockmap(mobj_t *thing, fixed_t x, fixed_t y)
{
	int blockx = x - bmaporgx;
	int blocky = y - bmaporgy;

	if (blockx>=0 && blocky>=0)
	{
		blockx = (unsigned)blockx >> MAPBLOCKSHIFT;
		blocky = (unsigned)blocky >> MAPBLOCKSHIFT;
		if (blockx < bmapwidth && blocky <bmapheight)
		{
			SPTR *link = &blocklinks[blocky*bmapwidth+blockx];
			thing->bprev = (SPTR)0;
			thing->bnext = *link;
			if (*link)
				((mobj_t *)(SPTR_TO_LPTR(*link)))->bprev = LPTR_TO_SPTR(thing);
			*link = LPTR_TO_SPTR(thing);
		}
		else
		{	/* thing is off the map */
			thing->bnext = thing->bprev = (SPTR)0;
		}
	}
	else
	{	/* thing is off the map */
		thing->bnext = thing->bprev = (SPTR)0;
	}
}

/*
===================
=
= P_UnsetThingPosition 
=
= Unlinks a thing from block map and sectors
=
===================
*/

void P_UnsetThingPosition (mobj_t *thing)
{
	if ( ! (thing->flags & MF_NOSECTOR) )
		P_UnlinkSubsector(thing);
	
	if ( ! (thing->flags & MF_NOBLOCKMAP) )
		P_UnlinkBlockmap(thing);
}

/*
===================
=
= P_SetThingPosition2
=
= Links a thing into both a block and a subsector based on its x y
=
===================
*/
void P_SetThingPosition2 (mobj_t *thing,  VINT iss)
{
/* */
/* link into subsector */
/* */
	thing->isubsector = iss;

	if ( ! (thing->flags & MF_NOSECTOR) )
	{	/* invisible things don't go into the sector links */
		P_LinkSubsector(thing, iss);
	}
	
/* */
/* link into blockmap */
/* */
	if (!(thing->flags & MF_NOBLOCKMAP) )
	{	/* inert things don't need to be in blockmap		 */
		fixed_t x, y;

		if (thing->flags & MF_RINGMOBJ)
		{
			ringmobj_t *ring = (ringmobj_t*)thing;
			x = (ring->x << FRACBITS);
			y = (ring->y << FRACBITS);
		}
		else
		{
			x = thing->x;
			y = thing->y;
		}

		P_LinkBlockmap(thing, x, y);
	}
}

/*
===================
=
= P_SetThingPosition
=
= Links a thing into both a block and a subsector based on it's x y
= Sets thing->subsector properly
=
===================
*/

void P_SetThingPosition (mobj_t *thing)
{
	P_SetThingPosition2(thing, R_PointInSubsector2(thing->x,thing->y));
}

void P_SetThingPositionConditionally(mobj_t *thing, fixed_t x, fixed_t y, VINT iss)
{
	if (thing->isubsector != iss)
	{
		if ( ! (thing->flags & MF_NOSECTOR))
		{	
			P_UnlinkSubsector(thing);
			thing->isubsector = iss;
			P_LinkSubsector(thing, iss);
		}
	}

	if (!(thing->flags & MF_NOBLOCKMAP))
	{	/* inert things don't need to be in blockmap */
		P_UnlinkBlockmap(thing);
		P_LinkBlockmap(thing, x, y);
	}

	thing->x = x;
	thing->y = y;
}

/*
===============================================================================

						BLOCK MAP ITERATORS

For each line/thing in the given mapblock, call the passed function.
If the function returns false, exit with false without checking anything else.

===============================================================================
*/

/*
==================
=
= P_BlockLinesIterator
=
= The validcount flags are used to avoid checking lines
= that are marked in multiple mapblocks, so increment validcount before
= the first call to P_BlockLinesIterator, then make one or more calls to it
===================
*/

boolean P_BlockLinesIterator (int x, int y, blocklinesiter_t func, void *userp )
{
	int			offset;
	short		*list;
	line_t		*ld;
	VINT *lvalidcount = validcount;
	VINT 		vc;

	//if (x<0 || y<0 || x>=bmapwidth || y>=bmapheight)
	//	return true;
	offset = y*bmapwidth+x;
	
	offset = *(blockmaplump+4+offset);

	vc = *lvalidcount;
	++lvalidcount;

	for ( list = blockmaplump+offset ; *list != -1 ; list++)
	{
		int l = *list;
		ld = &lines[*list];

		if (lvalidcount[l] == vc)
			continue;		/* line has already been checked */
		lvalidcount[l] = vc;
		
		if ( !func(ld, userp) )
			return false;
	}
	
	return true;		/* everything was checked */
}


/*
==================
=
= P_BlockThingsIterator
=
==================
*/

boolean P_BlockThingsIterator (int x, int y, blockthingsiter_t func, void *userp )
{
	mobj_t		*mobj;
	
	//if (x<0 || y<0 || x>=bmapwidth || y>=bmapheight)
	//	return true;

	for (mobj = SPTR_TO_LPTR(blocklinks[y*bmapwidth+x]) ; mobj ; mobj = SPTR_TO_LPTR(mobj->bnext))
		if (!func( mobj, userp ) )
			return false;	

	return true;
}

uint8_t P_GetLineTag(line_t *line)
{
	if (!(ldflags[line-lines] & ML_HAS_SPECIAL_OR_TAG))
		return 0;

	for (int i = 0; i < numlinetags*2; i += 2)
	{
		if (linetags[i] == line-lines)
			return (uint8_t)linetags[i+1];
	}

	return 0;
}

uint8_t P_GetLineSpecial(line_t *line)
{
	if (!(ldflags[line-lines] & ML_HAS_SPECIAL_OR_TAG))
		return 0;

	for (int i = 0; i < numlinespecials*2; i += 2)
	{
		if (linespecials[i] == line-lines)
			return (uint8_t)linespecials[i+1];
	}

	return 0;
}
