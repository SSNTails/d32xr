/* P_main.c */

#include "doomdef.h"
#include "p_local.h"
#include "mars.h"
#include "p_camera.h"
#include "st_inter.h"

#define DEFAULT_GAME_ZONE_MARGIN (4*1024)
#define SPECIAL_STAGE_GAME_ZONE_MARGIN (8*1024)		// Guessing at this number...

uint16_t			numvertexes;
uint16_t			numsegs;
uint16_t			numsectors;
uint16_t            numstaticsectors;
uint16_t            numdynamicsectors;
uint16_t			numsubsectors;
uint16_t			numnodes;
uint16_t			numlines;
uint16_t			numsides;
uint16_t            numsidetexes;
VINT            numpolyobjects;

uint16_t 		numlineinfos;
lineinfo_t 		*lineinfos;

mapvertex_t	*vertexes;
seg_t		*segs;
sector_t	**dpsectors;
sector_t    *static_sectors;
sector_t    *dynamic_sectors;
SPTR        *sector_thinglist;
subsector_t	*subsectors;
node_t		*nodes;
line_t		*lines;
uint16_t    *ldflags;
side_t		*sides;
sidetex_t   *sidetexes;

short		*blockmaplump;			/* offsets in blockmap are from here */
VINT		bmapwidth, bmapheight;	/* in mapblocks */
fixed_t		bmaporgx, bmaporgy;		/* origin of block map */
SPTR		*blocklinks;			/* for thing chains */

byte		*rejectmatrix;			/* for fast sight rejection */

int8_t		*validcount;			/* increment every time a check is made */

mapthing_t	playerstarts[MAXPLAYERS];

uint16_t		numthings;

int16_t worldbbox[4];

#define LOADFLAGS_VERTEXES 1
#define LOADFLAGS_BLOCKMAP 2
#define LOADFLAGS_REJECT 4
#define LOADFLAGS_NODES 8
#define LOADFLAGS_SEGS 16
#define LOADFLAGS_LINEDEFS 32
#define LOADFLAGS_SUBSECTORS 64
#define LOADFLAGS_SECTORS 128
#define LOADFLAGS_SIDEDEFS_ROM 256

/*
=================
=
= P_LoadVertexes
=
=================
*/

void P_LoadVertexes (int lump)
{
	numvertexes = W_LumpLength (lump) / sizeof(mapvertex_t);

	if (gamemapinfo.loadFlags & LOADFLAGS_VERTEXES)
	{
		vertexes = Z_Malloc (numvertexes*sizeof(mapvertex_t) + 16,PU_LEVEL);
		vertexes = (void*)(((uintptr_t)vertexes + 15) & ~15); // aline on cacheline boundary
		W_ReadLump(lump, vertexes);
	}
	else
		vertexes = (mapvertex_t*)W_POINTLUMPNUM(lump);
}


/*
=================
=
= P_LoadSegs
=
=================
*/

void P_LoadSegs (int lump)
{
	numsegs = W_LumpLength (lump) / sizeof(seg_t);

	if (gamemapinfo.loadFlags & LOADFLAGS_SEGS)
	{
		segs = Z_Malloc (numsegs*sizeof(seg_t) + 16,PU_LEVEL);
		segs = (void*)(((uintptr_t)segs + 15) & ~15); // aline on cacheline boundary
		W_ReadLump(lump, segs);
	}
	else
		segs = (seg_t *)W_POINTLUMPNUM(lump);
}


/*
=================
=
= P_LoadSubsectors
=
=================
*/

void P_LoadSubsectors (int lump)
{
	numsubsectors = W_LumpLength (lump) / sizeof(mapsubsector_t);

	if (gamemapinfo.loadFlags & LOADFLAGS_SUBSECTORS)
	{
		subsectors = Z_Malloc ((numsubsectors)*sizeof(subsector_t) + 16,PU_LEVEL);
		subsectors = (void*)(((uintptr_t)subsectors + 15) & ~15); // aline on cacheline boundary
		W_ReadLump(lump, subsectors);
	}
	else
		subsectors = (subsector_t*)W_POINTLUMPNUM(lump);

	numsubsectors--;
}

