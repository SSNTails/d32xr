/* D_main.c  */
 
#include "doomdef.h"
#include "v_font.h"
#include "r_local.h"

#ifdef MARS
#include "marshw.h"
#endif

#include <string.h>

#ifdef SKYDEBUG
uint8_t load_sky_lump_scroll_a = 0;
uint8_t load_sky_lump_scroll_b = 0;
uint8_t load_sky_lump_copper = 0;
uint8_t load_sky_lump_metadata = 0;
uint8_t load_sky_lump_palette = 0;
uint8_t load_sky_lump_tiles = 0;
#endif

#ifdef CPUDEBUG
uint16_t cpu_pulse_count = 0;
uint16_t cpu_pulse_timeout = 0;

uint32_t cpu_debug_pr = 0;
#endif

uint8_t cheats_enabled = 0;

uint8_t cheat_metrics_button_index = 0;

#define CHEAT_METRICS_SEQ_LENGTH	5
const uint8_t cheat_metrics_sequence[CHEAT_METRICS_SEQ_LENGTH] = { BT_UP, BT_DOWN, BT_DOWN, BT_DOWN, BT_UP };

boolean		splitscreen = false;
VINT		controltype = 0;		/* determine settings for BT_* */

boolean		sky_md_layer = false;
boolean		sky_32x_layer = false;

boolean		h40_sky = false;

boolean		h32_adjust = true;

VINT			gamevbls;		/* may not really be vbls in multiplayer */
VINT			vblsinframe;		/* range from ticrate to ticrate*2 */

VINT		maxlevel;			/* highest level selectable in menu (1-25) */

int 		ticstart;

#ifdef PLAY_POS_DEMO
	int			realtic;
#endif

unsigned configuration[NUMCONTROLOPTIONS][3] =
{
#ifdef SHOW_DISCLAIMER
	{BT_SPIN, BT_JUMP, BT_SPIN},
#else
	{BT_FLIP, BT_JUMP, BT_SPIN},
#endif
};

/*============================================================================ */


#define WORDMASK	3

/*
====================
=
= D_memset
=
====================
*/

void D_memset (void *dest, int val, size_t count)
{
	byte	*p;
	int		*lp;

/* round up to nearest word */
	p = dest;
	while ((int)p & WORDMASK)
	{
		if (count-- == 0)
			return;
		*p++ = val;
	}
	
/* write 32 bytes at a time */
	lp = (int *)p;
	val = (val<<24) | (val<<16) | (val<<8) | val;
	while (count >= 32)
	{
		lp[0] = lp[1] = lp[2] = lp[3] = lp[4] = lp[5] = lp[6] = lp[7] = val;
		lp += 8;
		count -= 32;
	}
	
/* finish up */
	p = (byte *)lp;
	while (count > 0)
	{
		*p++ = val;
		count--;
	}
}

#ifdef MARS
void fast_memcpy(void *dest, const void *src, int count);
#endif

void D_memcpy (void *dest, const void *src, size_t count)
{
	byte	*d;
	const byte *s;

	if (dest == src)
		return;

#ifdef MARS
	if ( (((intptr_t)dest & 15) == ((intptr_t)src & 15)) && (count > 16)) {
		unsigned i;
		unsigned rem = ((intptr_t)dest & 15), wordbytes;

		d = (byte*)dest;
		s = (const byte*)src;
		for (i = 0; i < rem; i++)
			*d++ = *s++;

		wordbytes = (unsigned)(count - rem) >> 2;
		fast_memcpy(d, s, wordbytes);

		wordbytes <<= 2;
		d += wordbytes;
		s += wordbytes;

		for (i = rem + wordbytes; i < (unsigned)count; i++)
			*d++ = *s++;
		return;
	}
#endif

	d = (byte *)dest;
	s = (const byte *)src;
	while (count--)
		*d++ = *s++;
}


void D_strncpy (char *dest, const char *src, int maxcount)
{
	byte	*p1;
	const byte *p2;

	p1 = (byte *)dest;
	p2 = (const byte *)src;
	while (maxcount--)
		if (! (*p1++ = *p2++) )
			return;
}

int D_strncasecmp (const char *s1, const char *s2, int len)
{
	while (*s1 && *s2)
	{
		int c1 = *s1, c2 = *s2;

		if (c1 >= 'A' && c1 <= 'Z') c1 += 'a' - 'A';
		if (c2 >= 'A' && c2 <= 'Z') c2 += 'a' - 'A';

		if (c1 != c2)
			return c1 - c2;

		s1++;
		s2++;

		if (!--len)
			return 0;
	}
	return *s1 - *s2;
}

// insertion sort
void D_isort(int* a, int len)
{
	int i, j;
	for (i = 1; i < len; i++)
	{
		int t = a[i];
		for (j = i - 1; j >= 0; j--)
		{
			if (a[j] <= t)
				break;
			a[j+1] = a[j];
		}
		a[j+1] = t;
	}
}



/*
===============
=
= M_Random
=
= Returns a 0-255 number
=
===============
*/

unsigned char rndtable[256] = {
	0,   8, 109, 220, 222, 241, 149, 107,  75, 248, 254, 140,  16,  66 ,
	74,  21, 211,  47,  80, 242, 154,  27, 205, 128, 161,  89,  77,  36 ,
	95, 110,  85,  48, 212, 140, 211, 249,  22,  79, 200,  50,  28, 188 ,
	52, 140, 202, 120,  68, 145,  62,  70, 184, 190,  91, 197, 152, 224 ,
	149, 104,  25, 178, 252, 182, 202, 182, 141, 197,   4,  81, 181, 242 ,
	145,  42,  39, 227, 156, 198, 225, 193, 219,  93, 122, 175, 249,   0 ,
	175, 143,  70, 239,  46, 246, 163,  53, 163, 109, 168, 135,   2, 235 ,
	25,  92,  20, 145, 138,  77,  69, 166,  78, 176, 173, 212, 166, 113 ,
	94, 161,  41,  50, 239,  49, 111, 164,  70,  60,   2,  37, 171,  75 ,
	136, 156,  11,  56,  42, 146, 138, 229,  73, 146,  77,  61,  98, 196 ,
	135, 106,  63, 197, 195,  86,  96, 203, 113, 101, 170, 247, 181, 113 ,
	80, 250, 108,   7, 255, 237, 129, 226,  79, 107, 112, 166, 103, 241 ,
	24, 223, 239, 120, 198,  58,  60,  82, 128,   3, 184,  66, 143, 224 ,
	145, 224,  81, 206, 163,  45,  63,  90, 168, 114,  59,  33, 159,  95 ,
	28, 139, 123,  98, 125, 196,  15,  70, 194, 253,  54,  14, 109, 226 ,
	71,  17, 161,  93, 186,  87, 244, 138,  20,  52, 123, 251,  26,  36 ,
	17,  46,  52, 231, 232,  76,  31, 221,  84,  37, 216, 165, 212, 106 ,
	197, 242,  98,  43,  39, 175, 254, 145, 190,  84, 118, 222, 187, 136 ,
	120, 163, 236, 249 
};
VINT	rndindex = 0;
VINT prndindex = 0;
uint32_t rng_seed = 0;

int P_Random (void)
{
	prndindex = (prndindex+1)&0xff;
	return rndtable[prndindex];
}

fixed_t P_RandomFixed(void)
{
	return (P_Random() << 8 | P_Random());
}

int P_RandomKey(int max)
{
	return (P_RandomFixed() * max) >> FRACBITS;
}

int M_Random (void)
{
	rndindex = (rndindex+1)&0xff;
	return rndtable[rndindex];
}

void M_ClearRandom (void)
{
	rndindex = prndindex = 0;
}

void P_RandomSeed(int seed)
{
	prndindex = seed & 0xff;
}

uint16_t P_Random16()
{
	unsigned int d0 = rng_seed;

	if (d0 == 0) {
		d0 = 0x2A6D365B;
	}

	unsigned int d1 = d0 * 41;

	d0 = (d0 & 0xFFFF0000) | (d1 & 0xFFFF);
	d1 = (d1 << 16) | ((d1 >> 16) & 0xFFFF);
	d0 += (d1 & 0xFFFF);
	d1 = (d1 & 0xFFFF0000) | (d0 & 0xFFFF);
	d1 = (d1 << 16) | ((d1 >> 16) & 0xFFFF);

	rng_seed = d1;

	return d0;
}


void M_ClearBox (fixed_t *box)
{
	box[BOXTOP] = box[BOXRIGHT] = D_MININT;
	box[BOXBOTTOM] = box[BOXLEFT] = D_MAXINT;
}

void M_AddToBox (fixed_t *box, fixed_t x, fixed_t y)
{
	if (x<box[BOXLEFT])
		box[BOXLEFT] = x;
	else if (x>box[BOXRIGHT])
		box[BOXRIGHT] = x;
	if (y<box[BOXBOTTOM])
		box[BOXBOTTOM] = y;
	else if (y>box[BOXTOP])
		box[BOXTOP] = y;
}

  
/*=============================================================================  */
 
static inline unsigned LocalToNet (unsigned cmd)
{
	return cmd;
}

static inline unsigned NetToLocal (unsigned cmd)
{
	return cmd;
}

 
/*=============================================================================  */

#ifndef SHOW_DISCLAIMER
VINT debugCounter = 0;
#endif
VINT		accum_time;
VINT		frames_to_skip = 0;
VINT		ticrate = 4;
VINT		ticsinframe;	/* how many tics since last drawer */
int		ticon;
int		frameon;
int		ticbuttons[MAXPLAYERS];
int		oldticbuttons[MAXPLAYERS];
int		ticrealbuttons, oldticrealbuttons;

#ifdef KIOSK_MODE
uint16_t kiosk_timeout_count;
#endif

extern	VINT	lasttics;

consistencymobj_t	emptymobj;
 
/*
===============
=
= MiniLoop
=
===============
*/

