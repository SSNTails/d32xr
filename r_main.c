/* r_main.c */

#include "doomdef.h"
#include "r_local.h"

/*===================================== */

/* */
/* subsectors */
/* */
subsector_t		**vissubsectors, **lastvissubsector;

/* */
/* walls */
/* */
viswall_t	*viswalls, *lastwallcmd;

/* */
/* planes */
/* */
visplane_t	visplanes[MAXVISPLANES], *lastvisplane;

/* */
/* sprites */
/* */
vissprite_t	*vissprites, *lastsprite_p, *vissprite_p;

/* */
/* openings / misc refresh memory */
/* */
unsigned short	*openings, *lastopening;

/* holds *vissubsectors[MAXVISSEC], spropening[SCREENWIDTH+1], spanstart[256] */
intptr_t 	*r_workbuf;

/*===================================== */

boolean		phase1completed;

pixel_t		*workingscreen;


fixed_t		viewx, viewy, viewz;
angle_t		viewangle;
fixed_t		viewcos, viewsin;
player_t	*viewplayer;

VINT			validcount = 1;		/* increment every time a check is made */
int			framecount;		/* incremented every frame */


boolean		fixedcolormap;

int			lightlevel;			/* fixed light level */
int			extralight;			/* bumped light from gun blasts */

/* */
/* sky mapping */
/* */
int			skytexture;


/* */
/* precalculated math */
/* */
angle_t		clipangle,doubleclipangle;
#ifndef MARS
fixed_t	*finecosine_ = &finesine_[FINEANGLES/4];
#endif

int t_ref_bsp, t_ref_prep, t_ref_segs, t_ref_planes, t_ref_sprites, t_ref_total;

/*
===============================================================================
=
= R_PointToAngle
=
===============================================================================
*/


extern	int	tantoangle[SLOPERANGE+1];

int SlopeDiv (unsigned num, unsigned den)
{
	unsigned ans;
	if (den < 512)
		return SLOPERANGE;
	ans = (num<<3)/(den>>8);
	return ans <= SLOPERANGE ? ans : SLOPERANGE;
}

angle_t R_PointToAngle2 (fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2)
{	
	int		x;
	int		y;
	
	x = x2 - x1;
	y = y2 - y1;
	
	if ( (!x) && (!y) )
		return 0;
	if (x>= 0)
	{	/* x >=0 */
		if (y>= 0)
		{	/* y>= 0 */
			if (x>y)
				return tantoangle[ SlopeDiv(y,x)];     /* octant 0 */
			else
				return ANG90-1-tantoangle[ SlopeDiv(x,y)];  /* octant 1 */
		}
		else
		{	/* y<0 */
			y = -y;
			if (x>y)
				return -tantoangle[SlopeDiv(y,x)];  /* octant 8 */
			else
				return ANG270+tantoangle[ SlopeDiv(x,y)];  /* octant 7 */
		}
	}
	else
	{	/* x<0 */
		x = -x;
		if (y>= 0)
		{	/* y>= 0 */
			if (x>y)
				return ANG180-1-tantoangle[ SlopeDiv(y,x)]; /* octant 3 */
			else
				return ANG90+ tantoangle[ SlopeDiv(x,y)];  /* octant 2 */
		}
		else
		{	/* y<0 */
			y = -y;
			if (x>y)
				return ANG180+tantoangle[ SlopeDiv(y,x)];  /* octant 4 */
			else
				return ANG270-1-tantoangle[ SlopeDiv(x,y)];  /* octant 5 */
		}
	}	
#ifndef LCC	
	return 0;
#endif
}


/*
===============================================================================
=
= R_PointOnSide
=
= Returns side 0 (front) or 1 (back)
===============================================================================
*/

int	R_PointOnSide (int x, int y, node_t *node)
{
	fixed_t	dx,dy;
	fixed_t	left, right;

	if (!node->dx)
	{
		if (x <= node->x)
			return node->dy > 0;
		return node->dy < 0;
	}
	if (!node->dy)
	{
		if (y <= node->y)
			return node->dx < 0;
		return node->dx > 0;
	}
	
	dx = (x - node->x);
	dy = (y - node->y);
	
	left = (node->dy>>16) * (dx>>16);
	right = (dy>>16) * (node->dx>>16);
	
	if (right < left)
		return 0;		/* front side */
	return 1;			/* back side */
}



/*
==============
=
= R_PointInSubsector
=
==============
*/

struct subsector_s *R_PointInSubsector (fixed_t x, fixed_t y)
{
	node_t	*node;
	int		side, nodenum;
	