/*
=================
=
= P_LoadSectors
=
=================
*/

void P_LoadSectors (int staticlump, int dynamiclump)
{
	byte			*data;
	int				i;
	mapsector_t		*ms;
	sector_t		*ss;

	// LOADFLAGS_SECTORS is actually handled at compile-time
	numstaticsectors = W_LumpLength(staticlump) / sizeof(sector_t);
	numdynamicsectors = W_LumpLength(dynamiclump) / sizeof(mapsector_t);
	numsectors = numstaticsectors + numdynamicsectors;
	static_sectors = W_POINTLUMPNUM(staticlump);

	sector_thinglist = Z_Malloc(numsectors*sizeof(SPTR) + 16, PU_LEVEL);
	sector_thinglist = (void*)(((uintptr_t)sector_thinglist + 15) & ~15); // aline on cacheline boundary
	D_memset(sector_thinglist, 0, numsectors*sizeof(SPTR));

	// Read in the dynamic sectors
	if (numdynamicsectors > 0)
	{
		dynamic_sectors = Z_Malloc(numdynamicsectors*sizeof(sector_t) + 16, PU_LEVEL);
		dynamic_sectors = (void*)(((uintptr_t)dynamic_sectors + 15) & ~15); // aline on cacheline boundary
		D_memset (dynamic_sectors, 0, numdynamicsectors*sizeof(sector_t));

		data = I_TempBuffer();
		W_ReadLump(dynamiclump, data);
		
		ms = (mapsector_t *)data;
		ss = dynamic_sectors;
		for (i=0 ; i<numdynamicsectors ; i++, ss++, ms++)
		{
			ss->floorheight = (ms->floorheight)<<FRACBITS;
			ss->ceilingheight = (ms->ceilingheight)<<FRACBITS;
			ss->floorpic = ms->floorpic;
			ss->ceilingpic = ms->ceilingpic;

			ss->lightlevel = ms->lightlevel;
			ss->special = ms->special;

			ss->tag = ms->tag;
			ss->heightsec = -1; // sector used to get floor and ceiling height
			ss->fofsec = -1;
			ss->specline = -1;
			ss->floor_xoffs = 0;
			ss->flags = 0;
		}
	}

	// Now create the double pointers to the sectors
	dpsectors = Z_Malloc(numsectors*sizeof(sector_t*) + 16, PU_LEVEL);
	dpsectors = (void*)(((uintptr_t)dpsectors + 15) & ~15); // aline on cacheline boundary

	int dpCount = 0;
	for (i = 0; i < numstaticsectors; i++)
		dpsectors[dpCount++] = &static_sectors[i];

	for (i = 0; i < numdynamicsectors; i++)
		dpsectors[dpCount++] = &dynamic_sectors[i];
}


/*
=================
=
= P_LoadNodes
=
=================
*/

void P_LoadNodes (int lump)
{
	numnodes = W_LumpLength (lump) / sizeof(node_t);

	if (gamemapinfo.loadFlags & LOADFLAGS_NODES)
	{
		nodes = Z_Malloc (numnodes*sizeof(node_t) + 16,PU_LEVEL);
		nodes = (void*)(((uintptr_t)nodes + 15) & ~15); // aline on cacheline boundary
		W_ReadLump(lump, nodes);
	}
	else
		nodes = (node_t *)W_POINTLUMPNUM(lump);

	// Calculate worldbbox
	worldbbox[BOXLEFT] = INT16_MAX;
	worldbbox[BOXRIGHT] = INT16_MIN;
	worldbbox[BOXBOTTOM] = INT16_MAX;
	worldbbox[BOXTOP] = INT16_MIN;
	for (int i = 0; i < numvertexes; i++)
	{
		const mapvertex_t *v = &vertexes[i];
		if (v->x < worldbbox[BOXLEFT])
			worldbbox[BOXLEFT] = v->x;
		if (v->x > worldbbox[BOXRIGHT])
			worldbbox[BOXRIGHT] = v->x;
		if (v->y < worldbbox[BOXBOTTOM])
			worldbbox[BOXBOTTOM] = v->y;
		if (v->y > worldbbox[BOXTOP])
			worldbbox[BOXTOP] = v->y;
	}
}