int last_frt_count = 0;
int total_frt_count = 0;
boolean optionsMenuOn = false;
int Mars_FRTCounter2Msec(int c);

int MiniLoop ( void (*start)(void),  void (*stop)(void)
		,  int (*ticker)(void), void (*drawer)(void)
		,  void (*update)(void) )
{
	int		i;
	int		exit;
	int		buttons;
	boolean firstdraw = true;

/* */
/* setup (cache graphics, etc) */
/* */
	start ();
	exit = 0;
	
	ticon = 0;
	frameon = 0;
	
	gametic = 0;
	leveltime = 0;

	gameaction = 0;
	gamevbls = 0;
	vblsinframe = 0;
	lasttics = 0;
	last_frt_count = 0;
	total_frt_count = 0;

#ifdef KIOSK_MODE
	kiosk_timeout_count = 0;
#endif

	ticbuttons[0] = ticbuttons[1] = oldticbuttons[0] = oldticbuttons[1] = 0;

	do
	{
		ticstart = I_GetFRTCounter();

/* */
/* adaptive timing based on previous frame */
/* */
		vblsinframe = TICVBLS;

		int frt_count = I_GetFRTCounter();

		if (last_frt_count == 0)
			last_frt_count = frt_count;

		accum_time = 0;
		// Frame skipping based on FRT count
		total_frt_count += Mars_FRTCounter2Msec(frt_count - last_frt_count);
		const int frametime = 1000/30;//I_IsPAL() ? 1000/25 : 1000/30;

		while (total_frt_count > frametime)
		{
			accum_time++;
			total_frt_count -= frametime;
		}

		if (accum_time > 3)
			accum_time = 3;

		last_frt_count = frt_count;

		if (optionsMenuOn || IsTitleScreen() || leveltime < TICRATE / 4) // Don't include map loading times into frameskip calculation
		{
			accum_time = 1;
			total_frt_count = 0;
		}

#ifdef CPUDEBUG
		cpu_pulse_count = 0;
#endif

		/* */
		/* get buttons for next tic */
		/* */
		oldticbuttons[0] = ticbuttons[0];
		oldticbuttons[1] = ticbuttons[1];
		oldticrealbuttons = ticrealbuttons;

		buttons = I_ReadControls();

#ifdef SKYDEBUG
		if (!gamepaused && oldticrealbuttons == BT_MODE && buttons & BT_MODE) {
			if (buttons & BT_A) {
				load_sky_lump_scroll_a++;
				if (load_sky_lump_scroll_a > 6) {
					load_sky_lump_scroll_a = 0;
				}
			}
			if (buttons & BT_B) {
				load_sky_lump_scroll_b++;
				if (load_sky_lump_scroll_b > 6) {
					load_sky_lump_scroll_b = 0;
				}
			}
			if (buttons & BT_C) {
				load_sky_lump_copper++;
				if (load_sky_lump_copper > 6) {
					load_sky_lump_copper = 0;
				}
			}
			if (buttons & BT_X) {
				load_sky_lump_palette++;
				if (load_sky_lump_palette > 6) {
					load_sky_lump_palette = 0;
				}
			}
			if (buttons & BT_Y) {
				load_sky_lump_tiles++;
				if (load_sky_lump_tiles > 6) {
					load_sky_lump_tiles = 0;
				}
			}
			if (buttons & BT_Z) {
				load_sky_lump_metadata++;
				if (load_sky_lump_metadata > 6) {
					load_sky_lump_metadata = 0;
				}
			}
		}
#endif

		if (IsDemoModeType(DemoMode_Playback) && buttons & BT_START) {
			exit = ga_exitdemo;
			break;
		}
		
		ticbuttons[consoleplayer] = buttons;
		ticrealbuttons = buttons;

		if (IsTitleIntro()) {
			if (oldticbuttons[0] == 0) {
				if (buttons == cheat_metrics_sequence[cheat_metrics_button_index]) {
					cheat_metrics_button_index += 1;
					if (cheat_metrics_button_index == CHEAT_METRICS_SEQ_LENGTH) {
						S_StartSoundId(sfx_s3k_33);
						cheats_enabled |= (CHEAT_METRICS | CHEAT_GAMEMODE_SELECT);
						cheat_metrics_button_index = 0;
					}
				}
				else if (buttons != 0) {
					cheat_metrics_button_index = 0;
				}
			}
		}
		else if (IsTitleScreen()) {
			int timeleft = gameinfo.titleTime - leveltime;
			if (timeleft <= 0) {
				R_FadePalette(dc_playpals, (PALETTE_SHIFT_CLASSIC_FADE_TO_BLACK + 20), dc_cshift_playpals);
				Mars_FadeMDPaletteFromBlack(0);
				copper_table_brightness = -31;
				effects_flags |= EFFECTS_COPPER_REFRESH;
				exit = ga_titleexpired;
				break;
			}
			if (timeleft <= 5) {
				int palette = PALETTE_SHIFT_CONVENTIONAL_FADE_TO_BLACK + (5 - timeleft);
				//int palette = PALETTE_SHIFT_CLASSIC_FADE_TO_BLACK + ((5 - timeleft) << 2);
				R_FadePalette(dc_playpals, palette, dc_cshift_playpals);
				R_FadeMDPaletteFromBlack(timeleft << 2);
				copper_table_brightness = -31 + (timeleft * 6);
				effects_flags |= EFFECTS_COPPER_REFRESH;
			}
			// Rotate on the title screen.
			ticbuttons[consoleplayer] = buttons = 0;
			players[0].mo->angle += TITLE_ANGLE_INC;
		}

#ifdef PLAY_POS_DEMO
		if (IsDemoModeType(DemoMode_Playback)) {
			players[0].mo->momx = 0;
			players[0].mo->momy = 0;
			players[0].mo->momz = 0;
		}
#endif

		gamevbls += vblsinframe;

		if (gameaction == ga_warped || gameaction == ga_startnew)
		{
			exit = gameaction;	/* hack for NeXT level reloading and net error */
			break;
		}

		if (!firstdraw)
		{
			if (update)
				update();
		}
		firstdraw = false;

		S_PreUpdateSounds();

		gametic++;
		ticon++;
		exit = ticker();

		S_UpdateSounds();

		/* */
		/* sync up with the refresh */
		/* */
		while (!I_RefreshCompleted())
			;

		drawer();

		#ifdef PLAY_POS_DEMO
		if (leveltime > 30) {
			V_DrawValueCenter(&menuFont, 160, 40, I_GetTime() - realtic);
			V_DrawValueCenter(&menuFont, 160, 50, I_GetFRTCounter());
		}
		else {
			realtic = I_GetTime();
		}
		#endif

#if 0
while (!I_RefreshCompleted ())
;	/* DEBUG */
#endif

#ifdef JAGUAR
		while ( DSPRead(&dspfinished) != 0xdef6 )
		;
#endif
	} while (exit == ga_nothing || exit == ga_demoending);

	stop ();
	S_Clear ();
	
	for (i = 0; i < MAXPLAYERS; i++)
		players[i].mo = (mobj_t*)&emptymobj;	/* for net consistency checks */

	return exit;
} 
 


/*=============================================================================  */

void ClearEEProm (void);
void DrawSinglePlaque (jagobj_t *pl);

jagobj_t	*titlepic;
byte    	*titlepic_a;
byte	    *titlepic_b;

int TIC_Abortable (void)
{
	if (ticon >= 90)
		return 1;

	return 0;
}

/*============================================================================= */

unsigned short screenCount = 0;
unsigned int frame_sync = 0;
int8_t selected_map = 0;
dmapinfo_t selected_map_info;

static jagobj_t *lvlsel_pic = NULL;
static jagobj_t *lvlsel_static[3] = { NULL, NULL, NULL };
static jagobj_t *arrowl_pic = NULL;
static jagobj_t *arrowr_pic = NULL;
static jagobj_t *chevblk_pic = NULL;

#ifdef SHOW_DISCLAIMER
	#define SELECTABLE_MAP_COUNT	7
	const int8_t selectable_maps[SELECTABLE_MAP_COUNT] = {0, 1, 2, 3, 4, 5, 6};
#else
	#define SELECTABLE_MAP_COUNT	gamemapcount
#endif

static VINT lvlsel_lump = -1;

