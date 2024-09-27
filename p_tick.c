#include "doomdef.h"
#include "p_local.h"
#ifdef MARS
#include "mars.h"
#endif

int	playertics, thinkertics, sighttics, basetics, latetics;
int	tictics, drawtics;

boolean		gamepaused;
jagobj_t	*pausepic;
char		clearscreen = 0;
char		remove_distortion = 0;

/*
===============================================================================

								THINKERS

All thinkers should be allocated by Z_Malloc so they can be operated on uniformly.  The actual
structures will vary in size, but the first element must be thinker_t.

Mobjs are similar to thinkers, but kept seperate for more optimal list
processing
===============================================================================
*/

thinker_t	thinkercap;	/* both the head and tail of the thinker list */
degenmobj_t		mobjhead;	/* head and tail of mobj list */
degenmobj_t		freemobjhead, freestaticmobjhead;	/* head and tail of free mobj list */
degenmobj_t		limbomobjhead;

scenerymobj_t *scenerymobjlist;
ringmobj_t *ringmobjlist;
VINT numscenerymobjs = 0;
VINT numringmobjs = 0;
VINT numstaticmobjs = 0;
VINT numregmobjs = 0;

//int			activethinkers;	/* debug count */
//int			activemobjs;	/* debug count */

/*
===============
=
= P_InitThinkers
=
===============
*/

void P_InitThinkers (void)
{
	thinkercap.prev = thinkercap.next  = &thinkercap;
	mobjhead.next = mobjhead.prev = (void *)&mobjhead;
	freemobjhead.next = freemobjhead.prev = (void *)&freemobjhead;
	freestaticmobjhead.next = freestaticmobjhead.prev = (void *)&freestaticmobjhead;
	limbomobjhead.next = limbomobjhead.prev = (void*)&limbomobjhead;
	scenerymobjlist = NULL;
	ringmobjlist = NULL;
}


/*
===============
=
= P_AddThinker
=
= Adds a new thinker at the end of the list
=
===============
*/

void P_AddThinker (thinker_t *thinker)
{
	thinkercap.prev->next = thinker;
	thinker->next = &thinkercap;
	thinker->prev = thinkercap.prev;
	thinkercap.prev = thinker;	
}


/*
===============
=
= P_RemoveThinker
=
= Deallocation is lazy -- it will not actually be freed until its
= thinking turn comes up
=
===============
*/

void P_RemoveThinker (thinker_t *thinker)
{
	thinker->function = (think_t)-1;
}

/*
===============
=
= P_RunThinkers
=
===============
*/

void P_RunThinkers (void)
{
	thinker_t	*currentthinker;
	
	//activethinkers = 0;
	
	currentthinker = thinkercap.next;
	while (currentthinker != &thinkercap)
		{
		if (currentthinker->function == (think_t)-1)
		{	/* time to remove it */
			currentthinker->next->prev = currentthinker->prev;
			currentthinker->prev->next = currentthinker->next;
			Z_Free (currentthinker);
		}
		else
		{
			if (currentthinker->function)
			{
				currentthinker->function (currentthinker);
			}
			//activethinkers++;
		}
		currentthinker = currentthinker->next;
	}
}


/*============================================================================= */

/*
===============
=
= P_CheckSights
=
= Check sights of all mobj thinkers that are going to change state this
= tic and have MF_COUNTKILL set
===============
*/

#ifdef MARS
void P_CheckSights2(int c) ATTR_DATA_CACHE_ALIGN ATTR_OPTIMIZE_SIZE;
#else
void P_CheckSights2(void) ATTR_DATA_CACHE_ALIGN ATTR_OPTIMIZE_SIZE;
#endif

#ifdef MARS
void Mars_Sec_P_CheckSights(void)
{
	P_CheckSights2(1);
}
#endif

void P_CheckSights (void)
{
#ifdef JAGUAR
	extern	int p_sight_start;
	DSPFunction (&p_sight_start);
#elif defined(MARS)
	Mars_P_BeginCheckSights();
	P_CheckSights2(0);
	Mars_P_EndCheckSights();
#else
	P_CheckSights2();
#endif
}

/*============================================================================= */

/* 
=================== 
= 
= P_RunMobjBase  
=
= Run stuff that doesn't happen every tick
=================== 
*/ 

void	P_RunMobjBase (void)
{
#ifdef JAGUAR
	extern	int p_base_start;
	 
	DSPFunction (&p_base_start);
#else
	P_RunMobjBase2 ();
#endif
}

/*============================================================================= */

/*
==============
=
= P_CheckCheats
=
==============
*/
 