/*
=================
=
= P_LoadThings
=
=================
*/
fixed_t P_GetMapThingSpawnHeight(const mobjtype_t mobjtype, const mapthing_t* mthing, const fixed_t x, const fixed_t y, const fixed_t z);

static void P_SpawnItemRow(mapthing_t *mt, VINT type, VINT count, VINT horizontalSpacing, VINT verticalSpacing)
{
	mapthing_t dummything = *mt;
	dummything.type = type;

	int mobjtype;
	/* find which type to spawn */
	for (mobjtype=0 ; mobjtype< NUMMOBJTYPES ; mobjtype++)
		if (mt->type == mobjinfo[mobjtype].doomednum)
			break;

	fixed_t spawnX = dummything.x << FRACBITS;
	fixed_t spawnY = dummything.y << FRACBITS;
	fixed_t spawnZ = P_GetMapThingSpawnHeight(mobjtype, mt, spawnX, spawnY, (mt->options >> 4) << FRACBITS);

	angle_t spawnAngle = dummything.angle * ANGLE_1;

	for (int i = 0; i < count ; i++)
	{
		P_ThrustValues(spawnAngle, horizontalSpacing << FRACBITS, &spawnX, &spawnY);

		const sector_t *sec = SS_SECTOR(R_PointInSubsector2(spawnX, spawnY));

		fixed_t curZ = spawnZ + ((i+1) * (verticalSpacing<<FRACBITS)); // Literal height

		// Now we have to work backwards and calculate the mapthing z
		VINT mapthingZ = (curZ - sec->floorheight) >> FRACBITS;
		mapthingZ <<= 4;
		dummything.options &= 15; // Clear the top 12 bits
		dummything.options |= mapthingZ; // Insert our new value

		dummything.x = spawnX >> FRACBITS;
		dummything.y = spawnY >> FRACBITS;

		P_SpawnMapThing(&dummything, mobjtype);
	}
}

typedef struct
{
	short		x,y;
	short		angle;
	short		type;
	short		options;
} mapmapthing_t; // Lol, only needed here

static short P_GetMaceLinkCount(mapthing_t *mthing)
{
	int tag = mthing->angle;
	line_t *line = NULL;
	for (int i = 0; i < numlines; i++)
	{
		int lineTag = P_GetLineTag(&lines[i]);
		int lineSpecial = P_GetLineSpecial(&lines[i]);

		if (lineSpecial == 9 && lineTag == tag)
		{
			line = &lines[i];
			break;
		}
	}

	if (!line)
		return 0;

	if (line->sidenum[1] < 0) // Must be an old unconverted one
		return 0;

	const mapvertex_t *v1 = &vertexes[line->v1];
	const mapvertex_t *v2 = &vertexes[line->v2];

	sector_t *frontsector = I_TO_SEC(sides[line->sidenum[0]].sector);
	VINT mlength = D_abs(v1->x - v2->x);

	VINT msublinks = frontsector->lightlevel; // number of links to subtract from the inside.
	if (msublinks > mlength)
		msublinks = mlength;

	return mlength - msublinks; // # of links
}