int TIC_LevelSelect (void)
{
	int exit = ga_nothing;

	uint8_t effects_flags_queue = 0;

	screenCount++;

	if (gameaction == ga_nothing && !IsTransitionType(TransitionType_Leaving)) {
		if ((ticrealbuttons & BT_START && !(oldticrealbuttons & BT_START))
			|| (ticrealbuttons & BT_B && !(oldticrealbuttons & BT_B)))
		{
			fadetime = 0;
			SetTransition(TransitionType_Leaving);
		}
#ifdef KIOSK_MODE
		else if (screenCount >= KIOSK_LEVELSELECT_TIMEOUT) {
			fadetime = 0;
			SetTransition(TransitionType_Leaving);
		}
#endif
	}

	if (IsTransitionType(TransitionType_Entering)) {
		if (fadetime < 21) {
			int palette = PALETTE_SHIFT_CLASSIC_FADE_TO_BLACK + 20 - fadetime;
			R_FadePalette(dc_playpals, palette, dc_cshift_playpals);
			R_FadeMDPaletteFromBlack(fadetime);
			copper_table_brightness = -31 + fadetime + (fadetime >> 1);
			effects_flags_queue = EFFECTS_COPPER_REFRESH;
			fadetime++;
		}
		else {
			I_SetPalette(dc_playpals);
			Mars_FadeMDPaletteFromBlack(0xEEE);
			copper_table_brightness = 0;
			effects_flags_queue = EFFECTS_COPPER_REFRESH;
			SetTransition(TransitionType_None);
		}
	}
	else if (IsTransitionType(TransitionType_Leaving)) {
		if (fadetime < 21) {
			int palette = PALETTE_SHIFT_CLASSIC_FADE_TO_BLACK + fadetime;
			R_FadePalette(dc_playpals, palette, dc_cshift_playpals);
			R_FadeMDPaletteFromBlack(21 - fadetime);
			copper_table_brightness = 0 - fadetime - (fadetime >> 1);
			effects_flags_queue = EFFECTS_COPPER_REFRESH;
			fadetime++;
		}
		else {
			R_FadePalette(dc_playpals, (PALETTE_SHIFT_CLASSIC_FADE_TO_BLACK + 20), dc_cshift_playpals);
			Mars_FadeMDPaletteFromBlack(0);
			copper_table_brightness = -31;
			effects_flags_queue = EFFECTS_COPPER_REFRESH;
			exit = ga_startnew;
		}
	}
	else {
		int prev_selected_map = selected_map;

		if (ticrealbuttons & BT_LEFT && !(oldticrealbuttons & BT_LEFT)) {
			selected_map -= 1;
			if (selected_map < 0) {
				selected_map = SELECTABLE_MAP_COUNT-1;
			}
#ifdef SHOW_DISCLAIMER
			if (emeralds != 63 && selected_map == 6) {
				// Skip CEZ1
				selected_map -= 1;
			}
#endif
		}
		else if (ticrealbuttons & BT_RIGHT && !(oldticrealbuttons & BT_RIGHT)) {
			selected_map += 1;
#ifdef SHOW_DISCLAIMER
			if (emeralds != 63 && selected_map == 6) {
				// Skip CEZ1
				selected_map += 1;
			}
#endif
			if (selected_map == SELECTABLE_MAP_COUNT) {
				selected_map = 0;
			}
		}

		if (selected_map != prev_selected_map) {
#ifdef SHOW_DISCLAIMER
			// Show a filtered list of stages.
			startmap = gamemapnumbers[selectable_maps[selected_map]];
#else
			// Show all the stages.
			startmap = gamemapnumbers[selected_map];
#endif

			char lvlsel_name[9] = { 'L','V','L','S','E','L','0','0','\0' };

			lvlsel_name[6] += (startmap / 10);
			lvlsel_name[7] += (startmap % 10);

			lvlsel_lump = W_CheckNumForName(lvlsel_name);
			if (lvlsel_lump != -1)
				W_ReadLump(lvlsel_lump, lvlsel_pic);

			if (gamemapinfo.data)
				Z_Free(gamemapinfo.data);
			gamemapinfo.data = NULL;
			D_memset(&gamemapinfo, 0, sizeof(gamemapinfo));

			char buf[512];
			G_FindMapinfo(G_LumpNumForMapNum(startmap), &selected_map_info, buf);

			if (copper_table_selection & 0xF) {
				copper_table_selection ^= 0x10;
			}

			int next_bank = (copper_table_selection >> 4) ^ 1;
			if (R_SetupCopperTable("MENU", selected_map_info.zone, next_bank) == -1)
			{
				R_SetupCopperTable("MENU", 0, next_bank);
			}
			if (R_SetupMDPalettes("MENU", selected_map_info.zone, 1, 0) == -1)
			{
				R_SetupMDPalettes("MENU", 0, 1, 0);
			}

			// Some stage selections cause the wrong neutral color to be used. Why??
			// TODO: Do we actually need to keep two neutral colors in memory?
			copper_neutral_color[next_bank^1] = copper_neutral_color[next_bank];

			Mars_FadeMDPaletteFromBlack(0xEEE);

			copper_table_selection &= 0x10;

			copper_table_selection++;

			Mars_CrossfadeMDPalette(copper_table_selection & 0xF);

			effects_flags_queue = EFFECTS_COPPER_REFRESH;
		}
		else if (copper_table_selection & 0xF) {
			copper_table_selection++;

			if (!(copper_table_selection & 0xF)) {
				copper_table_selection &= 0x10;
				Mars_CrossfadeMDPalette(0x10);
			}
			else {
				Mars_CrossfadeMDPalette(copper_table_selection & 0xF);
			}

			effects_flags_queue = EFFECTS_COPPER_REFRESH;
		}
	}

	effects_flags |= effects_flags_queue;

	return exit;
}

void START_LevelSelect (void)
{
	DoubleBufferSetup();	// Clear frame buffers to black.

	screenCount = 0;

	fadetime = 0;

	startmap = 1;

	I_SetPalette(dc_playpals);

	R_InitColormap();

	R_SetupBackground("MENU", 1, 1);
	R_SetupCopperTable("MENU", 1, 1);

	// Set to totally black
	R_FadePalette(dc_playpals, (PALETTE_SHIFT_CLASSIC_FADE_TO_BLACK + 20), dc_cshift_playpals);
	Mars_FadeMDPaletteFromBlack(0);
	copper_table_brightness = -31;
	effects_flags |= EFFECTS_COPPER_REFRESH;

	SetTransition(TransitionType_Entering);

	lvlsel_lump = W_CheckNumForName("LVLSEL01");
	lvlsel_pic = W_CacheLumpNum(lvlsel_lump, PU_STATIC);
	lvlsel_static[0] = W_CacheLumpName("LVLSELS1", PU_STATIC);
	lvlsel_static[1] = W_CacheLumpName("LVLSELS2", PU_STATIC);
	lvlsel_static[2] = W_CacheLumpName("LVLSELS3", PU_STATIC);

	arrowl_pic = W_CacheLumpName("ARROWL", PU_STATIC);
	arrowr_pic = W_CacheLumpName("ARROWR", PU_STATIC);
	chevblk_pic = W_CacheLumpName("CHEVBLK", PU_STATIC);
	
#ifdef KIOSK_MODE
	hudNumberFont.charCacheLength = 6;
	hudNumberFont.charCache = Z_Calloc(sizeof(void*) * 6, PU_STATIC);
	hudNumberFont.charCache[1] = W_CacheLumpName("STTNUM1", PU_STATIC);
	hudNumberFont.charCache[2] = W_CacheLumpName("STTNUM2", PU_STATIC);
	hudNumberFont.charCache[3] = W_CacheLumpName("STTNUM3", PU_STATIC);
	hudNumberFont.charCache[4] = W_CacheLumpName("STTNUM4", PU_STATIC);
	hudNumberFont.charCache[5] = W_CacheLumpName("STTNUM5", PU_STATIC);
#endif

	if (gamemapinfo.data)
		Z_Free(gamemapinfo.data);
	gamemapinfo.data = NULL;
	D_memset(&gamemapinfo, 0, sizeof(gamemapinfo));
	
	char buf[512];
	G_FindMapinfo(G_LumpNumForMapNum(1), &selected_map_info, buf);

	clearscreen = 2;

	for (int i = 0; i < 2; i++)
	{
		I_FillFrameBuffer(COLOR_THRU);
		UpdateBuffer();
	}
}

#ifdef MEMDEBUG
boolean debugStop = false;
#endif

void STOP_LevelSelect (void)
{
	DoubleBufferSetup();	// Clear frame buffers to black.

	Z_Free(lvlsel_pic);

	for (int i = 0; i < 3; i++)
		Z_Free(lvlsel_static[i]);

	Z_Free(arrowl_pic);
	Z_Free(arrowr_pic);
	Z_Free(chevblk_pic);

#ifdef KIOSK_MODE
	for (int i = 5; i > 0; i--) {
		Z_Free(hudNumberFont.charCache[i]);
	}
	Z_Free(hudNumberFont.charCache);
	hudNumberFont.charCache = NULL;
	hudNumberFont.charCacheLength = 0;
#endif

	ClearCopper();
}

void ClearCopper()
{
	if (copper_source_table[0])
	{
		Z_Free(copper_source_table[0]);
		copper_source_table[0] = NULL;
	}

	if (copper_source_table[1])
	{
		Z_Free(copper_source_table[1]);
		copper_source_table[1] = NULL;
	}

	if (copper_buffer)
	{
		Z_Free(copper_buffer);
		copper_buffer = NULL;
	}
}