void P_CheckCheats (void)
{
#ifdef JAGUAR
	int		buttons, oldbuttons;
	int 	warpmap;
	int		i;
	player_t	*p;

	for (i=0 ; i<MAXPLAYERS ; i++)
	{
		if (!playeringame[i])
			continue;
		buttons = ticbuttons[i];
		oldbuttons = oldticbuttons[i];
	
		if ( (buttons & BT_PAUSE) && !(oldbuttons&BT_PAUSE) )
			gamepaused ^= 1;
	}

	if (netgame)
		return;

	buttons = ticbuttons[0];
	oldbuttons = oldticbuttons[0];

	if ( (oldbuttons&BT_PAUSE) || !(buttons & BT_PAUSE ) )
		return;

	if (buttons&JP_NUM)
	{	/* free stuff */
		p=&players[0];
		for (i=0 ; i<NUMCARDS ; i++)
			p->cards[i] = true;			
		p->armorpoints = 200;
		p->armortype = 2;
		for (i=0;i<NUMWEAPONS;i++) p->weaponowned[i] = true;
		for (i=0;i<NUMAMMO;i++) p->ammo[i] = p->maxammo[i] = 500;
	}

	if (buttons&JP_STAR)
	{	/* godmode */
		players[0].cheats ^= CF_GODMODE;
	}
	warpmap = 0;
	if (buttons&JP_1) warpmap = 1;
	if (buttons&JP_2) warpmap = 2;
	if (buttons&JP_3) warpmap = 3;
	if (buttons&JP_4) warpmap = 4;
	if (buttons&JP_5) warpmap = 5;
	if (buttons&JP_6) warpmap = 6;
	if (buttons&JP_7) warpmap = 7;
	if (buttons&JP_8) warpmap = 8;
	if (buttons&JP_9) warpmap = 9;
	if (buttons&JP_A) warpmap += 10;
	else if (buttons&JP_B) warpmap += 20;
	
	if (warpmap>0 && warpmap < 27)
	{
		gamemaplump = G_LumpNumForMapNum(warpmap);
		gameaction = ga_warped;
	}
#elif defined(MARS)
	int buttons;
	int oldbuttons;
	const int stuff_combo = BT_A | BT_B | BT_C | BT_UP;
	const int godmode_combo = BT_X | BT_Y | BT_Z | BT_UP;
//	player_t* p;

	if (netgame)
		return;
	if (!gamepaused)
		return;

	buttons = ticrealbuttons;
	oldbuttons = oldticrealbuttons;

	if ((buttons & stuff_combo) == stuff_combo 
		&& (oldbuttons & stuff_combo) != stuff_combo)
	{
		/* free stuff */
//		p = &players[0];
	}

	if ((buttons & godmode_combo) == godmode_combo 
		&& (oldbuttons & godmode_combo) != godmode_combo)
	{
		/* godmode */
		players[0].cheats ^= CF_GODMODE;
	}
#endif
}

int playernum;

void G_DoReborn (int playernum); 
void G_DoLoadLevel (void);

/*
=================
=
= P_Ticker
=
=================
*/

int		ticphase;

#ifdef MARS
#define frtc I_GetFRTCounter()
#else
#define frtc samplecount
#endif

int P_Ticker (void)
{
	int		ticstart;
	player_t	*pl;

	if (demoplayback)
	{
		if (M_Ticker())
			return ga_exitdemo;
	}


	while (!I_RefreshLatched () )
	;		/* wait for refresh to latch all needed data before */
			/* running the next tick */

#ifdef JAGAUR
	while (DSPRead (&dspfinished) != 0xdef6)
	;		/* wait for sound mixing to complete */
#endif

#ifdef MARS
    // bank-switch to the page with map data
    W_GetLumpData(gamemaplump);
#endif

	gameaction = ga_nothing;

/* */
/* check for pause and cheats */
/* */
	P_CheckCheats ();
	
/* */
/* do option screen processing */
/* */

	for (playernum = 0, pl = players; playernum < MAXPLAYERS; playernum++, pl++)
		if (playeringame[playernum])
			O_Control(pl);

	if (gameaction != ga_nothing)
		return gameaction;

	/* */
	/* run player actions */
	/* */
	if (gamepaused)
		return 0;

	playertics = 0;
	thinkertics = 0;
	ticstart = frtc;
	for (int skipCount = 0; skipCount < accum_time; skipCount++)
	{
		for (playernum = 0, pl = players; playernum < MAXPLAYERS; playernum++, pl++)
			if (playeringame[playernum])
			{
				if (pl->playerstate == PST_REBORN)
					G_DoReborn(playernum);

				P_PlayerThink(pl);
			}

		P_RunThinkers();

		{
	//		if (gametic != prevgametic)
			{
				// If we don't do this every tic, it seems sight checking is broken.
				// Is there a way we can do this infrequently? Even every half second would be fine.
				P_CheckSights();
//				sighttics = frtc - start;
			}

//			start = frtc;
			P_RunMobjBase();
//			basetics = frtc - start;

//			start = frtc;
			P_RunMobjLate();
//			latetics = frtc - start;

			P_UpdateSpecials();

			leveltime++;
		}

		if (skipCount == 0)
			tictics = frtc - ticstart;
	}

	ST_Ticker();			/* update status bar */

	return gameaction;		/* may have been set to ga_died, ga_completed, */
							/* or ga_secretexit */
}