static void P_SetupMace(mapthing_t *mthing)
{
	VINT args[10];
	int tag = mthing->angle;
	line_t *line = NULL;
	for (int i = 0; i < numlines; i++)
	{
		int lineTag = P_GetLineTag(&lines[i]);
		int lineSpecial = P_GetLineSpecial(&lines[i]);

		if (lineSpecial == 9 && lineTag == tag)
		{
			line = &lines[i];
			break;
		}
	}

	if (!line)
		return;

	if (line->sidenum[1] < 0) // Must be an old unconverted one
		return;

	D_memset(args, 0, sizeof(args));

	vector3_t axis, rotation;

	sector_t *frontsector = I_TO_SEC(sides[line->sidenum[0]].sector);
	const mapvertex_t *v1 = &vertexes[line->v1];
	const mapvertex_t *v2 = &vertexes[line->v2];
//	const VINT angle = frontsector->ceilingheight >> FRACBITS;
//	const VINT pitch = frontsector->floorheight >> FRACBITS;
//	VINT roll = 0;
	VINT textureoffset = sides[line->sidenum[0]].textureoffset & 0xfff;
    textureoffset <<= 4; // sign extend
    textureoffset >>= 4; // sign extend
    VINT rowoffset = (sides[line->sidenum[0]].textureoffset & 0xf000) | ((unsigned)sides[line->sidenum[0]].rowoffset << 4);
    rowoffset >>= 4; // sign extend

	axis.x = textureoffset;
	axis.y = rowoffset;
	axis.z = frontsector->floorheight >> FRACBITS;

	args[0] = D_abs(v1->x - v2->x); // # of links
	args[1] = mthing->type >> 12;
	args[3] = D_abs(v1->y - v2->y);
	args[4] = textureoffset;
	args[7] = frontsector->lightlevel; // number of links to subtract from the inside.

	if (line->sidenum[1] >= 0)
	{
		sector_t *backsector = I_TO_SEC(sides[line->sidenum[1]].sector);
		VINT backtextureoffset = sides[line->sidenum[1]].textureoffset & 0xfff;
		backtextureoffset <<= 4; // sign extend
		backtextureoffset >>= 4; // sign extend
		VINT backrowoffset = (sides[line->sidenum[1]].textureoffset & 0xf000) | ((unsigned)sides[line->sidenum[1]].rowoffset << 4);
		backrowoffset >>= 4; // sign extend

//		roll = backsector->ceilingheight >> FRACBITS;
		args[2] = backrowoffset;
		args[5] = backsector->floorheight >> FRACBITS;
		args[6] = backtextureoffset;
		args[9] = backsector->lightlevel << 1; // Swing speed

		rotation.x = backtextureoffset;
		rotation.y = backrowoffset;
		rotation.z = backsector->floorheight >> FRACBITS;
	}

	if (mthing->options & MTF_AMBUSH)
		args[8] |= TMM_DOUBLESIZE;
	if (mthing->options & MTF_OBJECTSPECIAL)
		args[8] |= TMM_SILENT;
	if (ldflags[line-lines] & ML_NOCLIMB)
		args[8] |= TMM_ALLOWYAWCONTROL;
	if (ldflags[line-lines] & ML_CULLING)
		args[8] |= TMM_SWING;
	if (ldflags[line-lines] & ML_UNDERWATERONLY)
		args[8] |= TMM_MACELINKS;
	if (ldflags[line-lines] & ML_CULL_MIDTEXTURE)
		args[8] |= TMM_CENTERLINK;
	if (ldflags[line-lines] & ML_MIDTEXTUREBLOCK)
		args[8] |= TMM_CLIP;
	if (ldflags[line-lines] & ML_UNUSED2_WRAPMIDTEX)
		args[8] |= TMM_ALWAYSTHINK;

//	if (tag == 120)
//		args[8] |= TMM_ALWAYSTHINK;

	// Whew! We gathered all of the info. Let's do something with it, now.
	P_AddMaceChain(mthing, &axis, &rotation, args);
}

#define BAREBONESMAP
//#define BAREBONESMAP_NOENEMIES