void DRAW_LevelSelect (void)
{
	while (frame_sync == mars_vblank_count);
	frame_sync = mars_vblank_count;
	
	if (clearscreen > 0) {
		I_ResetLineTable();

		uint16_t *lines = Mars_FrameBufferLines();
		uint16_t pixel_offset = (512>>1);
		for (int i=0; i <= 17; i++) {
			lines[i] = pixel_offset;
			lines[223-i] = pixel_offset;
			pixel_offset += (352>>1);
		}

		clearscreen--;
	}

	if (screenCount < 4) {
		for (int i=0; i < 0x160; i += 0x20) {
			DrawJagobj2(chevblk_pic, i, 0, 352);
		}

		// Draw text
		V_DrawStringCenterWithColormap(&menuFont, 160, 32, "SELECT A STAGE", YELLOWTEXTCOLORMAP);

		// Draw black lines
		DrawLine(86, 58, 152, 0x1F, false);
		DrawLine(86, 193, 152, 0x1F, false);
		DrawLine(86, 59, 134, 0x1F, true);
		DrawLine(237, 59, 134, 0x1F, true);

		// Draw red lines
		DrawLine(84, 56, 152, 0x23, false);
		DrawLine(84, 191, 152, 0x23, false);
		DrawLine(84, 57, 134, 0x23, true);
		DrawLine(235, 57, 134, 0x23, true);

		// Draw level picture
		DrawJagobj(lvlsel_pic, (320-96)>>1, 72);
	}

	Mars_SetScrollPositions(0, screenCount >> 1, 0, 0);

	// Scroll chevrons
	uint16_t *lines = Mars_FrameBufferLines();
	uint16_t pixel_offset = (512>>1);
	for (int i=0; i < 16; i++) {
		lines[i] = pixel_offset + (15 - ((screenCount>>1) & 15));
		lines[223-i] = pixel_offset + ((screenCount>>1) & 15);
		pixel_offset += (352>>1);
	}

	// Move arrows
	int arrow_offset = ((screenCount>>2) & 7) * (((screenCount>>2) & 0x8) == 0);
	if (arrow_offset > 3) {
		arrow_offset = 7 - arrow_offset;
	}

	pixel_t* background;

	if ((((screenCount & 0x20) == 0) && ((screenCount & 0x2) == 0)) ||
		(((screenCount & 0x20) != 0) && ((screenCount & 0x1E) == 0)))
	{
		// Clear left arrow
		background = I_FrameBuffer() + (((320*112) + ((320-16)>>1)-96-4) >> 1);

		for (int y=0; y < 29; y++) {
			for (int x=0; x < (24>>3); x++) {
				// Write 8 thru pixels
				*background++ = COLOR_THRU_2;
				*background++ = COLOR_THRU_2;
				*background++ = COLOR_THRU_2;
				*background++ = COLOR_THRU_2;
			}

			background += (296>>1);
		}

		// Clear right arrow
		background = I_FrameBuffer() + (((320*112) + ((320-16)>>1)+96-4) >> 1);

		for (int y=0; y < 29; y++) {
			for (int x=0; x < (24>>3); x++) {
				// Write 8 thru pixels
				*background++ = COLOR_THRU_2;
				*background++ = COLOR_THRU_2;
				*background++ = COLOR_THRU_2;
				*background++ = COLOR_THRU_2;
			}

			background += (296>>1);
		}

		// Draw arrows
		DrawJagobj(arrowl_pic, ((320-16)>>1)-96 - arrow_offset, 112);
		DrawJagobj(arrowr_pic, ((320-16)>>1)+96 + arrow_offset, 112);
	}

	if (screenCount < 4 || ((copper_table_selection & 0xF) && (copper_table_selection & 0xF) < 3)) {
		// Clear level name text
		background = I_FrameBuffer() + (((320*160) + ((320>>1)-64)) >> 1);

		for (int y=0; y < 20; y++) {
			for (int x=0; x < (128>>3); x++) {
				// Write 8 thru pixels
				*background++ = COLOR_THRU_2;
				*background++ = COLOR_THRU_2;
				*background++ = COLOR_THRU_2;
				*background++ = COLOR_THRU_2;
			}

			background += (192>>1);
		}

		// Draw text
		V_DrawStringCenterWithColormap(&menuFont, 160, 160, selected_map_info.name, YELLOWTEXTCOLORMAP);

		if (selected_map_info.act > 0) {
			char act_string[6] = { 'A','C','T',' ','0','\0' };
			act_string[4] += selected_map_info.act;
			V_DrawStringCenterWithColormap(&menuFont, 160, 172, act_string, YELLOWTEXTCOLORMAP);
		}
	}

	// Draw level picture
	if (lvlsel_lump < 0) {
		if ((copper_table_selection & 0xF) && (copper_table_selection & 0xF) < 3) {
			background = I_FrameBuffer() + (((320*(72-2)) + ((320-96)>>1)-3) >> 1);

			for (int y=0; y < 76; y++) {
				for (int x=0; x < (104>>3); x++) {
					// Write 8 thru pixels
					*background++ = COLOR_THRU_2;
					*background++ = COLOR_THRU_2;
					*background++ = COLOR_THRU_2;
					*background++ = COLOR_THRU_2;
				}

				background += (216>>1);
			}
		}

		DrawJagobj(lvlsel_static[((screenCount >> 2) % 3)], (320-96)>>1, 72);
	}
	else if ((copper_table_selection & 0xF) && (copper_table_selection & 0xF) < 8) {
		uint16_t size_table[5] = { 2048, 3547, 4096, 3547, 2048 };
		uint8_t x_offset_table[5] = { 2, 2, 3, 2, 2 };
		uint8_t y_offset_table[5] = { 1, 2, 2, 2, 1 };

		int size_index = (copper_table_selection & 0xF) - 1;
		
		if (size_index < 5) {
			int x = ((320-96)>>1) - x_offset_table[size_index];
			int y = 72 - y_offset_table[size_index];
			fixed_t size_scale = FRACUNIT + size_table[size_index];

			// 98 x 74
			// 100 x 75
			// 102 x 76
			if (size_index >= 3) {
				background = I_FrameBuffer() + (((320*(72-2)) + ((320-96)>>1)-3) >> 1);

				for (int y=0; y < 76; y++) {
					for (int x=0; x < (104>>3); x++) {
						// Write 8 thru pixels
						*background++ = COLOR_THRU_2;
						*background++ = COLOR_THRU_2;
						*background++ = COLOR_THRU_2;
						*background++ = COLOR_THRU_2;
					}

					background += (216>>1);
				}
			}

			DrawScaledJagobj(lvlsel_pic, x, y, size_scale, size_scale, I_OverwriteBuffer());
		}
		else {
			background = I_FrameBuffer() + (((320*(72-2)) + ((320-96)>>1)-3) >> 1);

			for (int y=0; y < 76; y++) {
				for (int x=0; x < (104>>3); x++) {
					// Write 8 thru pixels
					*background++ = COLOR_THRU_2;
					*background++ = COLOR_THRU_2;
					*background++ = COLOR_THRU_2;
					*background++ = COLOR_THRU_2;
				}

				background += (216>>1);
			}

			DrawJagobj(lvlsel_pic, (320-96)>>1, 72);
		}
	}

#ifdef KIOSK_MODE
	// Clear countdown digits
	background = I_FrameBuffer() + (((320*177) + 272) >> 1);

	for (int y=0; y < 31; y++) {
		for (int x=0; x < (24>>3); x++) {
			// Write 8 thru pixels
			*background++ = COLOR_THRU_2;
			*background++ = COLOR_THRU_2;
			*background++ = COLOR_THRU_2;
			*background++ = COLOR_THRU_2;
		}

		background += (296>>1);
	}

	int countdown = (KIOSK_LEVELSELECT_TIMEOUT/60) - (screenCount/60);
	if (countdown > 5) {
		V_DrawValueLeft(&hudNumberFont, 280, 184, (KIOSK_LEVELSELECT_TIMEOUT/60) - (screenCount/60));
	}
	else if (countdown > 0) {
		int size_index = (((screenCount<<4) / 15) & 63);
		fixed_t size_scale = FRACUNIT + (2048 * (63 - size_index));
		DrawScaledJagobj(
				hudNumberFont.charCache[countdown],
				280-8+(size_index>>2)-(size_index>>3),
				184-7+(size_index>>3),
				size_scale,
				size_scale,
				I_OverwriteBuffer());
	}
#endif
}

/*============================================================================= */

int offsetX_Gen = 0;
int offsetY_Gen = 0;
int offsetX_32x = 0;
int offsetY_32x = 0;
int genRes = 0x81;

					//   LOW     HIGH    BLACK
uint16_t colorGen[3] = { 0x00E0, 0x000E, 0x0000 };
uint16_t color32x[3] = { 0x3000, 0xD100, 0x03FF };
					//   THRU    HIGH    LOW

int colorSelect = 0;