	if (!numnodes)				/* single subsector is a special case */
		return subsectors;
		
	nodenum = numnodes-1;

	while (! (nodenum & NF_SUBSECTOR) )
	{
		node = &nodes[nodenum];
		side = R_PointOnSide (x, y, node);
		nodenum = node->children[side];
	}
	
	return &subsectors[nodenum & ~NF_SUBSECTOR];
	
}

/*============================================================================= */


/*
==============
=
= R_Init
=
==============
*/

void R_Init (void)
{
D_printf ("R_InitData\n");
	R_InitData ();
D_printf ("Done\n");

	clipangle = xtoviewangle[0];
	doubleclipangle = clipangle*2;
	
	framecount = 0;
	viewplayer = &players[0];
}

/*============================================================================= */

extern int ticsinframe;

extern int checkpostics, shoottics;
extern int lasttics;

extern	int	playertics, thinkertics, sighttics, basetics, latetics;
extern	int	tictics;

extern	int		soundtics;

/*
===================
=
= R_DebugScreen
=
===================
*/

void R_DebugScreen (void)
{
#ifdef JAGUAR

#if 1
	PrintNumber (15,1, vblsinframe);

	PrintNumber (15,2, phasetime[8] - phasetime[0]);
	PrintNumber (15,3, tictics);
	PrintNumber (15,4, soundtics);
#endif

#if 1
	PrintNumber (15,6, playertics);
	PrintNumber (15,7, thinkertics);
	PrintNumber (15,8, sighttics);
	PrintNumber (15,9, basetics);
	PrintNumber (15,10, latetics);
#endif

#if 0
	int	i;
	
	PrintNumber (15,1, phasetime[8] - phasetime[0]);
	for (i=0 ; i < 8 ; i++)
		PrintNumber (15,3+i,phasetime[i+1]-phasetime[i]);
#endif


#endif

}
/*============================================================================= */

int shadepixel;
#ifndef MARS
extern	int	workpage;
extern	pixel_t	*screens[2];	/* [SCREENWIDTH*SCREENHEIGHT];  */
#endif

/*
==================
=
= R_Setup
=
==================
*/

void R_Setup (void)
{
	int 		i;
	int		damagecount, bonuscount;
	player_t *player;
	int		shadex, shadey, shadei;
	unsigned short  *tempbuf;
	int		*wb, *wbp;
	
/* */
/* set up globals for new frame */
/* */
#ifndef MARS
	workingscreen = screens[workpage];

	*(pixel_t  **)0xf02224 = workingscreen;	/* a2 base pointer */
	*(int *)0xf02234 = 0x10000;				/* a2 outer loop add (+1 y) */
	*(int *)0xf0226c = *(int *)0xf02268 = 0;		/* pattern compare */
#endif

	framecount++;	
	validcount++;
		
	viewplayer = player = &players[displayplayer];
	viewx = player->mo->x;
	viewy = player->mo->y;
	viewz = player->viewz;
	viewangle = player->mo->angle;

	viewsin = finesine(viewangle>>ANGLETOFINESHIFT);
	viewcos = finecosine(viewangle>>ANGLETOFINESHIFT);
		
	extralight = player->extralight << 6;
	fixedcolormap = player->fixedcolormap;
		
/* */
/* calc shadepixel */
/* */
	player = &players[consoleplayer];

	damagecount = player->damagecount;
	bonuscount = player->bonuscount;
	
	if (damagecount)
		damagecount += 10;
	if (bonuscount)
		bonuscount += 2;
	damagecount >>= 2;
	shadex = (bonuscount>>1) + damagecount;
	shadey = (bonuscount>>1) - damagecount;
	shadei = (bonuscount + damagecount)<<2;

	shadei += player->extralight<<3;

/* */
/* pwerups */
/* */
	if (player->powers[pw_invulnerability] > 60
	|| (player->powers[pw_invulnerability]&4) )
	{
		shadex -= 8;
		shadei += 32;
	}

	if (player->powers[pw_ironfeet] > 60
	|| (player->powers[pw_ironfeet]&4) )
		shadey += 7;

	if (player->powers[pw_strength] 
	&& (player->powers[pw_strength]< 64) )
		shadex += (8 - (player->powers[pw_strength]>>3) );


/* */
/* bound and store shades */
/* */
	if (shadex > 7)
		shadex = 7;
	else if (shadex < -8)
		shadex = -8;
	if (shadey > 7)
		shadey = 7;
	else if (shadey < -8)
		shadey = -8;
	if (shadei > 127)
		shadei = 127;
	else if (shadei < -128)
		shadei = -128;
		
	shadepixel = ((shadex<<12)&0xf000) + ((shadey<<8)&0xf00) + (shadei&0xff);

	tempbuf = (unsigned short *)I_WorkBuffer();

/* */
/* plane filling */
/*	 */
	tempbuf = (unsigned short *)(((int)tempbuf+2)&~1);
	tempbuf++; // padding
	for (i = 0; i < MAXVISPLANES; i++) {
		visplanes[i].open = tempbuf;
		tempbuf += SCREENWIDTH+2;
	}

	lastvisplane = visplanes+1;		/* visplanes[0] is left empty */

	tempbuf = (unsigned short *)(((int)tempbuf+4)&~3);
	viswalls = (void *)tempbuf;
	tempbuf += sizeof(*viswalls)*MAXWALLCMDS/sizeof(*tempbuf);

	lastwallcmd = viswalls;			/* no walls added yet */

	vissubsectors = (subsector_t **)&r_workbuf[0];
	lastvissubsector = vissubsectors;	/* no subsectors visible yet */
	
/*	 */
/* clear sprites */
/* */
	tempbuf = (unsigned short *)(((int)tempbuf+4)&~3);
	vissprites = (void *)tempbuf;
	tempbuf += sizeof(*vissprites)*MAXVISSPRITES/sizeof(*tempbuf);
	vissprite_p = vissprites;

	tempbuf = (unsigned short*)(((int)tempbuf + 4) & ~3);
	wbp = (int*)tempbuf;
	openings = tempbuf;
	tempbuf += sizeof(*openings)*MAXOPENINGS/sizeof(*tempbuf);
	wb = (int*)tempbuf;

#ifdef MARS
	// clear the openings in workbuffer as they are later written as single bytes,
	// some of which may be 0 and since the work buffer resides in VRAM, those are
	// going to be ignored and that will be causing visual glitches
	while (wbp < wb) *wbp++ = 0;
#endif

	lastopening = openings;
#ifndef MARS
	phasetime[0] = samplecount;
#endif
}