void P_LoadThings (int lump)
{
	byte			*data;
	int				i;
	mapthing_t		*mt;
	short			numthingsreal, numstaticthings, numringthings;

	ringmobjstates = Z_Malloc(sizeof(*ringmobjstates) * NUMMOBJTYPES, PU_LEVEL);
	ringmobjtics = Z_Malloc(sizeof(*ringmobjtics) * NUMMOBJTYPES, PU_LEVEL);

	for (int i = 0; i < NUMMOBJTYPES; i++)
	{
		ringmobjstates[i] = mobjinfo[i].spawnstate;
		ringmobjtics[i] = states[mobjinfo[i].spawnstate].tics;
	}

	data = I_TempBuffer();
	numthings = W_LumpLength (lump) / sizeof(mapmapthing_t);
	numthingsreal = 0;
	numstaticthings = 0;
	numringthings = 0;
	numscenerymobjs = 0;

	mapmapthing_t *mmt = (mapmapthing_t*)(data + (sizeof(mapthing_t) * numthings));
	W_ReadLump(lump, mmt);

	mt = (mapthing_t *)data;
	for (i=0 ; i<numthings ; i++, mt++, mmt++)
	{
		mt->x = LITTLESHORT(mmt->x);
		mt->y = LITTLESHORT(mmt->y);
		mt->angle = LITTLESHORT(mmt->angle);
		mt->type = LITTLESHORT(mmt->type);
		mt->options = LITTLESHORT(mmt->options);
		mt->extrainfo = mt->type >> 12;
		mt->type &= 4095;
	}

	int numMaces = 0;

	mt = (mapthing_t *)data;
	for (i=0 ; i<numthings ; i++, mt++)
	{
#ifdef BAREBONESMAP_NOENEMIES
		if (mt->type >= 100 && mt->type < 200)
			continue; // Don't count
#endif

		if (mt->type == 1104 || mt->type == 1105 || mt->type == 1107) // Mace points
		{
#ifndef BAREBONESMAP
			// Determine the # of objects that will be spawned
			int maceLinkCount = P_GetMaceLinkCount(mt);

			if (maceLinkCount > 0)
				numringthings += maceLinkCount; // links

			numringthings++; // End of chain (ball)
			numMaces++;
#endif
		}
		else
		{
			switch (P_MapThingSpawnsMobj(mt))
			{
				case 0:
					break;
				case 1:
					numthingsreal++;
					if (mt->type == 118)
						numthingsreal++; // Jet fume
					else if (mt->type == 119) // MT_EGGGUARD
						numringthings++;
					break;
				case 2:
					numstaticthings++;
					break;
				case 3:
					if (mt->type >= 600 && mt->type <= 602)
						numringthings += 5;
					else if (mt->type == 603)
						numringthings += 10;
					else
						numringthings++;
					break;
				case 4:
					numscenerymobjs++;
					break;
			}
		}
	}

	// preallocate a few mobjs for puffs and projectiles
	numthingsreal += 48; // 32 rings, plus other items
	numstaticthings += 8;

	if (gamemapinfo.act == 3)
	{
		// Bosses spawn lots of stuff, so preallocate more.
		numthingsreal += 512;
		numstaticthings += 256;
	}

	P_PreSpawnMobjs(numthingsreal, numstaticthings, numringthings, numscenerymobjs);

	// Pre-allocate maces, too.
	P_PreallocateMaces(numMaces);

	mt = (mapthing_t *)data;
	for (i=0 ; i<numthings ; i++, mt++)
	{
#ifdef BAREBONESMAP_NOENEMIES
		if (mt->type >= 100 && mt->type < 200)
			continue; // Don't count
#endif
		if (mt->type == 600) // 5 vertical rings (yellow spring)
			P_SpawnItemRow(mt, mobjinfo[MT_RING].doomednum, 5, 0, 64);
		else if (mt->type == 601) // 5 vertical rings (red spring)
			P_SpawnItemRow(mt, mobjinfo[MT_RING].doomednum, 5, 0, 128);
		else if (mt->type == 602) // 5 diagonal rings (yellow spring)
			P_SpawnItemRow(mt, mobjinfo[MT_RING].doomednum, 5, 64, 64);
		else if (mt->type == 603) // 10 diagonal rings (yellow spring)
			P_SpawnItemRow(mt, mobjinfo[MT_RING].doomednum, 10, 64, 64);
		else if (mt->type == 1104 || mt->type == 1105 || mt->type == 1107) // Mace points
		{
#ifndef BAREBONESMAP
			P_SetupMace(mt);
#endif
		}
		else
			P_SpawnMapThing(mt, i);
	}

	camBossMobj = P_FindFirstMobjOfType(MT_EGGMOBILE);
	if (!camBossMobj)
		camBossMobj = P_FindFirstMobjOfType(MT_EGGMOBILE2);

	if (players[0].starpostnum)
		P_SetStarPosts(players[0].starpostnum + 1);
}

