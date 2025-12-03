#include "doomdef.h"
#include "p_local.h"
#include "sounds.h"
#ifdef MARS
#include "mars.h"
#include "v_font.h"
#endif

//int free_memory;

int	playertics, thinkertics, sighttics, basetics, latetics;
int	tictics, drawtics;

uint8_t		lightning_count;

boolean		gamepaused;
jagobj_t	*pausepic;
char		clearscreen = 0;
char		clear_h32_borders = 0;
VINT        distortion_action = DISTORTION_NONE;

#if defined(PLAY_POS_DEMO) || defined(REC_POS_DEMO)
	fixed_t prev_rec_values[4];
#endif

unsigned char rec_prev_buttons = 0;
unsigned char rec_buttons = 0;
unsigned char rec_button_count = 0;
unsigned short rec_start_time = 0;

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

sectorBBox_t sectorBBoxes;

scenerymobj_t *scenerymobjlist;
ringmobj_t *ringmobjlist;
VINT numscenerymobjs = 0;
VINT numringmobjs = 0;
VINT numstaticmobjs = 0;
VINT numregmobjs = 0;

VINT scenerymobjcount = 0;
VINT ringmobjcount = 0;

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
	sectorBBoxes.next = sectorBBoxes.prev = (void *)&sectorBBoxes;
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

	P_RunMobjBase2();
#endif
}

/*============================================================================= */

int playernum;

void G_DoReborn (int playernum); 
void G_DoLoadLevel (void);

gameaction_t RecordDemo();
gameaction_t PlayDemo();

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

void P_Weather()
{
	if (gamemapinfo.weather == 1) {
		unsigned short lightning_chance = P_Random16();

		if (lightning_chance < 160) {
			// Close lightning
			lightning_count = 0x18;
		}
		else if (lightning_chance < 560) {
			// Distant lightning
			lightning_count = 0x08;
		}

		int proximity = lightning_count >> 4;
		int count = lightning_count & 0xF;
		if (count > 0) {
			const uint8_t brightness_levels[2][4] = {
				{ 0x09, 0x13, 0x09, 0x00 },
				{ 0x0F, 0x1F, 0x0F, 0x00 }
			};

			if (proximity == 1)
			{
				// Close strike
				if (count == 8) {
					// Disable shadow for a short time to lighten the sky.
					R_SetShadowHighlight(false);
					P_SpawnLightningStrike(true);
				}
				else if (count == 5) {
					// Re-enable shadow to return the sky back to normal (i.e. dark).
					R_SetShadowHighlight(true);
					S_StartSoundId(sfx_litng1);
				}
			}
			else //if (proximity == 0)
			{
				if (count == 8)
					P_SpawnLightningStrike(false);

				// Distant strike
				if (count == 1) {
					// Enable shadow in case it wasn't already enabled previously.
					R_SetShadowHighlight(true);
					S_StartSoundId(sfx_litng2);
				}
			}

			if (count > 4) {
				copper_table_brightness = brightness_levels[proximity][8-count];
				effects_flags |= EFFECTS_COPPER_BRIGHTNESS_CHANGE;
			}
			else {
				effects_flags &= (~EFFECTS_COPPER_BRIGHTNESS_CHANGE);
			}

			lightning_count--;
		}
	}
}