int TIC_Test(void)
{
	screenCount++;

	// Read directly from the controller. Works better in this function. Don't know why!
	if ((Mars_ReadController(0) & BT_START)) {
		return ga_closeprompt;
	}

	if (ticrealbuttons & BT_A) {
		if (ticrealbuttons & BT_UP && !(oldticrealbuttons & BT_UP)) {
			colorGen[colorSelect] = (colorGen[colorSelect] & 0xEE0) | ((colorGen[colorSelect] + 0x2) & 0xE);
		}
		if (ticrealbuttons & BT_DOWN && !(oldticrealbuttons & BT_DOWN)) {
			colorGen[colorSelect] = (colorGen[colorSelect] & 0xEE0) | ((colorGen[colorSelect] - 0x2) & 0xE);
		}
	}
	if (ticrealbuttons & BT_B) {
		if (ticrealbuttons & BT_UP && !(oldticrealbuttons & BT_UP)) {
			colorGen[colorSelect] = (colorGen[colorSelect] & 0xE0E) | ((colorGen[colorSelect] + 0x20) & 0xE0);
		}
		if (ticrealbuttons & BT_DOWN && !(oldticrealbuttons & BT_DOWN)) {
			colorGen[colorSelect] = (colorGen[colorSelect] & 0xE0E) | ((colorGen[colorSelect] - 0x20) & 0xE0);
		}
	}
	if (ticrealbuttons & BT_C) {
		if (ticrealbuttons & BT_UP && !(oldticrealbuttons & BT_UP)) {
			colorGen[colorSelect] = (colorGen[colorSelect] & 0x0EE) | ((colorGen[colorSelect] + 0x200) & 0xE00);
		}
		if (ticrealbuttons & BT_DOWN && !(oldticrealbuttons & BT_DOWN)) {
			colorGen[colorSelect] = (colorGen[colorSelect] & 0x0EE) | ((colorGen[colorSelect] - 0x200) & 0xE00);
		}
	}
	if (ticrealbuttons & BT_X) {
		if (ticrealbuttons & BT_UP && !(oldticrealbuttons & BT_UP)) {
			color32x[colorSelect] = (color32x[colorSelect] & 0xFFE0) | ((color32x[colorSelect] + 0x4) & 0x1F);
		}
		if (ticrealbuttons & BT_DOWN && !(oldticrealbuttons & BT_DOWN)) {
			color32x[colorSelect] = (color32x[colorSelect] & 0xFFE0) | ((color32x[colorSelect] - 0x4) & 0x1F);
		}
	}
	if (ticrealbuttons & BT_Y) {
		if (ticrealbuttons & BT_UP && !(oldticrealbuttons & BT_UP)) {
			color32x[colorSelect] = (color32x[colorSelect] & 0xFC1F) | ((color32x[colorSelect] + 0x80) & 0x3E0);
		}
		if (ticrealbuttons & BT_DOWN && !(oldticrealbuttons & BT_DOWN)) {
			color32x[colorSelect] = (color32x[colorSelect] & 0xFC1F) | ((color32x[colorSelect] - 0x80) & 0x3E0);
		}
	}
	if (ticrealbuttons & BT_Z) {
		if (ticrealbuttons & BT_UP && !(oldticrealbuttons & BT_UP)) {
			color32x[colorSelect] = (color32x[colorSelect] & 0x83FF) | ((color32x[colorSelect] + 0x1000) & 0x7C00);
		}
		if (ticrealbuttons & BT_DOWN && !(oldticrealbuttons & BT_DOWN)) {
			color32x[colorSelect] = (color32x[colorSelect] & 0x83FF) | ((color32x[colorSelect] - 0x1000) & 0x7C00);
		}
	}

	if (ticrealbuttons & BT_MODE && !(oldticrealbuttons & BT_MODE)) {
		colorSelect++;
		if (colorSelect > 2) {
			colorSelect = 0;
		}
	}
/*
	if (ticrealbuttons & BT_MODE) {
		if (ticrealbuttons & BT_UP && !(oldticrealbuttons & BT_UP)) {
			offsetY_32x--;
			Mars_ScrollMDSky(430 + offsetX_32x, 40 + offsetY_32x, 0, 0);
		}
		else if (ticrealbuttons & BT_DOWN && !(oldticrealbuttons & BT_DOWN)) {
			offsetY_32x++;
			Mars_ScrollMDSky(430 + offsetX_32x, 40 + offsetY_32x, 0, 0);
		}

		if (ticrealbuttons & BT_LEFT && !(oldticrealbuttons & BT_LEFT)) {
			offsetX_32x--;
			Mars_ScrollMDSky(430 + offsetX_32x, 40 + offsetY_32x, 0, 0);
		}
		else if (ticrealbuttons & BT_RIGHT && !(oldticrealbuttons & BT_RIGHT)) {
			offsetX_32x++;
			Mars_ScrollMDSky(430 + offsetX_32x, 40 + offsetY_32x, 0, 0);
		}
	}
	else {
		if (ticrealbuttons & BT_UP && !(oldticrealbuttons & BT_UP)) {
			offsetY_Gen--;
			Mars_ScrollMDSky(430 + offsetX_Gen, 40 + offsetY_Gen, 0, 0);
		}
		else if (ticrealbuttons & BT_DOWN && !(oldticrealbuttons & BT_DOWN)) {
			offsetY_Gen++;
			Mars_ScrollMDSky(430 + offsetX_Gen, 40 + offsetY_Gen, 0, 0);
		}

		if (ticrealbuttons & BT_LEFT && !(oldticrealbuttons & BT_LEFT)) {
			offsetX_Gen--;
			Mars_ScrollMDSky(430 + offsetX_Gen, 40 + offsetY_Gen, 0, 0);
		}
		else if (ticrealbuttons & BT_RIGHT && !(oldticrealbuttons & BT_RIGHT)) {
			offsetX_Gen++;
			Mars_ScrollMDSky(430 + offsetX_Gen, 40 + offsetY_Gen, 0, 0);
		}
	}

	if (ticrealbuttons & BT_B && !(oldticrealbuttons & BT_B)) {
		int reg12_write = 0x8C00;
		genRes ^= 0x81;
		reg12_write |= genRes;
		Mars_WriteMDVDPRegister(reg12_write);
	}
*/
	return ga_nothing;
}

void START_Test (void)
{
	//DoubleBufferSetup();	// Clear frame buffers to black.

	for (int i = 0; i < 2; i++)
	{
		I_ClearFrameBuffer();
		UpdateBuffer();
	}
	
	//vd.viewangle = 0;
	//I_Update();
	Mars_ScrollMDSky(430, 128+40, 0, 0);
	//Mars_SetScrollPositions(0, 0, 0, 0);

	screenCount = 0;

	I_SetPalette(dc_playpals);
	Mars_FadeMDPaletteFromBlack(0xEEE);

	R_InitColormap();

	// Custom colors
	unsigned short* cram = (unsigned short *)&MARS_CRAM;
	cram[0x01] = 0x8000 | (0x0B << 10) | (0x00 << 5) | (0x00);		// Blue (high)
	cram[0x02] = 0x8000 | (0x00 << 10) | (0x0F << 5) | (0x00);		// Green (high)
	cram[0x81] = 0x0000 | (0x0B << 10) | (0x00 << 5) | (0x00);		// Blue (high)
	cram[0x82] = 0x0000 | (0x00 << 10) | (0x0F << 5) | (0x00);		// Green (high)

	R_SetupBackground("TEST", 1, 1);
	effects_flags &= (~EFFECTS_COPPER_ENABLED);

	clearscreen = 2;

	for (int i = 0; i < 2; i++)
	{
		I_FillFrameBuffer(COLOR_THRU);
		UpdateBuffer();
	}
}

void STOP_Test (void)
{
	// Set to totally black
	R_FadePalette(dc_playpals, (PALETTE_SHIFT_CLASSIC_FADE_TO_BLACK + 20), dc_cshift_playpals);
}