/*
=================
=
= P_LoadLineDefs
=
= Also counts secret lines for intermissions
=================
*/

void P_LoadLineDefs (int lump)
{
	int				i;
	line_t			*ld;
	mapvertex_t		*v1, *v2;
	
	numlines = W_LumpLength (lump) / sizeof(maplinedef_t);

	if (gamemapinfo.loadFlags & LOADFLAGS_LINEDEFS)
	{
		lines = Z_Malloc (numlines*sizeof(line_t)+16,PU_LEVEL);
		lines = (void*)(((uintptr_t)lines + 15) & ~15); // aline on cacheline boundary
		W_ReadLump(lump, lines);
	}
	else
		lines = (line_t *)W_POINTLUMPNUM(lump);

	// LDFlags always live in RAM. Lump comes directly after LINEDEFS
	ldflags = Z_Malloc(numlines*sizeof(uint16_t)+16, PU_LEVEL);
	ldflags = (void*)(((uintptr_t)ldflags + 15) & ~15); // aline on cacheline boundary
	D_memset (ldflags, 0, numlines*sizeof(uint16_t));
	byte *ldData = I_TempBuffer ();
	W_ReadLump (lump + 1, ldData);

	int numlinetags = 0;
	int numlinespecials = 0;
	numlineinfos = 0;

	uint16_t *ldFlagsPtr = ldflags;
	mapldflags_t *mapldFlags = (mapldflags_t*)ldData;
	ld = lines;
	for (i=0 ; i<numlines ; i++, ld++, mapldFlags++, ldFlagsPtr++)
	{
		uint8_t tag = mapldFlags->tag;
		uint8_t special = mapldFlags->special;
		uint16_t flags = mapldFlags->flags;

		fixed_t dx,dy;
		v1 = &vertexes[ld->v1];
		v2 = &vertexes[ld->v2];
		dx = (v2->x - v1->x) << FRACBITS;
		dy = (v2->y - v1->y) << FRACBITS;
		if (!dx)
			flags |= ML_ST_VERTICAL;
		else if (!dy)
			flags |= ML_ST_HORIZONTAL;
		else
		{
			if (FixedDiv (dy , dx) > 0)
				flags |= ML_ST_POSITIVE;
			else
				flags |= ML_ST_NEGATIVE;
		}

#define ML_TWOSIDED 4
/*
		// if the two-sided flag isn't set, set the back side to -1
		if (ld->sidenum[1] >= 0) {
			if (!(flags & ML_TWOSIDED)) {
				ld->sidenum[1] = -1;
			}
		}*/
		flags &= ~ML_TWOSIDED;
#undef ML_TWOSIDED

		if (tag > 0 || special > 0)
			flags |= ML_HAS_SPECIAL_OR_TAG;

		if (tag)
			numlinetags++;

		if (special)
			numlinespecials++;

		if (tag || special)
			numlineinfos++;

		*ldFlagsPtr = flags;
	}

	if (numlineinfos)
	{
		lineinfos = Z_Malloc(sizeof(*lineinfos)*numlineinfos, PU_LEVEL);
		lineinfo_t *lineinfoPtr = lineinfos;
		mapldFlags = (mapldflags_t*)ldData;
		for (i = 0; i < numlines; i++, mapldFlags++)
		{
			if (mapldFlags->special || mapldFlags->tag)
			{
				lineinfoPtr->line = i;
				lineinfoPtr->special = mapldFlags->special;
				lineinfoPtr->tag = mapldFlags->tag;
				lineinfoPtr++;
			}
		}
	}
}