void R_BSP (void);
void R_WallPrep (void);
void R_SpritePrep (void);
boolean R_LatePrep (void);
void R_Cache (void);
void R_SegCommands (void);
void R_DrawPlanes (void);
void R_Sprites (void);
void R_Update (void);


/*
==============
=
= R_RenderView
=
==============
*/

#ifndef MARS
int		phasetime[9] = {1,2,3,4,5,6,7,8,9};

extern	ref1_start;
extern	ref2_start;
extern	ref3_start;
extern	ref4_start;
extern	ref5_start;
extern	ref6_start;
extern	ref7_start;
extern	ref8_start;
#endif

extern	boolean	debugscreenactive;

void R_RenderPlayerView (void)
{

/* make sure its done now */
#ifdef JAGUAR
	while (!I_RefreshCompleted ())
	;
#endif

/* */
/* initial setup */
/* */
	if (debugscreenactive)
		R_DebugScreen ();

	t_ref_total = I_GetTime();

	R_Setup ();

#ifndef JAGUAR
	t_ref_bsp = I_GetTime();
	R_BSP ();
	t_ref_bsp = I_GetTime() - t_ref_bsp;

	t_ref_prep = I_GetTime();
	R_WallPrep ();
	R_SpritePrep ();
/* the rest of the refresh can be run in parallel with the next game tic */
	if (R_LatePrep ())
		R_Cache ();
	t_ref_prep = I_GetTime() - t_ref_prep;

	t_ref_segs = I_GetTime();
	R_SegCommands ();
	t_ref_segs = I_GetTime() - t_ref_segs;

	t_ref_planes = I_GetTime();
	R_DrawPlanes ();
	t_ref_planes = I_GetTime() - t_ref_planes;

	t_ref_sprites = I_GetTime();
	R_Sprites ();
	t_ref_sprites = I_GetTime() - t_ref_sprites;

	t_ref_total = I_GetTime() - t_ref_total;

#ifndef MARS
	I_Update();
#endif
#else

/* start the gpu running the refresh */
	phasetime[1] = 0;	
	phasetime[2] = 0;	
	phasetime[3] = 0;	
	phasetime[4] = 0;	
	phasetime[5] = 0;	
	phasetime[6] = 0;	
	phasetime[7] = 0;	
	phasetime[8] = 0;	
	gpufinished = zero;
	gpucodestart = (int)&ref1_start;

#if 0
	while (!I_RefreshCompleted () )
	;		/* wait for refresh to latch all needed data before */
			/* running the next tick */
#endif

#endif
}