int P_Ticker (void)
{
	int		ticstart;
	player_t	*pl;

	if (!IsDemoModeType(DemoMode_Playback)) {
		players[0].buttons = Mars_ConvGamepadButtons(ticbuttons[consoleplayer]);
	}

	if (IsTitleScreen())
	{
		int result = M_Ticker(); // Run menu logic.
		if (result)
			return result;
	}

	// This is needed so the fade isn't removed until the new world is drawn at least once
	if (gametic >= 2 && gametic < 10) {
		fadetime = 0;
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
    W_POINTLUMPNUM(gamemaplump);
#endif

	if (leveltime < 30 && IsLevelType(LevelType_Normal) && !IsDemo()) {
		leveltime += accum_time;
		return ga_nothing;
	}

	gameaction = ga_nothing;

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
		return ga_nothing;

	playertics = 0;
	thinkertics = 0;
	ticstart = frtc;

	// Not needed for every tic. At least not unless we're needing synchronicity...
	if (!(IsDemo()))
	{
#ifdef MARS
   		Mars_P_BeginAnimationUpdate();
#endif
		P_CheckSights();
	}
	else
	{
		P_AnimateScenery((int8_t)accum_time);
		P_UpdateSpecials((int8_t)accum_time);
	}

	P_Weather();

	Mars_P_EndAnimationUpdate();

	for (int skipCount = 0; skipCount < accum_time; skipCount++)
	{
#ifndef SHOW_DISCLAIMER
		debugCounter = 0;
#endif
		if (IsDemoModeType(DemoMode_Playback)) {
			players[0].buttons = Mars_ConvGamepadButtons(rec_buttons);
		}

		if (gameaction == ga_nothing) {
			if (IsDemoModeType(DemoMode_Recording)) {
				gameaction = RecordDemo();
			}
			else if (IsDemoModeType(DemoMode_Playback)) {
				gameaction = PlayDemo();
			}
		}

		if (IsDemo())
			P_CheckSights();

		for (playernum = 0, pl = players; playernum < MAXPLAYERS; playernum++, pl++)
		{
			if (playeringame[playernum])
			{
				if (pl->playerstate == PST_REBORN)
					G_DoReborn(playernum);

				P_PlayerThink(pl);
			}
		}

		P_RunThinkers();

//		start = frtc;
		P_RunMobjBase();
//		basetics = frtc - start;

#ifndef SHOW_DISCLAIMER
//		CONS_Printf("DebugCounter: %d", debugCounter);
#endif

#ifdef KIOSK_MODE
		if ((players[0].pflags & PF_CONTROLDISABLED)) {
			kiosk_timeout_count = 0;
		}
		else if (players[0].buttons == 0) {
			kiosk_timeout_count++;
			if (kiosk_timeout_count >= KIOSK_TIMEOUT) {
				kiosk_timeout_count = 0;
				gameaction = ga_backtotitle;
//				exit = ga_backtotitle;
				break;
			}
		}
		else {
			kiosk_timeout_count = 0;
		}
#endif

		if (overlay_graphics != og_about) {
			leveltime++;
		}

		if (skipCount == 0)
			tictics = frtc - ticstart;
	}

//	start = frtc;
	P_RunMobjLate(); // I think we can get away with freeing dead mobjs once per vblank
//	latetics = frtc - start;

	ST_Ticker();			/* update status bar */

	return gameaction;		/* may have been set to ga_died, ga_completed, */
							/* or ga_secretexit */
}

gameaction_t RecordDemo()
{
#ifdef REC_POS_DEMO
	if (((short *)demobuffer)[3] == -1) {
		// This is the first frame, so record the initial values in full.
		prev_rec_values[0] = players[0].mo->x;
		prev_rec_values[1] = players[0].mo->y;
		prev_rec_values[2] = players[0].mo->z;
		prev_rec_values[3] = players[0].mo->angle >> ANGLETOFINESHIFT;

		char *values_p = prev_rec_values;
		for (int i=0; i < 4; i++) {
			*demo_p++ = *values_p++;
			*demo_p++ = *values_p++;
			*demo_p++ = *values_p++;
			*demo_p++ = *values_p++;
		}

		((short *)demobuffer)[2] += 16; // 16 bytes written.
	}
	else {
		// Beyond the first frame, we record only the values that
		// have changed.
		unsigned char frame_bytes = 1; // At least one byte will be written.

		// Calculate the difference between values in the current
		// frame and previous frame.
		fixed_t delta[4];
		delta[0] = players[0].mo->x - prev_rec_values[0];
		delta[1] = players[0].mo->y - prev_rec_values[1];
		delta[2] = players[0].mo->z - prev_rec_values[2];
		delta[3] = (players[0].mo->angle >> ANGLETOFINESHIFT) - prev_rec_values[3];

		// Save the current frame's values in 'prev_rec_values' so
		// the next frame's comparisons can be done against the
		// current frame.
		prev_rec_values[0] = players[0].mo->x;
		prev_rec_values[1] = players[0].mo->y;
		prev_rec_values[2] = players[0].mo->z;
		prev_rec_values[3] = players[0].mo->angle >> ANGLETOFINESHIFT;

		unsigned char key = 0;

		// Record the values that have changed and the minimum number
		// of bytes needed to represent the deltas.
		for (int i=0; i < 4; i++) {
			key >>= 2;

			fixed_t d = delta[i];
			if (d != 0) {
				if (d <= 0x7F && d >= -0x80) {
					key |= 0x40; // Byte
					frame_bytes++;
				}
				else if (d <= 0x7FFF && d >= -0x8000) {
					key |= 0x80; // Short
					frame_bytes += 2;
				}
				else {
					key |= 0xC0; // Long
					frame_bytes += 4;
				}
			}
		}

		unsigned char *delta_bytes;
		unsigned char *prev_rec_values_bytes;

		*demo_p++ = key;

		// Based on the sizes put into the key, we record either the
		// deltas (in the case of char and short) or the full value.
		// Values with no difference will not be recorded.
		for (int i=0; i < 4; i++) {
			delta_bytes = &delta[i];
			prev_rec_values_bytes = &prev_rec_values[i];
			switch (key & 3) {
				case 3: // Long
					*demo_p++ = *prev_rec_values_bytes++;
					*demo_p++ = *prev_rec_values_bytes++;
					*demo_p++ = *prev_rec_values_bytes++;
					*demo_p++ = *prev_rec_values_bytes++;
					break;

				case 2: // Short
					*demo_p++ = delta_bytes[2];
					// fall-thru

				case 1: // Byte
					*demo_p++ = delta_bytes[3];
			}

			// Advance the key so the next two bits can be read to
			// check for updated values.
			key >>= 2;
		}

		((short *)demobuffer)[2] += frame_bytes;	// Increase data length.
	}

	((short *)demobuffer)[3] += 1;	// Increase frame count.
#endif

#ifdef REC_INPUT_DEMO
	if ((short)demo_p - (short)demobuffer >= (0x200 - 1)) {
		// The demo recording buffer has been filled up. End the recording.
		if (demobuffer[0x1FE] == 0xFF) {
			// This byte is a count of 255. Set it to zero so the demo can be ended.
			demobuffer[0x1FE] = 0;
		}
		demobuffer[0x1FF] = 0x80;
		*((short *)&demobuffer[6]) = leveltime - rec_start_time;
		SetDemoMode(DemoMode_None);
	}
	else if (leveltime - rec_start_time >= REC_DEMO_TIMEOUT || players[0].buttons & BT_START) {
		// The demo timeout period has been reached, or START was pressed. End the recording.
		demo_p += 1;
		*demo_p++ = 0x80;
		*((short *)&demobuffer[6]) = leveltime - rec_start_time;
		SetDemoMode(DemoMode_None);
	}
	else if (rec_start_time == 0x7FFF) {
		if (!(players[0].pflags & PF_CONTROLDISABLED)) {
			// Start recording!
			rec_start_time = leveltime;
			*demo_p++ = players[0].buttons;
			*demo_p = 1;
		}
	}
	else if (rec_prev_buttons == (players[0].buttons & 0xFF)) {
		// Same button combination as last frame. Increase the count.
		*demo_p += 1;
		if (*demo_p == 255) {
			// Count reached 255. Create an additional count and start it at zero.
			demo_p++;
			*demo_p = 0;
		}
	}
	else {
		// New button combination. Record the buttons with a count of 1.
		demo_p += 1;
		*demo_p++ = players[0].buttons;
		*demo_p = 1;
	}

	rec_prev_buttons = players[0].buttons;
#endif

	return ga_nothing;
}


gameaction_t PlayDemo()
{
	gameaction_t exit = ga_nothing;
#ifdef PLAY_POS_DEMO
	if (demo_p == demobuffer + 0xA) {
		// This is the first frame, so grab the initial values.
		prev_rec_values[0] = players[0].mo->x;
		prev_rec_values[1] = players[0].mo->y;
		prev_rec_values[2] = players[0].mo->z;
		prev_rec_values[3] = players[0].mo->angle >> ANGLETOFINESHIFT;

		demo_p += 16;
	}
	else {
		// Beyond the first frame, we update only the values that
		// have changed.
		unsigned char key = *demo_p++;

		int rec_value;
		unsigned char *prev_rec_values_bytes;

		for (int i=0; i < 4; i++) {
			// Check to see which values have changed and save them
			// in 'prev_rec_values' so the next frame's comparisons
			// can be done against the current frame.
			prev_rec_values_bytes = &prev_rec_values[i];
			rec_value = 0;

			switch (key&3) {
				case 3: // Long -- update the value as recorded (i.e. no delta).
					rec_value = *demo_p++;
					rec_value <<= 8;
					rec_value |= *demo_p++;
					rec_value <<= 8;
					rec_value |= *demo_p++;
					rec_value <<= 8;
					rec_value |= *demo_p++;
					prev_rec_values[i] = rec_value;
					break;

				case 2: // Short -- add the difference to the current value.
					rec_value = *demo_p++;
					rec_value <<= 8;
					rec_value |= *demo_p++;
					prev_rec_values[i] += (signed short)rec_value;
					break;

				case 1: // Byte -- add the difference to the current value.
					rec_value = *demo_p++;
					prev_rec_values[i] += (signed char)rec_value;
			}

			// Advance the key so the next two bits can be read to
			// check for updates.
			key >>= 2;
		}

		// Update the player variables with the newly updated
		// frame values.
		players[0].mo->x = prev_rec_values[0];
		players[0].mo->y = prev_rec_values[1];
		players[0].mo->z = prev_rec_values[2];
		players[0].mo->angle = prev_rec_values[3] << ANGLETOFINESHIFT;
	}
#endif

#ifndef PLAY_POS_DEMO	// Input demo code should *always* be present if position demo code is left out.
	int rec_current_time = leveltime - rec_start_time;
	int rec_end_time = ((short *)demobuffer)[3];

	if (rec_current_time >= rec_end_time) {
		ticbuttons[consoleplayer] = players[0].buttons = 0;
		SetDemoMode(DemoMode_None);
		exit = ga_completed;
	}
	else if (rec_start_time == 0x7FFF) {
		if (!(players[0].pflags & PF_CONTROLDISABLED)) {
			// Start demo playback!
			rec_start_time = leveltime;
			rec_buttons = *demo_p;
			ticbuttons[consoleplayer] = players[0].buttons = Mars_ConvGamepadButtons(rec_buttons);
			demo_p++;
			rec_button_count = *demo_p;
		}
	}
	else {
		rec_button_count--;
		if (rec_button_count == 0) {
			// Count is zero. Check if the next byte is a button mask or an additional count.
			if (*demo_p == 0xFF) {
				demo_p++;
				if (*demo_p > 0) {
					// Read next byte as an additional count.
					rec_button_count = *demo_p;
				}
			}
			if (rec_button_count == 0) {
				// Count is still zero. Read the next byte as a button mask.
				demo_p++;
				rec_buttons = *demo_p;
				if (rec_buttons & BT_START) {
					ticbuttons[consoleplayer] = players[0].buttons = 0;
					SetDemoMode(DemoMode_None);
					exit = ga_completed;
				}
				ticbuttons[consoleplayer] = players[0].buttons = Mars_ConvGamepadButtons(rec_buttons);
				demo_p++;
				rec_button_count = *demo_p;
			}
		}
		else {
			// Count is not zero. Reuse the previous button mask.
			ticbuttons[consoleplayer] = players[0].buttons = Mars_ConvGamepadButtons(rec_buttons);
		}
	}
	
	if (rec_end_time - rec_current_time <= 30) {
		int palette = PALETTE_SHIFT_CLASSIC_FADE_TO_BLACK + (((30 - (rec_end_time - rec_current_time)) * 2) / 3);
		R_FadePalette(dc_playpals, palette, dc_cshift_playpals);
		if (effects_flags &= EFFECTS_COPPER_ENABLED) {
			copper_table_brightness = -31 + (rec_end_time - rec_current_time);
			effects_flags |= EFFECTS_COPPER_REFRESH;
		}
		if (exit == ga_nothing) {
			exit = ga_demoending;
		}
	}
#endif

	return exit;
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
 
void ClearViewportOverdraw(void); // marsdraw.c

void P_Drawer (void) 
{ 	
	static boolean o_wasactive = false;

#ifdef MARS
	extern	boolean	debugscreenactive;

	drawtics = frtc;

	if (!optionsMenuOn && o_wasactive)
		clearscreen = 2;

	if (distortion_action == DISTORTION_NORMALIZE_H40) {
		// The other frame buffer has already been normalized.
		// Now normalize the current frame buffer.
		RemoveDistortionFilters();
		distortion_action = DISTORTION_NONE;
	}
	else if (distortion_action == DISTORTION_NORMALIZE_H32) {
		// The other frame buffer has already been normalized.
		// Now normalize the current frame buffer.
		RemoveDistortionFilters();
		distortion_action = DISTORTION_NONE;
	}
	else if (distortion_action == DISTORTION_REMOVE) {
		// The other frame buffer has already been normalized.
		// Now normalize the current frame buffer.
		RemoveDistortionFilters();
		distortion_action = DISTORTION_NONE;
	}
	else if (distortion_action == DISTORTION_ADD) {
		ApplyHorizontalDistortionFilter(gametic << 1);
		distortion_action = DISTORTION_NONE;
	}

	if (clearscreen > 0) {
		RemoveDistortionFilters();

		//if ((viewportWidth == (VIEWPORT_WIDTH>>1) && lowResMode) || viewportWidth == 320)
		//	DrawTiledLetterbox();
		//else
		//	DrawTiledBackground();

		DrawTiledLetterbox();

		if (viewportNum == VIEWPORT_H32 && clear_h32_borders == 0) {
			ClearViewportOverdraw();
		}
		
		if (clearscreen == 2 || optionsMenuOn)
			ST_ForceDraw();
		clearscreen--;
	}

	if (clear_h32_borders > 0) {
		ClearViewportOverdraw();
		clear_h32_borders--;
	}

	if (initmathtables)
	{
		R_InitMathTables();
		initmathtables--;
	}

	if (IsLevel() || (IsTitleScreen() && overlay_graphics != og_about)) {
		/* view the guy you are playing */
		R_RenderPlayerView(consoleplayer);
		/* view the other guy in split screen mode */
		if (splitscreen) {
			Mars_R_SecWait();
			R_RenderPlayerView(consoleplayer ^ 1);
		}
	}

	// Gotta wait for the other CPU to finish drawing before we start drawing the HUD overtop.
	Mars_R_SecWait();

	ST_Drawer();

	if (IsTitleScreen())
		M_Drawer();	// Show title emblem and menus.
	if (optionsMenuOn)
		O_Drawer();	// Show options menu.

	o_wasactive = optionsMenuOn;

	drawtics = frtc - drawtics;

	if (debugscreenactive) {
		I_DebugScreen();
	}
#ifdef SHOW_FPS
	I_DebugScreen();
#endif
#else
	if (optionsMenuOn)
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
	DoubleBufferSetup();

	/* load a level */
	G_DoLoadLevel();

	if (IsLevel()) {
		if (startmap >= SSTAGE_START && startmap <= SSTAGE_END) {
			SetLevel(LevelType_SpecialStage);
		}
		else {
			SetLevel(LevelType_Normal);
		}
	}

#ifndef MARS
	S_RestartSounds ();
#endif
	optionsMenuOn = false;
	M_ClearRandom ();

	if (IsDemo()) {
		rec_prev_buttons = 0;
		rec_buttons = 0;
		rec_button_count = 0;
		rec_start_time = 0x7FFF;

		players[0].pflags |= PF_CONTROLDISABLED;
		players[0].buttons = 0;
	}
	else if (!IsTitleScreen()) {
		if (!netgame || splitscreen) {
			P_RandomSeed(I_GetTime());
		}
	}

	if (IsLevelType(LevelType_SpecialStage)) {
		hudNumberFont.charCacheLength = 10;
		hudNumberFont.charCache = Z_Calloc(sizeof(void *) * 10, PU_LEVEL);
		char sttnum_name[8] = "STTNUM0";
		for (int i=0; i < 10; i++) {
			hudNumberFont.charCache[i] = W_CacheLumpName(sttnum_name, PU_LEVEL);
			sttnum_name[6]++;
		}

		jagobj_t **jagobjs_list[7] = {
			&narrow9_jagobj,
			&nbrackt_jagobj,
			&nbrackl_jagobj,
			&nbrackr_jagobj,
			&nbrackb_jagobj,
			&nrng1_jagobj,
			&nsshud_jagobj,
			//&chaos_jagobj
		};

		char *names_list[7] = {
			"NARROW9",
			"NBRACKT",
			"NBRACKL",
			"NBRACKR",
			"NBRACKB",
			"NRNG1",
			"NSSHUD",
			//"CHAOS1"
		};

		//names_list[7][5] += (gamemapinfo.mapNumber - SSTAGE_START);

		//free_memory = Z_FreeMemory(mainzone);
		int free_memory = Z_FreeMemory(mainzone);

		for (int i=0; i < 7; i++) {
			int lumpnum = W_GetNumForName(names_list[i]);
			int length = Z_CalculateAllocSize(W_LumpLength(lumpnum));
			if (length < free_memory) {
				*jagobjs_list[i] = (jagobj_t *)W_CacheLumpName(names_list[i], PU_LEVEL);
				free_memory = Z_FreeMemory(mainzone);
			}
		}

		char chaos_name[7] = { 'C', 'H', 'A', 'O', 'S', '1', '\0' };
		chaos_name[5] += gamemapinfo.mapNumber - SSTAGE_START;
		int lumpnum = W_GetNumForName(chaos_name);
		int length = Z_CalculateAllocSize(W_LumpLength(lumpnum));
		if (length < free_memory) {
			chaos_jagobj = W_CacheLumpName(chaos_name, PU_LEVEL);
		}

		//free_memory = Z_FreeMemory(mainzone);
	}

	if (IsTitleScreen()) {
		overlay_graphics = og_title;
	}
	else {
		overlay_graphics = og_none;
	}

	//clearscreen = 2;
}

void P_Stop (void)
{
	if (!IsTitleIntro() && !IsTitleScreen()) {
		S_StopSong();
	}

	DoubleBufferSetup();	// Clear frame buffers to black.

	if (IsLevelType(LevelType_SpecialStage)) {
		chaos_jagobj = NULL;
		nsshud_jagobj = NULL;
		nrng1_jagobj = NULL;
		nbrackt_jagobj = NULL;
		nbrackl_jagobj = NULL;
		nbrackr_jagobj = NULL;
		nbrackb_jagobj = NULL;
		narrow9_jagobj = NULL;
		hudNumberFont.charCache = NULL;
		hudNumberFont.charCacheLength = 0;
	}

	M_Stop();
	ClearCopper();
	Z_FreeTags (mainzone);
}

void P_Update (void)
{
	I_Update();
}