/*
=================
=
= P_LoadSideDefs
=
=================
*/

void P_LoadSideDefs (int lump)
{
	numsides = W_LumpLength (lump) / sizeof(mapsidedef_t);

	if (gamemapinfo.loadFlags & LOADFLAGS_SIDEDEFS_ROM)
		sides = (side_t*)W_POINTLUMPNUM(lump);
	else
	{
		sides = Z_Malloc (numsides*sizeof(side_t)+16,PU_LEVEL);
		sides = (void*)(((uintptr_t)sides + 15) & ~15); // aline on cacheline boundary
		W_ReadLump(lump, sides);
	}
}

void P_LoadSideTexes(int lump)
{
	numsidetexes = W_LumpLength (lump) / sizeof(sidetex_t);
	sidetexes = Z_Malloc (numsidetexes*sizeof(sidetex_t)+16,PU_LEVEL);
	sidetexes = (void*)(((uintptr_t)sidetexes + 15) & ~15); // aline on cacheline boundary
	W_ReadLump(lump, sidetexes);
}

/*
=================
=
= P_LoadBlockMap
=
=================
*/

void P_LoadBlockMap (int lump)
{
	int		count;

	if (gamemapinfo.loadFlags & LOADFLAGS_BLOCKMAP)
	{
		blockmaplump = Z_Malloc (W_LumpLength(lump) + 16,PU_LEVEL);
		blockmaplump = (void*)(((uintptr_t)blockmaplump + 15) & ~15); // aline on cacheline boundary
		W_ReadLump(lump, blockmaplump);
	}
	else
		blockmaplump = (short *)W_POINTLUMPNUM(lump);

	bmaporgx = blockmaplump[0]<<FRACBITS;
	bmaporgy = blockmaplump[1]<<FRACBITS;
	bmapwidth = blockmaplump[2];
	bmapheight = blockmaplump[3];
	
/* clear out mobj chains */
	count = sizeof(*blocklinks)* bmapwidth*bmapheight;
	blocklinks = Z_Malloc (count,PU_LEVEL);
	D_memset (blocklinks, 0, count);
}

void P_LoadReject (int lump)
{
	if (gamemapinfo.loadFlags & LOADFLAGS_REJECT)
	{
		rejectmatrix = Z_Malloc (W_LumpLength(lump) + 16,PU_LEVEL);
		rejectmatrix = (void*)(((uintptr_t)rejectmatrix + 15) & ~15); // aline on cacheline boundary
		W_ReadLump(lump, rejectmatrix);
	}
	else
		rejectmatrix = (byte *)W_POINTLUMPNUM(lump);

}


/*
=================
=
= P_GroupLines
=
= Builds sector line lists and subsector sector numbers
= Finds block bounding boxes for sectors
=================
*/

void P_GroupLines (void)
{
//	VINT		*linebuffer;
//	int			i;//, total;
//	sector_t	*sector;
//	subsector_t	*ss;
//	seg_t		*seg;
//	line_t		*li;

/* look up sector number for each subsector */
/*
	ss = subsectors;
	for (i=0 ; i<numsubsectors ; i++, ss++)
	{
		side_t *sidedef;
		line_t* linedef;

		seg = &segs[ss->firstline];
		linedef = &lines[seg->linedef];
		sidedef = &sides[linedef->sidenum[seg->sideoffset & 1]];
		ss->isector = sidedef->sector;
	}*/
/*
// count number of lines in each sector
	li = lines;
	total = 0;
	for (i=0 ; i<numlines ; i++, li++)
	{
		sector_t *front = LD_FRONTSECTOR(li);
		sector_t *back = LD_BACKSECTOR(li);

		front->linecount++;
		total++;
		if (back && back != front)
		{
			back->linecount++;
			total++;
		}
	}
	
// build line tables for each sector
	linebuffer = Z_Malloc (total*sizeof(*linebuffer), PU_LEVEL);
	sector = sectors;
	for (i=0 ; i<numsectors ; i++, sector++)
	{
		sector->lines = linebuffer;
		linebuffer += sector->linecount;
		sector->linecount = 0;
	}

	li = lines;
	for (i=0 ; i<numlines ; i++, li++)
	{
		sector_t *front = LD_FRONTSECTOR(li);
		sector_t *back = LD_BACKSECTOR(li);

		front->lines[front->linecount++] = i;
		if (back && back != front)
			back->lines[back->linecount++] = i;
	}*/
}