void DRAW_Test (void)
{
	//I_SetPalette(dc_playpals);
	if (screenCount < 4) {
		Mars_FadeMDPaletteFromBlack(0xEEE);
	}

	while (frame_sync == mars_vblank_count);
	frame_sync = mars_vblank_count;

	// Custom colors
	unsigned short* cram = (unsigned short *)&MARS_CRAM;
	//cram[0x00] = 0x0000 | (0x0C << 10);

	//cram[0x01] = 0x8000 | (0x1F << 10) | (0x1F << 5) | (0x1F);		// White (high)
	//cram[0x81] = 0x0000 | (0x0F << 10) | (0x0F << 5) | (0x0F);		// Grey (high)

	cram[0xFF] = color32x[0];
	cram[0x81] = color32x[1];
	cram[0x01] = color32x[2];

	Mars_SetMDColor(colorGen[0], 0);
	Mars_SetMDColor(colorGen[1], 17);
	Mars_SetMDColor(colorGen[2], 18);

	cram[0x20] = 0x8000 | (0x0C << 10) | (0x00 << 5) | (0x0C);
	cram[0x21] = 0x0000 | (0x00 << 10) | (0x0C << 5) | (0x00);
	
	if (clearscreen > 0) {
		I_ResetLineTable();
		clearscreen--;
	}

	pixel_t *framebuffer = I_FrameBuffer();


	framebuffer[((320/2) * 160) + ((64+0)/2)] = 0x2021;
	framebuffer[((320/2) * 160) + ((64+2)/2)] = 0x2021;
	framebuffer[((320/2) * 160) + ((64+4)/2)] = 0x2021;
	framebuffer[((320/2) * 160) + ((64+6)/2)] = 0x2021;


	/*for (int i=0; i < (160*224); i++) {
		framebuffer[i] = 0x0000;
	}*/

	// LOW | LOW
	framebuffer[((320/2) * 85) + ((14+0)/2)] = 0x8181;
	framebuffer[((320/2) * 85) + ((16+0)/2)] = 0x8181;
	framebuffer[((320/2) * 85) + ((18+0)/2)] = 0x8181;
	framebuffer[((320/2) * 85) + ((20+0)/2)] = 0x8181;
	framebuffer[((320/2) * 88) + ((14+0)/2)] = 0x8181;
	framebuffer[((320/2) * 88) + ((16+0)/2)] = 0x8181;
	framebuffer[((320/2) * 88) + ((18+0)/2)] = 0x8181;
	framebuffer[((320/2) * 88) + ((20+0)/2)] = 0x8181;

	framebuffer[((320/2) * 93) + ((14+0)/2)] = 0x8181;
	framebuffer[((320/2) * 93) + ((16+0)/2)] = 0x8181;
	framebuffer[((320/2) * 93) + ((18+0)/2)] = 0x8181;
	framebuffer[((320/2) * 93) + ((20+0)/2)] = 0x8181;
	framebuffer[((320/2) * 96) + ((14+0)/2)] = 0x8181;
	framebuffer[((320/2) * 96) + ((16+0)/2)] = 0x8181;
	framebuffer[((320/2) * 96) + ((18+0)/2)] = 0x8181;
	framebuffer[((320/2) * 96) + ((20+0)/2)] = 0x8181;

	framebuffer[((320/2) * 101) + ((14+0)/2)] = 0x8181;
	framebuffer[((320/2) * 101) + ((16+0)/2)] = 0x8181;
	framebuffer[((320/2) * 101) + ((18+0)/2)] = 0x8181;
	framebuffer[((320/2) * 101) + ((20+0)/2)] = 0x8181;
	framebuffer[((320/2) * 104) + ((14+0)/2)] = 0x8181;
	framebuffer[((320/2) * 104) + ((16+0)/2)] = 0x8181;
	framebuffer[((320/2) * 104) + ((18+0)/2)] = 0x8181;
	framebuffer[((320/2) * 104) + ((20+0)/2)] = 0x8181;

	framebuffer[((320/2) * 109) + ((14+0)/2)] = 0x8181;
	framebuffer[((320/2) * 109) + ((16+0)/2)] = 0x8181;
	framebuffer[((320/2) * 109) + ((18+0)/2)] = 0x8181;
	framebuffer[((320/2) * 109) + ((20+0)/2)] = 0x8181;
	framebuffer[((320/2) * 112) + ((14+0)/2)] = 0x8181;
	framebuffer[((320/2) * 112) + ((16+0)/2)] = 0x8181;
	framebuffer[((320/2) * 112) + ((18+0)/2)] = 0x8181;
	framebuffer[((320/2) * 112) + ((20+0)/2)] = 0x8181;

	// LOW | HIGH
	framebuffer[((320/2) * 85) + ((14+16)/2)] = 0x8181;
	framebuffer[((320/2) * 85) + ((16+16)/2)] = 0x8181;
	framebuffer[((320/2) * 85) + ((18+16)/2)] = 0x0101;
	framebuffer[((320/2) * 85) + ((20+16)/2)] = 0x0101;
	framebuffer[((320/2) * 88) + ((14+16)/2)] = 0x8181;
	framebuffer[((320/2) * 88) + ((16+16)/2)] = 0x8181;
	framebuffer[((320/2) * 88) + ((18+16)/2)] = 0x0101;
	framebuffer[((320/2) * 88) + ((20+16)/2)] = 0x0101;

	framebuffer[((320/2) * 93) + ((14+16)/2)] = 0x8181;
	framebuffer[((320/2) * 93) + ((16+16)/2)] = 0x8181;
	framebuffer[((320/2) * 93) + ((18+16)/2)] = 0x0101;
	framebuffer[((320/2) * 93) + ((20+16)/2)] = 0x0101;
	framebuffer[((320/2) * 96) + ((14+16)/2)] = 0x8181;
	framebuffer[((320/2) * 96) + ((16+16)/2)] = 0x8181;
	framebuffer[((320/2) * 96) + ((18+16)/2)] = 0x0101;
	framebuffer[((320/2) * 96) + ((20+16)/2)] = 0x0101;

	framebuffer[((320/2) * 101) + ((14+16)/2)] = 0x8181;
	framebuffer[((320/2) * 101) + ((16+16)/2)] = 0x8181;
	framebuffer[((320/2) * 101) + ((18+16)/2)] = 0x0101;
	framebuffer[((320/2) * 101) + ((20+16)/2)] = 0x0101;
	framebuffer[((320/2) * 104) + ((14+16)/2)] = 0x8181;
	framebuffer[((320/2) * 104) + ((16+16)/2)] = 0x8181;
	framebuffer[((320/2) * 104) + ((18+16)/2)] = 0x0101;
	framebuffer[((320/2) * 104) + ((20+16)/2)] = 0x0101;

	framebuffer[((320/2) * 109) + ((14+16)/2)] = 0x8181;
	framebuffer[((320/2) * 109) + ((16+16)/2)] = 0x8181;
	framebuffer[((320/2) * 109) + ((18+16)/2)] = 0x0101;
	framebuffer[((320/2) * 109) + ((20+16)/2)] = 0x0101;
	framebuffer[((320/2) * 112) + ((14+16)/2)] = 0x8181;
	framebuffer[((320/2) * 112) + ((16+16)/2)] = 0x8181;
	framebuffer[((320/2) * 112) + ((18+16)/2)] = 0x0101;
	framebuffer[((320/2) * 112) + ((20+16)/2)] = 0x0101;

	// HIGH | LOW
	framebuffer[((320/2) * 85) + ((14+32)/2)] = 0x0101;
	framebuffer[((320/2) * 85) + ((16+32)/2)] = 0x0101;
	framebuffer[((320/2) * 85) + ((18+32)/2)] = 0x8181;
	framebuffer[((320/2) * 85) + ((20+32)/2)] = 0x8181;
	framebuffer[((320/2) * 88) + ((14+32)/2)] = 0x0101;
	framebuffer[((320/2) * 88) + ((16+32)/2)] = 0x0101;
	framebuffer[((320/2) * 88) + ((18+32)/2)] = 0x8181;
	framebuffer[((320/2) * 88) + ((20+32)/2)] = 0x8181;

	framebuffer[((320/2) * 93) + ((14+32)/2)] = 0x0101;
	framebuffer[((320/2) * 93) + ((16+32)/2)] = 0x0101;
	framebuffer[((320/2) * 93) + ((18+32)/2)] = 0x8181;
	framebuffer[((320/2) * 93) + ((20+32)/2)] = 0x8181;
	framebuffer[((320/2) * 96) + ((14+32)/2)] = 0x0101;
	framebuffer[((320/2) * 96) + ((16+32)/2)] = 0x0101;
	framebuffer[((320/2) * 96) + ((18+32)/2)] = 0x8181;
	framebuffer[((320/2) * 96) + ((20+32)/2)] = 0x8181;

	framebuffer[((320/2) * 101) + ((14+32)/2)] = 0x0101;
	framebuffer[((320/2) * 101) + ((16+32)/2)] = 0x0101;
	framebuffer[((320/2) * 101) + ((18+32)/2)] = 0x8181;
	framebuffer[((320/2) * 101) + ((20+32)/2)] = 0x8181;
	framebuffer[((320/2) * 104) + ((14+32)/2)] = 0x0101;
	framebuffer[((320/2) * 104) + ((16+32)/2)] = 0x0101;
	framebuffer[((320/2) * 104) + ((18+32)/2)] = 0x8181;
	framebuffer[((320/2) * 104) + ((20+32)/2)] = 0x8181;

	framebuffer[((320/2) * 109) + ((14+32)/2)] = 0x0101;
	framebuffer[((320/2) * 109) + ((16+32)/2)] = 0x0101;
	framebuffer[((320/2) * 109) + ((18+32)/2)] = 0x8181;
	framebuffer[((320/2) * 109) + ((20+32)/2)] = 0x8181;
	framebuffer[((320/2) * 112) + ((14+32)/2)] = 0x0101;
	framebuffer[((320/2) * 112) + ((16+32)/2)] = 0x0101;
	framebuffer[((320/2) * 112) + ((18+32)/2)] = 0x8181;
	framebuffer[((320/2) * 112) + ((20+32)/2)] = 0x8181;

	// HIGH | HIGH
	framebuffer[((320/2) * 85) + ((14+48)/2)] = 0x0101;
	framebuffer[((320/2) * 85) + ((16+48)/2)] = 0x0101;
	framebuffer[((320/2) * 85) + ((18+48)/2)] = 0x0101;
	framebuffer[((320/2) * 85) + ((20+48)/2)] = 0x0101;
	framebuffer[((320/2) * 88) + ((14+48)/2)] = 0x0101;
	framebuffer[((320/2) * 88) + ((16+48)/2)] = 0x0101;
	framebuffer[((320/2) * 88) + ((18+48)/2)] = 0x0101;
	framebuffer[((320/2) * 88) + ((20+48)/2)] = 0x0101;

	framebuffer[((320/2) * 93) + ((14+48)/2)] = 0x0101;
	framebuffer[((320/2) * 93) + ((16+48)/2)] = 0x0101;
	framebuffer[((320/2) * 93) + ((18+48)/2)] = 0x0101;
	framebuffer[((320/2) * 93) + ((20+48)/2)] = 0x0101;
	framebuffer[((320/2) * 96) + ((14+48)/2)] = 0x0101;
	framebuffer[((320/2) * 96) + ((16+48)/2)] = 0x0101;
	framebuffer[((320/2) * 96) + ((18+48)/2)] = 0x0101;
	framebuffer[((320/2) * 96) + ((20+48)/2)] = 0x0101;

	framebuffer[((320/2) * 101) + ((14+48)/2)] = 0x0101;
	framebuffer[((320/2) * 101) + ((16+48)/2)] = 0x0101;
	framebuffer[((320/2) * 101) + ((18+48)/2)] = 0x0101;
	framebuffer[((320/2) * 101) + ((20+48)/2)] = 0x0101;
	framebuffer[((320/2) * 104) + ((14+48)/2)] = 0x0101;
	framebuffer[((320/2) * 104) + ((16+48)/2)] = 0x0101;
	framebuffer[((320/2) * 104) + ((18+48)/2)] = 0x0101;
	framebuffer[((320/2) * 104) + ((20+48)/2)] = 0x0101;

	framebuffer[((320/2) * 109) + ((14+48)/2)] = 0x0101;
	framebuffer[((320/2) * 109) + ((16+48)/2)] = 0x0101;
	framebuffer[((320/2) * 109) + ((18+48)/2)] = 0x0101;
	framebuffer[((320/2) * 109) + ((20+48)/2)] = 0x0101;
	framebuffer[((320/2) * 112) + ((14+48)/2)] = 0x0101;
	framebuffer[((320/2) * 112) + ((16+48)/2)] = 0x0101;
	framebuffer[((320/2) * 112) + ((18+48)/2)] = 0x0101;
	framebuffer[((320/2) * 112) + ((20+48)/2)] = 0x0101;
}

/*============================================================================= */

#ifdef SHOW_COMPATIBILITY_PROMPT
int TIC_Compatibility(void)
{
	screenCount++;

	// Read directly from the controller. Works better in this function. Don't know why!
	if ((Mars_ReadController(0) & BT_START)) {
		return ga_closeprompt;
	}

	return ga_nothing;
}

void START_Compatibility (void)
{
	for (int i = 0; i < 2; i++)
	{
		I_ClearFrameBuffer();
		UpdateBuffer();
	}

	screenCount = 0;

	I_SetPalette(dc_playpals);

	R_InitColormap();
}

void STOP_Compatibility (void)
{
	// Set to totally black
	R_FadePalette(dc_playpals, (PALETTE_SHIFT_CLASSIC_FADE_TO_BLACK + 20), dc_cshift_playpals);
}