/* 
============= 
= 
= DrawPlaque 
= 
============= 
*/ 
 
void DrawPlaque (jagobj_t *pl)
{
#ifndef MARS
	int			x,y,w;
	short		*sdest;
	byte		*bdest, *source;
	extern		int isrvmode;
	
	while ( !I_RefreshCompleted () )
	;

	bufferpage = (byte *)screens[!workpage];
	source = pl->data;
	
	if (isrvmode == 0xc1 + (3<<9) )
	{	/* 320 mode, stretch pixels */
		bdest = (byte *)bufferpage + 80*320 + (160 - pl->width);
		w = pl->width;
		for (y=0 ; y<pl->height ; y++)
		{
			for (x=0 ; x<w ; x++)
			{
				bdest[x*2] = bdest[x*2+1] = *source++;
			}
			bdest += 320;
		}
	}
	else
	{	/* 160 mode, draw directly */
		sdest = (short *)bufferpage + 80*160 + (80 - pl->width/2);
		w = pl->width;
		for (y=0 ; y<pl->height ; y++)
		{
			for (x=0 ; x<w ; x++)
			{
				sdest[x] = palette8[*source++];
			}
			sdest += 160;
		}
	}
#else
	clearscreen = 2;
#endif
}

/* 
============= 
= 
= DrawPlaque 
= 
============= 
*/ 
#ifndef MARS
void DrawSinglePlaque (jagobj_t *pl)
{
	int			x,y,w;
	byte		*bdest, *source;
	
	while ( !I_RefreshCompleted () )
	;

	bufferpage = (byte *)screens[!workpage];
	source = pl->data;
	
	bdest = (byte *)bufferpage + 80*320 + (160 - pl->width/2);
	w = pl->width;
	for (y=0 ; y<pl->height ; y++)
	{
		for (x=0 ; x<w ; x++)
			bdest[x] = *source++;
		bdest += 320;
	}
}
#endif


/* 
============= 
= 
= P_Drawer 
= 
= draw current display 
============= 
*/ 
 
 
void P_Drawer (void) 
{ 	
	boolean optionsactive = (players[consoleplayer].automapflags & AF_OPTIONSACTIVE) != 0;
	static boolean o_wasactive = false;

#ifdef MARS
	extern	boolean	debugscreenactive;

	drawtics = frtc;

	if (!optionsactive && o_wasactive)
		clearscreen = 2;

	if (clearscreen > 0) {
		I_ResetLineTable();

		if ((viewportWidth == 160 && lowResMode) || viewportWidth == 320)
			DrawTiledLetterbox();
		else
			DrawTiledBackground();

		I_DrawSbar();
		
		if (clearscreen == 2 || optionsactive)
			ST_ForceDraw();
		clearscreen--;
	}

	if (remove_distortion) {
		// The other frame buffer has already been normalized.
		// Now normalize the current frame buffer.
		RemoveDistortionFilters();
		remove_distortion = 0;
	}

	if (initmathtables)
	{
		R_InitMathTables();
		initmathtables--;
	}

	/* view the guy you are playing */
	R_RenderPlayerView(consoleplayer);
	/* view the other guy in split screen mode */
	if (splitscreen) {
		Mars_R_SecWait();
		R_RenderPlayerView(consoleplayer ^ 1);
	}

	ST_Drawer();

	Mars_R_SecWait();

	if (demoplayback)
		M_Drawer();
	if (optionsactive)
		O_Drawer();

	o_wasactive = optionsactive;

	drawtics = frtc - drawtics;

	if (debugscreenactive)
		I_DebugScreen();
#else
	if (optionsactive)
	{
		O_Drawer ();
	}
	else if (gamepaused && refreshdrawn)
		DrawPlaque (pausepic);
	else
	{
#ifdef JAGUAR
		ST_Drawer();
#endif
		R_RenderPlayerView(consoleplayer);
		clearscreen = 0;
	}
	/* assume part of the refresh is now running parallel with main code */
#endif
}

void P_Start (void)
{
	extern boolean canwipe;

	/* load a level */
	G_DoLoadLevel();

#ifndef MARS
	S_RestartSounds ();
#endif
	players[0].automapflags = 0;
	players[1].automapflags = 0;
	M_ClearRandom ();

	if (!demoplayback && !demorecording)
		if (!netgame || splitscreen)
			P_RandomSeed(I_GetTime());

	clearscreen = 2;
	canwipe = true;
}

void P_Stop (void)
{
	M_Stop();
	Z_FreeTags (mainzone);
}

void P_Update (void)
{
	int ticratebak;

	ticratebak = ticsperframe;

	if (viewportWidth >= 320 && ticsperframe < 3)
		ticsperframe = 3;

	I_Update();

	ticsperframe = ticratebak;
}