/*============================================================================= */

/*
=================
=
= P_SetupLevel
=
=================
*/

#ifdef BENCHMARK
extern int benchcounter;
#endif

void P_SetupLevel (int lumpnum)
{
#ifndef MARS
	mobj_t	*mobj;
#endif
	extern	VINT	cy;
	int gamezonemargin;
	
	M_ClearRandom ();

	stagefailed = true;
	leveltime = 0;
	numpolyobjects = 0;
	polyobjects = NULL;
	
D_printf ("P_SetupLevel(%i)\n",lumpnum);

	P_InitThinkers ();

	R_ResetTextures();

/* note: most of this ordering is important	 */
	P_LoadBlockMap (lumpnum+ML_BLOCKMAP);
	P_LoadVertexes (lumpnum+ML_VERTEXES);
	P_LoadSectors (lumpnum+ML_SECTORS, lumpnum+ML_DSECTORS);
	P_LoadSideDefs (lumpnum+ML_SIDEDEFS);
	P_LoadSideTexes (lumpnum+ML_SIDETEX);
	P_LoadLineDefs (lumpnum+ML_LINEDEFS);
	P_LoadSegs (lumpnum+ML_SEGS);
	P_LoadSubsectors (lumpnum+ML_SSECTORS);
	P_LoadNodes (lumpnum+ML_NODES);
	P_LoadReject(lumpnum+ML_REJECT);

	validcount = Z_Malloc((numlines + 1) * sizeof(*validcount), PU_LEVEL);
	D_memset(validcount, 0, (numlines + 1) * sizeof(*validcount));
	validcount[0] = 1; // cpu 0

	P_GroupLines();

/* set up world state */
#ifndef BAREBONESMAP
	P_SpawnSpecials ();
#endif
	P_LoadThings (lumpnum+ML_THINGS);

	ST_InitEveryLevel ();
	
/*I_Error("free memory: 0x%x\n", Z_FreeMemory(mainzone)); */
/*printf ("free memory: 0x%x\n", Z_FreeMemory(mainzone)); */

	cy = 4;

#ifdef BENCHMARK
	benchcounter = 0;
#endif

#ifdef JAGUAR
{
extern byte *debugscreen;
	D_memset (debugscreen,0,32*224);
	
}
#endif

	gamepaused = false;

	if (IsLevelType(LevelType_SpecialStage)) {
		gamezonemargin = SPECIAL_STAGE_GAME_ZONE_MARGIN;
	}
	else {
		gamezonemargin = DEFAULT_GAME_ZONE_MARGIN;
	}

	R_SetupLevel(gamezonemargin, gamemapinfo.sky);

#ifdef MARS
	Mars_CommSlaveClearCache();
#endif
}


/*
=================
=
= P_Init
=
=================
*/

void P_Init (void)
{
	anims = Z_Malloc(sizeof(*anims) * MAXANIMS, PU_STATIC);
	linespeciallist = Z_Malloc(sizeof(*linespeciallist) * MAXLINEANIMS, PU_STATIC);

	P_InitPicAnims ();
#ifndef MARS
	pausepic = W_CacheLumpName ("PAUSED",PU_STATIC);
#endif
}