void DRAW_Compatibility (void)
{
	const char *ares[6] = {
		"This emulator exhibits certain",
		"undesirable graphical glitches.",
		"While it should play fine otherwise,",
		"you may want to consider one of",
		"these alternatives for the best",
		"experience:"
	};

	const char *kega[6] = {
		"This emulator does not support",
		"certain features used by this game.",
		"While we do our best to support it,",
		ares[3],	// "you may want to consider one of",
		ares[4],	// "these alternatives for the best",
		ares[5]		// "experience:"
	};

	const char *gens[4] = {
		kega[0],	// "This emulator does not support",
		kega[1],	// "certain features used by this game.",
		"It is therefore not recommended. We",
		"suggest one of these alternatives:"
	};

	const char *ages[4] = {
		"This emulator has a large number of",
		"severe graphical and audio issues",
		"and is strongly discouraged. We",
		gens[3]		// "suggest one of these alternatives:"
	};

	const char *incompatible[3] = {
		"This emulator is not compatible with",
		"this game. We suggest one of these",
		"alternatives:"
	};

	const char *emulators[2] = {
		"* Jgenesis 0.10.0",
		"* PicoDrive 2.04",
	};

	const uint8_t compatibility_color[6] = { 0x70, 0xBC, 0x49, 0x36, 0x47, 0x23 };

	while (frame_sync == mars_vblank_count);
	frame_sync = mars_vblank_count;

	viewportbuffer = (pixel_t*)I_FrameBuffer();

	h40_sky = 1;	// Make sure the screen isn't shifted three pixels.

	if (screenCount < 4)
	{
		RemoveDistortionFilters();	// Normalize the line table to get rid of the three-pixel shift.

		I_SetThreadLocalVar(DOOMTLS_COLORMAP, dc_colormaps);

		DrawFillRect(0, 0, 320, 8, compatibility_color[legacy_emulator]);
		DrawFillRect(0, 216, 320, 8, compatibility_color[legacy_emulator]);
		DrawFillRect(0, 8, 8, 208, compatibility_color[legacy_emulator]);
		DrawFillRect(312, 8, 8, 208, compatibility_color[legacy_emulator]);

		V_DrawValueRight(&menuFont, 296, 16, legacy_emulator);

		V_DrawStringCenterWithColormap(&menuFont, 160, 24, "*** NOTICE ***", YELLOWTEXTCOLORMAP);

		switch (legacy_emulator)
		{
			case LEGACY_EMULATOR_ARES:
				for (int i=0; i < 6; i++) {
					V_DrawStringCenter(&menuFont, 160, 42+(i*12), ares[i]);
				}
				for (int i=0; i < 2; i++) {
					V_DrawStringLeft(&menuFont, 100, 132+(i*12), emulators[i]);
				}
				break;

			case LEGACY_EMULATOR_KEGA:
				for (int i=0; i < 6; i++) {
					V_DrawStringCenter(&menuFont, 160, 42+(i*12), kega[i]);
				}
				for (int i=0; i < 2; i++) {
					V_DrawStringLeft(&menuFont, 100, 132+(i*12), emulators[i]);
				}
				break;

			case LEGACY_EMULATOR_GENS:
				for (int i=0; i < 4; i++) {
					V_DrawStringCenter(&menuFont, 160, 42+(i*12), gens[i]);
				}
				for (int i=0; i < 2; i++) {
					V_DrawStringLeft(&menuFont, 100, 108+(i*12), emulators[i]);
				}
				break;

			case LEGACY_EMULATOR_AGES:
				for (int i=0; i < 4; i++) {
					V_DrawStringCenter(&menuFont, 160, 42+(i*12), ages[i]);
				}
				for (int i=0; i < 2; i++) {
					V_DrawStringLeft(&menuFont, 100, 108+(i*12), emulators[i]);
				}
				break;

			case LEGACY_EMULATOR_INCOMPATIBLE:
				for (int i=0; i < 3; i++) {
					V_DrawStringCenter(&menuFont, 160, 48+(i*12), incompatible[i]);
				}
				for (int i=0; i < 2; i++) {
					V_DrawStringLeft(&menuFont, 100, 96+(i*12), emulators[i]);
				}
				//break;
		}

		V_DrawStringCenterWithColormap(&menuFont, 160, 192, "PRESS START TO CONTINUE", YELLOWTEXTCOLORMAP);
	}

	uint16_t *lines = Mars_FrameBufferLines();
	short pixel_offset;
	if (screenCount & 0x20) {
		// Show "PRESS START" message.
		pixel_offset = (((320*192)+512)/2);
	}
	else {
		// Hide "PRESS START" message.
		pixel_offset = (((320*200)+512)/2);
	}

	for (int i=192; i < 200; i++) {
		lines[i] = pixel_offset;
		pixel_offset += (320/2);
	}
}
#endif

/*============================================================================= */

#ifdef SHOW_DISCLAIMER
int TIC_Disclaimer(void)
{
	if (++screenCount > 600)
		return 1;

	if (screenCount == 540)
	{
		// Set to totally black
		R_FadePalette(dc_playpals, (PALETTE_SHIFT_CLASSIC_FADE_TO_BLACK + 20), dc_cshift_playpals);
	}

	return 0;
}

void START_Disclaimer(void)
{
	int		i;

	for (i = 0; i < 2; i++)
	{
		I_ClearFrameBuffer();
		UpdateBuffer();
	}

	screenCount = 0;

	UpdateBuffer();

	const byte *dc_playpals = (uint8_t*)W_POINTLUMPNUM(W_GetNumForName("PLAYPALS"));
	I_SetPalette(dc_playpals);

	S_StartSong(gameinfo.gameoverMus, 0, cdtrack_gameover);

	R_InitColormap();
}

void STOP_Disclaimer (void)
{
}

const unsigned char key[] = { 0x78, 0x11, 0xB6, 0x2A, 0x48, 0x6A, 0x30, 0xA7 };
const int keyValue = 3043;
void parse_data(unsigned char *data, size_t dataLen)
{
	size_t startPos = 0;
	size_t i;
	for (i = 0; i < dataLen; i++, startPos++)
		data[i] = data[i] ^ key[startPos & 7];
}

/*
==================
=
= BufferedDrawSprite
=
= Cache and draw a game sprite to the 8 bit buffered screen
==================
*/

void BufferedDrawSprite (int sprite, int frame, int rotation, int top, int left, boolean flip)
{
	spritedef_t	*sprdef;
	spriteframe_t	*sprframe;
	VINT 		*sprlump;
	patch_t		*patch;
	byte		*pixels, *src;
	int			x, sprleft, sprtop, spryscale;
	fixed_t 	spriscale;
	int			lump;
	int			texturecolumn;
	int			light = HWLIGHT(255);
	int 		height = I_FrameBufferHeight();

	if ((unsigned)sprite >= NUMSPRITES)
		I_Error ("BufferedDrawSprite: invalid sprite number %i "
		,sprite);
	sprdef = &sprites[sprite];
	if ( (frame&FF_FRAMEMASK) >= sprdef->numframes )
		I_Error ("BufferedDrawSprite: invalid sprite frame %i : %i "
		,sprite,frame);
	sprframe = &spriteframes[sprdef->firstframe + (frame & FF_FRAMEMASK)];
	sprlump = &spritelumps[sprframe->lump];

	if (sprlump[rotation] != -1)
		lump = sprlump[rotation];
	else
		lump = sprlump[0];

	if (lump < 0)
	{
		lump = -(lump + 1);
		flip = true;
	}

	if (lump <= 0)
		return;

	patch = (patch_t *)W_POINTLUMPNUM(lump);
	pixels = R_CheckPixels(lump + 1);
	 	
/* */
/* coordinates are in a 160*112 screen (doubled pixels) */
/* */
	sprtop = top;
	sprleft = left;
	spryscale = 1;
	spriscale = FRACUNIT/spryscale;

	sprtop -= patch->topoffset;
	sprleft -= patch->leftoffset;

/* */
/* draw it by hand */
/* */
	for (x=0 ; x<patch->width ; x++)
	{
		int 	colx;
		byte	*columnptr;

		if (sprleft+x < 0)
			continue;
		if (sprleft+x >= 320)
			break;

		if (flip)
			texturecolumn = patch->width-1-x;
		else
			texturecolumn = x;
			
		columnptr = (byte *)patch + BIGSHORT(patch->columnofs[texturecolumn]);

/* */
/* draw a masked column */
/* */
		for ( ; *columnptr != 0xff ; columnptr += sizeof(column_t)) 
		{
			column_t *column = (column_t *)columnptr;
			int top    = column->topdelta + sprtop;
			int bottom = top + column->length - 1;
			byte *dataofsofs = columnptr + offsetof(column_t, dataofs);
			int dataofs = (dataofsofs[0] << 8) | dataofsofs[1];

			top *= spryscale;
			bottom *= spryscale;
			src = pixels + dataofs;

			if (top < 0) top = 0;
			if (bottom >= height) bottom = height - 1;
			if (top > bottom) continue;

			colx = sprleft + x;
//			colx += colx;

			I_DrawColumn(colx, top, bottom, light, 0, spriscale, src, 128);
//			I_DrawColumn(colx+1, top, bottom, light, 0, spriscale, src, 128);
		}
	}
}

void DRAW_Disclaimer (void)
{
	while (frame_sync == mars_vblank_count);
	frame_sync = mars_vblank_count;

	if (screenCount < 4) {
		unsigned char text1[] = { 0x2C, 0x59, 0xFF, 0x79, 0x68, 0x2D, 0x71, 0xEA, 0x3D, 0x31, 0xE5, 0x62, 0x07, 0x3F, 0x7C, 0xE3, 0x78};
		unsigned char text2[] = { 0x36, 0x5E, 0xE2, 0x0A, 0x0A, 0x2F, 0x10, 0xF4, 0x37, 0x5D, 0xF2, 0x2A};
		unsigned char text3[] = { 0x10, 0x65, 0xC2, 0x5A, 0x3B, 0x50, 0x1F, 0x88, 0x0B, 0x62, 0xD8, 0x5E, 0x29, 0x03, 0x5C, 0xD4, 0x56, 0x62, 0xC4, 0x48, 0x7A, 0x44, 0x5F, 0xD5, 0x1F, 0x3E, 0xC5, 0x58, 0x2A, 0x59, 0x02, 0xDF, 0x78 };
		const VINT stext1 = 16;
		const VINT stext2 = 11;
		const VINT stext3 = 32;

		int sum = 0;
		for (int i = 0; i < stext1; i++)
			sum += text1[i] / 2;
		for (int i = 0; i < stext2; i++)
			sum += text2[i] / 2;
		for (int i = 0; i < stext3; i++)
			sum += text3[i] / 2;

		if (sum != keyValue)
			I_Error("");

		parse_data(text1, stext1+1);
		parse_data(text2, stext2+1);
		parse_data(text3, stext3+1);

		DrawFillRect(0, 0, 320, viewportHeight, COLOR_BLACK);

		V_DrawStringCenter(&creditFont, 160, 64+32, (const char*)text1);
		V_DrawStringCenter(&creditFont, 160, 88+32, (const char*)text2);

		V_DrawStringCenter(&menuFont, 160, 128+32, (const char*)text3);
	}

	if (screenCount < 500)
	{
		DrawFillRect((320>>1)-(56>>1), 24, 56, 64, COLOR_BLACK);

		if (screenCount < 480)
		{
			viewportbuffer = (pixel_t*)I_FrameBuffer();
			I_SetThreadLocalVar(DOOMTLS_COLORMAP, dc_colormaps);
			BufferedDrawSprite(SPR_PLAY, 1 + ((screenCount>>4) & 1), 0, 80, 160, false);
		}
		else
		{
			DrawJagobjLump(W_GetNumForName("ZOOM"), 136, 80-56, NULL, NULL);
		}
	}
	else
	{
		VINT Xpos = screenCount - 500;

		DrawFillRect((320>>1)-(80>>1) + (Xpos<<3), 20, 80, 64, COLOR_BLACK);

		viewportbuffer = (pixel_t*)I_FrameBuffer();
		I_SetThreadLocalVar(DOOMTLS_COLORMAP, dc_colormaps);
		BufferedDrawSprite(SPR_PLAY, 11 + ((screenCount>>2) & 3), 2, 80, 160 + (Xpos<<3), true);
	}

	Mars_ClearCache();
}
#endif

