/* P_spec.h */

/*
===============================================================================

							P_SPEC

===============================================================================
*/

/* */
/*	Animating textures and planes */
/* */
typedef struct
{
	char	istexture;
	char	numpics;
	uint8_t	picnum;
	uint8_t	basepic;
	char	current;
} anim_t;

/* */
/*	source animation definition */
/* */
typedef struct
{
	char	istexture;		/* if false, it's a flat */
	char	endname[9];
	char	startname[9];
} animdef_t;

#define	MAXANIMS		16

extern	anim_t	*anims/*[MAXANIMS]*/, * lastanim;


/* */
/*	Animating line specials */
/* */
#define	MAXLINEANIMS		64
extern	VINT		numlineanimspecials;
extern	VINT	*linespeciallist/*[MAXLINEANIMS]*/;


/*	Define values for map objects */
#define	MO_TELEPORTMAN		14


/* at game start */
void	P_InitPicAnims (void);

/* at map load */
void	P_SpawnSpecials (void);

/* every tic */
void 	P_UpdateSpecials (int8_t numframes);

void 	P_PlayerInSpecialSector (player_t *player);

VINT P_FindLowestFloorSurrounding(VINT sec);
VINT P_FindHighestFloorSurrounding(VINT sec);
VINT P_FindNextHighestFloor(VINT sec, fixed_t currentheight);
VINT P_FindNextLowestFloor(VINT sec, fixed_t currentheight);
VINT P_FindNextHighestCeiling(VINT sec, fixed_t currentheight);
VINT P_FindLowestCeilingSurrounding(VINT sec);
VINT P_FindHighestCeilingSurrounding(VINT sec);
VINT P_FindSectorWithTag(VINT tag, int start);
int		P_FindSectorFromLineTag(line_t	*line,int start);
int     P_FindSectorFromLineTagNum(uint8_t tag,int start);
int		P_FindMinSurroundingLight(VINT isector,int max);
VINT getNextSector(line_t *line, VINT sec);

/*
===============================================================================

							P_LIGHTS

===============================================================================
*/
typedef struct
{
	thinker_t	thinker;
	sector_t	*sector;
	int			count;
	VINT		maxlight;
	VINT		minlight;
	VINT		maxtime;
	VINT		mintime;
} lightflash_t;

typedef struct
{
	thinker_t	thinker;
	sector_t	*sector;
	int			count;
	VINT		minlight;
	VINT		maxlight;
	VINT		darktime;
	VINT		brighttime;
} strobe_t;

typedef struct
{
	thinker_t	thinker;
	sector_t	*sector;
	VINT		minlight;
	VINT		maxlight;
	int			direction;
} glow_t;

typedef struct
{
    thinker_t	thinker;
    sector_t*	sector;
    VINT		count;
    VINT		maxlight;
    VINT		minlight;
} fireflicker_t;

#define GLOWSPEED		16
#define	STROBEBRIGHT	3
#define	FASTDARK		8
#define	SLOWDARK		15

/*
===============================================================================

							P_SWITCH

===============================================================================
*/

#define	MAXSWITCHES	50		/* max # of wall switches in a level */
#define	MAXBUTTONS	16		/* 4 players, 4 buttons each at once, max. */
#define BUTTONTIME	15		/* 1 second */

/*
===============================================================================

							P_PLATS

===============================================================================
*/
typedef enum
{
	up,
	down,
	waiting,
	in_stasis
} plat_e;

typedef enum
{
	perpetualRaise,
	downWaitUpStay,
	raiseAndChange,
	raiseToNearestAndChange,
	blazeDWUS
} plattype_e;

typedef struct
{
	thinker_t	thinker;
	sector_t	*sector;
	fixed_t		speed;
	fixed_t		low;
	fixed_t		high;
	VINT		wait;
	VINT		count;
	char		status;
	char		oldstatus;
	VINT		crush;
	VINT		tag;
	VINT		type;
} plat_t;

#define	PLATWAIT	3*2/THINKERS_TICS			/* seconds */
#define	PLATSPEED	(FRACUNIT*THINKERS_TICS)
#define	MAXPLATS	16

void	T_PlatRaise(plat_t	*plat);
int		EV_DoPlat(line_t *line,plattype_e type,int amount);

/*
===============================================================================

							P_CEILNG

===============================================================================
*/
typedef enum
{
	crushAndRaise,
	raiseAndCrush,
	raiseCeiling,
} ceiling_e;