/*============================================================================= */

void START_Title(void)
{
#ifdef MARS
	int		i;

	for (i = 0; i < 2; i++)
	{
		I_ClearFrameBuffer();
		UpdateBuffer();
	}
#else
	DoubleBufferSetup();
#endif

	/*
	if (gamemapinfo.data)
		Z_Free(gamemapinfo.data);
	gamemapinfo.data = NULL;
	D_memset(&gamemapinfo, 0, sizeof(gamemapinfo));

	char buf[512];
	G_FindMapinfo(G_LumpNumForMapNum(TITLE_MAP_NUMBER), &gamemapinfo, buf);

	R_SetupMDSky(gamemapinfo.sky, 1);
	*/

	R_SetupMDSky("sky1", 1); // TODO: //DLG: Load MAP30 gamemapinfo to get the sky name.

	titlepic = W_CacheLumpNum(gameinfo.titlePage, PU_STATIC);
	titlepic_a = W_CacheLumpNum(gameinfo.titlePageA, PU_STATIC);
	titlepic_b = W_CacheLumpNum(gameinfo.titlePageB, PU_STATIC);

	ticon = 0;

	I_InitMenuFire(titlepic, titlepic_a, titlepic_b);

	S_StartSong(gameinfo.titleMus, 0, cdtrack_title);
}

void STOP_Title (void)
{
	if (titlepic_b != NULL)
	{
		Z_Free (titlepic_b);
		titlepic_b = NULL;
	}

	if (titlepic_a != NULL)
	{
		Z_Free (titlepic_a);
		titlepic_a = NULL;
	}

	if (titlepic != NULL)
	{
		Z_Free (titlepic);
		titlepic = NULL;
	}

	I_StopMenuFire();
}

void DRAW_Title (void)
{
	I_DrawMenuFire();
}

/*============================================================================= */

int RunInputDemo (char *demoname)
{
	unsigned char *demo;
	int	exit;
	int lump;

	lump = W_CheckNumForName(demoname);
	if (lump == -1)
		return ga_exitdemo;

	// avoid zone memory fragmentation which is due to happen
	// if the demo lump cache is tucked after the level zone.
	// this will cause shrinking of the zone area available
	// for the level data after each demo playback and eventual
	// Z_Malloc failure
	ClearCopper();
	Z_FreeTags(mainzone);

	demo = W_CacheLumpNum(lump, PU_STATIC);
	exit = G_PlayInputDemoPtr (demo);
	Z_Free(demo);

	return exit;
}

#ifdef PLAY_POS_DEMO
int RunPositionDemo (char *demoname)
{
	unsigned *demo;
	int	exit;
	int lump;

	lump = W_CheckNumForName(demoname);
	if (lump == -1)
		return ga_exitdemo;

	// avoid zone memory fragmentation which is due to happen
	// if the demo lump cache is tucked after the level zone.
	// this will cause shrinking of the zone area available
	// for the level data after each demo playback and eventual
	// Z_Malloc failure
	ClearCopper();
	Z_FreeTags(mainzone);

	demo = W_CacheLumpNum(lump, PU_STATIC);
	exit = G_PlayPositionDemoPtr ((unsigned char*)demo);
	Z_Free(demo);

	return exit;
}
#endif

/*============================================================================ */


 
/* 
============= 
= 
= D_DoomMain 
= 
============= 
*/ 

VINT			startmap = 1;
gametype_t	starttype = gt_single;
VINT			startsave = -1;
boolean 	startsplitscreen = 0;

boolean clearedGame = false;

void D_DoomMain (void) 
{
	effects_flags = EFFECTS_COPPER_ENABLED;	// This allows for testing of dropped interrupts.

D_printf ("C_Init\n");
	C_Init ();		/* set up object list / etc	  */
D_printf ("Z_Init\n");
	Z_Init (); 
D_printf ("W_Init\n");
	W_Init ();
D_printf ("I_Init\n");
	I_Init (); 
D_printf ("R_Init\n");
	R_Init ();
D_printf ("P_Init\n");
	P_Init ();
D_printf ("S_Init\n");
	S_Init ();
D_printf("ST_Init\n");
	ST_Init ();
D_printf("O_Init\n");
	O_Init ();
D_printf("G_Init\n");
	G_Init();
	

/*========================================================================== */

D_printf ("DM_Main\n");

#ifdef PLAY_INPUT_DEMO
	while(1) {
		RunInputDemo("DEMO1");
	}
#endif
#ifdef PLAY_POS_DEMO
	while(1) {
		RunPositionDemo("DEMO9");
	}
#endif

#ifdef REC_INPUT_DEMO
	G_RecordInputDemo();	// set startmap and startskill
#endif
#ifdef REC_POS_DEMO
	G_RecordPositionDemo();
#endif

	if (legacy_emulator == LEGACY_EMULATOR_GENS && mars_hblank_count_peak == 224) {
		// This is probably AGES.
		legacy_emulator = LEGACY_EMULATOR_AGES;
	}
	if (I_GetFRTCounter() <= 256) {
		// Likely an old version of PicoDrive with incorrect WDT handling.
		// This will also fail for RetroDrive.
		legacy_emulator = LEGACY_EMULATOR_INCOMPATIBLE;
	}

#ifdef SHOW_COMPATIBILITY_PROMPT
	if (legacy_emulator) {
		SetCompatibility();
		MiniLoop (START_Compatibility, STOP_Compatibility, TIC_Compatibility, DRAW_Compatibility, UpdateBuffer);
	}
#endif

#ifdef SHOW_DISCLAIMER
	SetDisclaimer();
	MiniLoop (START_Disclaimer, STOP_Disclaimer, TIC_Disclaimer, DRAW_Disclaimer, UpdateBuffer);
#else
	cheats_enabled = CHEAT_METRICS | CHEAT_GAMEMODE_SELECT;	// Cheats already active for development builds.
#endif

#ifdef SHOW_FPS
	debugmode = DEBUGMODE_FPSCOUNT;
#endif

	startmap = 1;
	starttype = gt_single;
	consoleplayer = 0;

	char demo_name[6] = { 'D', 'E', 'M', 'O', '0', '\0' };
	int exit = ga_titleexpired;

	if (!gameinfo.noAttractDemo) {
		do {
			// Title intro
			G_InitNew (TITLE_MAP_NUMBER, gt_single, false);
			SetTitleIntro();
			MiniLoop (START_Title, STOP_Title, TIC_Abortable, DRAW_Title, UpdateBuffer);
#ifdef CPUDEBUG
			cpu_pulse_timeout = 300;	// 5 seconds
#endif

			// Title with menu
			M_Start();
			SetTitleScreen();
			exit = MiniLoop (P_Start, P_Stop, P_Ticker, P_Drawer, P_Update);
			//M_Stop();

			switch (exit) {
				case ga_startnew:
					// Level selection screen
					SetLevelSelect();
					MiniLoop (START_Test, STOP_Test, TIC_Test, DRAW_Test, UpdateBuffer);
					exit = MiniLoop (START_LevelSelect, STOP_LevelSelect, TIC_LevelSelect, DRAW_LevelSelect, UpdateBuffer);

					// Start a new game
					G_InitNew(startmap, starttype, startsplitscreen);
					if (startmap >= SSTAGE_START && startmap <= SSTAGE_END) {
						SetLevel(LevelType_SpecialStage);
					}
					else {
						SetLevel(LevelType_Normal);
					}
					G_RunGame();
					break;
				case ga_titleexpired:
					// Run a demo
					{
						demo_name[4]++;	// Point to the next demo.
						
						int lump = W_CheckNumForName(demo_name);
						if (lump == -1) {
							demo_name[4] = '1';	// Assume we at least have a DEMO1 lump.
						}
						
						SetDemoMode(DemoMode_Playback);
						exit = RunInputDemo(demo_name);
						SetDemoMode(DemoMode_None);
					}
					break;
				case ga_showcredits:
					// Show the credits sequence
					SetCredits();
					MiniLoop(F_Start, F_Stop, F_Ticker, F_Drawer, I_Update);
			}
		} while (1);
	}

	/*
	if (consoleplayer == 0)
	{
		if (starttype != gt_single && !startsplitscreen)
		{
			I_NetSetup();
		}
	}

	if (startsave != -1)
		G_LoadGame(startsave);
	else
		G_InitNew(startmap, starttype, startsplitscreen);

	G_RunGame ();
	*/
}