typedef struct
{
	thinker_t	thinker;
	sector_t	*sector;
	VINT		bottomheight, topheight;
	fixed_t     downspeed;
	fixed_t     upspeed;
	uint8_t		type;
	uint8_t		crush;
	int8_t		direction;		/* 1 = up, 0 = waiting, -1 = down */
} ceiling_t;

typedef struct
{
	thinker_t thinker;
	sector_t *fofSector;
	sector_t *watersec;
	sector_t *targetSector;
	fixed_t speed;
	fixed_t distance;
	fixed_t floorwasheight;
	fixed_t ceilingwasheight;
	boolean low;
} bouncecheese_t;

void EV_BounceSector(sector_t *fofsec, sector_t *targetSector, fixed_t momz, VINT heightsec);

typedef struct
{
	thinker_t thinker;
	VINT *lines;
	VINT numlines;
	VINT totalLines;
	sector_t *ctrlSector;
} scrolltex_t;

typedef struct {
	thinker_t thinker;
	VINT *sectors;
	VINT numsectors;
	VINT totalSectors;
	line_t *ctrlLine;
	boolean carry;
} scrollflat_t;

typedef struct {
	thinker_t thinker;
	VINT *sectorData;
	VINT numsectors;
	VINT timer;
} lightningspawn_t;

void P_SpawnLightningStrike(boolean close);

typedef enum
{
	TMM_DOUBLESIZE      = 1,
	TMM_SILENT          = 1<<1,
	TMM_ALLOWYAWCONTROL = 1<<2,
	TMM_SWING           = 1<<3,
	TMM_MACELINKS       = 1<<4,
	TMM_CENTERLINK      = 1<<5,
	TMM_CLIP            = 1<<6,
	TMM_ALWAYSTHINK     = 1<<7,
} textmapmaceflags_t;

void P_PreallocateMaces(int numMaces);
void P_AddMaceChain(mapthing_t *point, vector3_t *axis, vector3_t *rotation, VINT *args);

#define	CEILSPEED		FRACUNIT*THINKERS_TICS
#define MAXCEILINGS		16

extern	ceiling_t	**activeceilings/*[MAXCEILINGS]*/;

int		EV_DoCeiling (line_t *line, ceiling_e  type);
void	T_MoveCeiling (ceiling_t *ceiling);

/*
===============================================================================

							P_FLOOR

===============================================================================
*/
typedef enum
{
	lowerFloor,			/* lower floor to highest surrounding floor */
	lowerFloorToLowest,	/* lower floor to lowest surrounding floor */
	turboLower,			/* lower floor to highest surrounding floor VERY FAST */
	raiseFloor,			/* raise floor to lowest surrounding CEILING */
	raiseFloorToNearest,	/* raise floor to next highest surrounding floor */
	raiseToTexture,		/* raise floor to shortest height texture around it */
	lowerAndChange,		/* lower floor to lowest surrounding floor and change */
						/* floorpic */
	floorContinuous,
	bothContinuous,
	continuousMoverFloor,
	continuousMoverCeiling,
	eggCapsuleOuter,
	eggCapsuleInner,
	eggCapsuleOuterPop,
	eggCapsuleInnerPop,
	moveFloorByFrontSector,
	moveCeilingByFrontSector,
	thz2DropBlock,
} floor_e;

typedef struct
{
	thinker_t	thinker;
	sector_t	*sector;
	sector_t    *controlSector;
	fixed_t     origSpeed;
	fixed_t		speed;
	VINT		sourceline;
	VINT        lowestSector;
	VINT        highestSector;
	VINT        dontChangeSector;
	VINT        floorwasheight;
	VINT		floordestheight;
	VINT        ceilDiff;
	VINT        delay; // like 'origSpeed'... set only at start
	VINT        delayTimer; // the actual counter
	VINT		type;
	uint8_t		crush;
	int8_t		direction;
	uint8_t		texture;
	uint8_t     tag;
} floormove_t;

typedef enum
{
	build8,	// slowly build by 8
	turbo16	// quickly build by 16
} stair_e;

#define	FLOORSPEED	((FRACUNIT+(FRACUNIT>>1))*THINKERS_TICS)

typedef enum
{
	ok,
	pastdest
} result_e;

result_e	T_MovePlane(sector_t *sector,fixed_t speed,
			fixed_t dest,boolean changeSector,int floorOrCeiling,int direction);

int		EV_DoFloor(line_t *line,floor_e floortype);
int		EV_DoFloorTag(line_t *line,floor_e floortype, uint8_t tag);
void	T_MoveFloor(floormove_t *floor);

void P_LinedefExecute(uint8_t tag, player_t *player, sector_t *caller);
