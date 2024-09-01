/* D_main.c  */
 
#include "doomdef.h"

#ifdef PLAY_POS_DEMO
#include "v_font.h"
#endif

#ifdef MARS
#include "marshw.h"
#endif

#include <string.h>

boolean		splitscreen = false;
VINT		controltype = 0;		/* determine settings for BT_* */
VINT		alwaysrun = 0;

int			gamevbls;		/* may not really be vbls in multiplayer */
int			vblsinframe;		/* range from ticrate to ticrate*2 */

VINT		ticsperframe = MINTICSPERFRAME;

int			maxlevel;			/* highest level selectable in menu (1-25) */
jagobj_t	*backgroundpic;

boolean canwipe = false;

int 		ticstart;

#ifdef PLAY_POS_DEMO
	int			realtic;
	fixed_t prev_rec_values[4];
#else 
#ifdef REC_POS_DEMO
	fixed_t prev_rec_values[4];
#endif
#endif

unsigned configuration[NUMCONTROLOPTIONS][3] =
{
	{BT_ATTACK, BT_USE, BT_SPEED},
	{BT_SPEED, BT_USE, BT_ATTACK},
	{BT_ATTACK, BT_SPEED, BT_USE},
	{BT_ATTACK, BT_USE, BT_SPEED},
	{BT_USE, BT_SPEED, BT_ATTACK},
	{BT_USE, BT_ATTACK, BT_SPEED}
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
int	rndindex = 0;
int prndindex = 0;

int P_Random (void)
{
	prndindex = (prndindex+1)&0xff;
	return rndtable[prndindex];
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

int		ticrate = 4;
int		ticsinframe;	/* how many tics since last drawer */
int		ticon;
int		frameon;
int		ticbuttons[MAXPLAYERS];
int		oldticbuttons[MAXPLAYERS];
int		ticmousex[MAXPLAYERS], ticmousey[MAXPLAYERS];
int		ticrealbuttons, oldticrealbuttons;
boolean	mousepresent;

extern	int	lasttics;

mobj_t	emptymobj;
 
/*
===============
=
= MiniLoop
=
===============
*/
__attribute((noinline))
static void D_LoadMDSky(void)
{
	// Retrieve lumps for drawing the sky on the MD.

	#ifdef MDSKY
	uint8_t *sky_name_ptr;
	uint8_t *sky_pal_ptr;
	uint8_t *sky_pat_ptr;
	int lump;

	char lumpname[9];

	D_strncpy(lumpname, gamemapinfo.sky, 5);
	strcat(lumpname, "NAM");
	lump = W_CheckNumForName(lumpname);
	if (lump != -1) {
		sky_name_ptr = (uint8_t *)W_POINTLUMPNUM(lump);
	}
	else {
		return;
	}

	D_strncpy(lumpname, gamemapinfo.sky, 5);
	strcat(lumpname, "PAL");
	lump = W_CheckNumForName(lumpname);
	if (lump != -1) {
		sky_pal_ptr = (uint8_t *)W_POINTLUMPNUM(lump);
	}
	else {
		return;
	}

	D_strncpy(lumpname, gamemapinfo.sky, 5);
	strcat(lumpname, "TIL");
	lump = W_CheckNumForName(lumpname);
	if (lump != -1) {
		sky_pat_ptr = (uint8_t *)W_POINTLUMPNUM(lump);
	}
	else {
		return;
	}

	Mars_LoadMDSky(sky_name_ptr, sky_pal_ptr, sky_pat_ptr);
	#endif
}

int last_frt_count = 0;
int total_frt_count = 0;

int MiniLoop ( void (*start)(void),  void (*stop)(void)
		,  int (*ticker)(void), void (*drawer)(void)
		,  void (*update)(void) )
{
	int		i;
	int		exit;
	int		buttons;
	int		mx, my;
	boolean wipe = canwipe;
	boolean firstdraw = true;

/* */
/* setup (cache graphics, etc) */
/* */
	start ();
	exit = 0;
	
	ticon = 0;
	frameon = 0;
	
	gametic = 0;
	gametic30 = 0;
	prevgametic = 0;
	leveltime = 0;

	gameaction = 0;
	gamevbls = 0;
	vblsinframe = 0;
	lasttics = 0;
	last_frt_count = 0;
	total_frt_count = 0;

	ticbuttons[0] = ticbuttons[1] = oldticbuttons[0] = oldticbuttons[1] = 0;
	ticmousex[0] = ticmousex[1] = ticmousey[0] = ticmousey[1] = 0;

	#ifdef MDSKY
	if (wipe)
	{
		D_LoadMDSky();
	}
	#endif

	do
	{
		ticstart = I_GetFRTCounter();

/* */
/* adaptive timing based on previous frame */
/* */
	vblsinframe = TICVBLS;

/* */
/* get buttons for next tic */
/* */
	oldticbuttons[0] = ticbuttons[0];
	oldticbuttons[1] = ticbuttons[1];
	oldticrealbuttons = ticrealbuttons;

	buttons = I_ReadControls();
	buttons |= I_ReadMouse(&mx, &my);
	if (demoplayback)
	{
		ticmousex[consoleplayer] = 0;
		ticmousey[consoleplayer] = 0;
	}
	else
	{
		ticmousex[consoleplayer] = mx;
		ticmousey[consoleplayer] = my;
	}

	ticbuttons[consoleplayer] = buttons;
	ticrealbuttons = buttons;

	if (demoplayback)
	{
#ifndef MARS
		if (buttons & (BT_ATTACK|BT_SPEED|BT_USE) )
		{
			exit = ga_exitdemo;
			break;
		}
#endif

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

#ifndef PLAY_POS_DEMO
		if (gamemapinfo.mapNumber == TITLE_MAP_NUMBER) {
			// Rotate on the title screen.
			ticbuttons[consoleplayer] = buttons = 0;
			players[0].mo->angle += TITLE_ANGLE_INC;
		}
		else {
			// This is for reading conventional input-based demos.
			ticbuttons[consoleplayer] = buttons = *((long *)demobuffer);
			demobuffer += 4;
		}
		#endif
	}

	#ifdef PLAY_POS_DEMO
	if (demoplayback) {
		players[0].mo->momx = 0;
		players[0].mo->momy = 0;
		players[0].mo->momz = 0;
	}
	#endif

	gamevbls += vblsinframe;

	if (demorecording) {
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
		*((long *)demo_p) = buttons;
		demo_p += 4;
#endif
	}
		
	if ((demorecording || demoplayback) && (buttons & BT_PAUSE) )
		exit = ga_completed;

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
		
	int frt_count = I_GetFRTCounter();

	if (last_frt_count == 0)
		last_frt_count = frt_count;

	int accum_time = 0;
	// Frame skipping based on FRT count
	total_frt_count += Mars_FRTCounter2Msec(frt_count - last_frt_count);
	const int frametime = I_IsPAL() ? 1000/25 : 1000/30;

	while (total_frt_count > frametime)
	{
		accum_time++;
		total_frt_count -= frametime;
	}

	last_frt_count = frt_count;

	if (leveltime < TICRATE / 4) // Don't include map loading times into frameskip calculation
	{
		accum_time = 1;
		total_frt_count = 0;
	}

	int tempticbuttons[2];
	int tempticmousex[2];
	int tempticmousey[2];
	int tempticrealbuttons = ticrealbuttons;
	tempticbuttons[0] = ticbuttons[0];
	tempticbuttons[1] = ticbuttons[1];
	for (int i = 0; i < accum_time; i++)
	{
		ticbuttons[0] = tempticbuttons[0];
		ticbuttons[1] = tempticbuttons[1];
		ticmousex[0] = tempticmousex[0];
		ticmousex[1] = tempticmousex[1];
		ticmousey[0] = tempticmousey[0];
		ticmousey[1] = tempticmousey[1];
		ticrealbuttons = tempticrealbuttons;

		if (splitscreen && !demoplayback)
			ticbuttons[consoleplayer ^ 1] = I_ReadControls2();
		else if (netgame)	/* may also change vblsinframe */
			ticbuttons[consoleplayer ^ 1]
				= NetToLocal(I_NetTransfer(LocalToNet(ticbuttons[consoleplayer])));

		if (ticon & 1)
			gametic++;
			
		ticon++;
		exit = ticker();
	}

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

		if (!exit && wipe)
		{
			wipe_EndScreen();
			wipe = false;
		}

		prevgametic = gametic;

#if 0
while (!I_RefreshCompleted ())
;	/* DEBUG */
#endif

#ifdef JAGUAR
		while ( DSPRead(&dspfinished) != 0xdef6 )
		;
#endif
	} while (!exit);

	stop ();
	S_Clear ();
	
	for (i = 0; i < MAXPLAYERS; i++)
		players[i].mo = &emptymobj;	/* for net consistency checks */

	return exit;
} 
 


/*=============================================================================  */

void ClearEEProm (void);
void DrawSinglePlaque (jagobj_t *pl);

jagobj_t	*titlepic;

int TIC_Abortable (void)
{
#ifdef JAGUAR
	jagobj_t	*pl;
#endif
	int buttons = ticbuttons[0];
	int oldbuttons = oldticbuttons[0];

	if (titlepic == NULL)
		return 1;
	if (ticon < TICVBLS)
		return 0;
	if (ticon >= gameinfo.titleTime)
		return 1;		/* go on to next demo */

#ifdef JAGUAR	
	if (ticbuttons[0] == (BT_OPTION|BT_STAR|BT_HASH) )
	{	/* reset eeprom memory */
		void Jag68k_main (void);

		ClearEEProm ();
		pl = W_CacheLumpName ("defaults", PU_STATIC);	
		DrawSinglePlaque (pl);
		Z_Free (pl);
		S_Clear ();
		ticcount = 0;
		while ( (junk = ticcount) < 240)
		;
		Jag68k_main ();
	}
#endif

#ifdef MARS
	if ( (buttons & BT_A) && !(oldbuttons & BT_A) )
		return ga_exitdemo;
	if ( (buttons & BT_B) && !(oldbuttons & BT_B) )
		return ga_exitdemo;
	if ( (buttons & BT_C) && !(oldbuttons & BT_C) )
		return ga_exitdemo;
	if ( (buttons & BT_START) && !(oldbuttons & BT_START) )
		return ga_exitdemo;
#else
	if ( (buttons & BT_ATTACK) && !(oldbuttons & BT_ATTACK) )
		return ga_exitdemo;
	if ( (buttons & BT_USE) && !(oldbuttons & BT_USE) )
		return ga_exitdemo;
	if ( (buttons & BT_OPTION) && !(oldbuttons & BT_OPTION) )
		return ga_exitdemo;
	if ( (buttons & BT_START) && !(oldbuttons & BT_START) )
		return ga_exitdemo;
#endif

	return 0;
}


/*============================================================================= */

void START_Title(void)
{
	int l;

#ifdef MARS
	int		i;

	for (i = 0; i < 2; i++)
	{
		I_ClearFrameBuffer();
		UpdateBuffer();
	}
#else
	backgroundpic = W_POINTLUMPNUM(W_GetNumForName("M_TITLE"));
	DoubleBufferSetup();
#endif

	l = gameinfo.titlePage;
	titlepic = l != -1 ? W_CacheLumpNum(l, PU_STATIC) : NULL;

#ifdef MARS
	I_InitMenuFire(titlepic);
#else
	S_StartSong(gameinfo.titleMus, 0, cdtrack_title);
#endif
}

void STOP_Title (void)
{
	if (titlepic != NULL)
		Z_Free (titlepic);
#ifdef MARS
	I_StopMenuFire();
#else
	S_StopSong();
#endif
}

void DRAW_Title (void)
{
#ifdef MARS
	I_DrawMenuFire();
#endif
}

/*============================================================================= */

#ifdef MARS
static char* credits;
static short creditspage;
#endif

static void START_Credits (void)
{
#ifdef MARS
	credits = NULL;
	titlepic = NULL;
	creditspage = 1;
	if (gameinfo.creditsPage < 0)
		return;
	credits = W_CacheLumpNum(gameinfo.creditsPage, PU_STATIC);
#else
	backgroundpic = W_POINTLUMPNUM(W_GetNumForName("M_TITLE"));
	titlepic = W_CacheLumpName("credits", PU_STATIC);
#endif
	DoubleBufferSetup();
}

void STOP_Credits (void)
{
#ifdef MARS
	if (credits)
		Z_Free(credits);
#endif
	if (titlepic)
		Z_Free(titlepic);
}

static int TIC_Credits (void)
{
	int buttons = ticbuttons[0];
	int oldbuttons = oldticbuttons[0];

	if (gameinfo.creditsPage < 0)
		return ga_exitdemo;
	if (ticon >= gameinfo.creditsTime)
		return 1;		/* go on to next demo */

#ifdef MARS
	if ( (buttons & BT_A) && !(oldbuttons & BT_A) )
		return ga_exitdemo;
	if ( (buttons & BT_B) && !(oldbuttons & BT_B) )
		return ga_exitdemo;
	if ( (buttons & BT_C) && !(oldbuttons & BT_C) )
		return ga_exitdemo;
	if ( (buttons & BT_START) && !(oldbuttons & BT_START) )
		return ga_exitdemo;
#else
	if ( (buttons & BT_ATTACK) && !(oldbuttons & BT_ATTACK) )
		return ga_exitdemo;
	if ( (buttons & BT_SPEED) && !(oldbuttons & BT_SPEED) )
		return ga_exitdemo;
	if ( (buttons & BT_USE) && !(oldbuttons & BT_USE) )
		return ga_exitdemo;
	if ( (buttons & BT_START) && !(oldbuttons & BT_START) )
		return ga_exitdemo;
#endif

	if (ticon * 2 >= gameinfo.creditsTime)
	{
		if (creditspage != 2)
		{
			char name[9];

			D_memcpy(name, W_GetNameForNum(gameinfo.creditsPage), 8);
			name[7]+= creditspage;
			name[8] = '\0';

			int l = W_CheckNumForName(name);
			if (l >= 0)
			{
				Z_Free(credits);
				credits = W_CacheLumpNum(l, PU_STATIC);
			}
			creditspage = 2;

			DoubleBufferSetup();
		}
	}

	return 0;
}

static void DRAW_RightString(int x, int y, const char* str)
{
	int len = I_Print8Len(str);
	I_Print8(x - len * 8, y, str);
}

static void DRAW_CenterString(int y, const char* str)
{
	int len = I_Print8Len(str);
	I_Print8((320 - len * 8) / 2, y, str);
}

static void DRAW_LineCmds(char *lines)
{
	int y;
	static const char* dots = "...";
	int dots_len = mystrlen(dots);
	int x_right = (320 - dots_len*8) / 2 - 2;
	int x_left = (320 + dots_len*8) / 2;
	char* p = lines;

	y = 0;
	while (1) {
		char *end = D_strchr(p, '\n');
		char* str = p, *nextl;
		char cmd = str[0];
		char inc = str[1];
		char bak = 0;

		if (!cmd || !inc)
			break;

		str += 3;
		if (end)
		{
			nextl = end + 1;
			if (*(end - 1) == '\r')
				--end;
			bak = *end;
			*end = '\0';
		}
		else
		{
			nextl = NULL;
		}

		switch (cmd) {
		case 'c':
			DRAW_CenterString(y, str);
			break;
		case 'r':
			DRAW_RightString(x_right, y, str);
			break;
		case 'l':
			I_Print8(x_left, y, str);
			break;
		}

		if (inc == 'i')
		{
			y++;
		}

		if (nextl)
			*end = bak;
		else
			break;
		p = nextl;
	}
}

static void DRAW_Credits(void)
{
#ifdef MARS
	DRAW_LineCmds(credits);
#else
	DrawJagobj(titlepic, 0, 0);
#endif
}

/*============================================================================ */
void RunMenu (void);
void RunCredits(void);

void RunTitle (void)
{
	startmap = 1;
	starttype = gt_single;
	consoleplayer = 0;
	canwipe = true;

	MiniLoop (START_Title, STOP_Title, TIC_Abortable, DRAW_Title, UpdateBuffer);

	RunMenu();
}

void RunCredits (void)
{
	int		l;
	int		exit;
	
	l = gameinfo.creditsPage;
	if (l > 0)
		exit = MiniLoop(START_Credits, STOP_Credits, TIC_Credits, DRAW_Credits, UpdateBuffer);
	else
		exit = ga_exitdemo;

	if (exit == ga_exitdemo)
		RunMenu ();
}

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
	Z_FreeTags(mainzone);

	demo = W_CacheLumpNum(lump, PU_STATIC);
	exit = G_PlayInputDemoPtr (demo);
	Z_Free(demo);

#ifndef MARS
	if (exit == ga_exitdemo)
		RunMenu ();
#endif
	return exit;
}

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
	Z_FreeTags(mainzone);

	demo = W_CacheLumpNum(lump, PU_STATIC);
	exit = G_PlayPositionDemoPtr ((unsigned char*)demo);
	Z_Free(demo);

#ifndef MARS
	if (exit == ga_exitdemo)
		RunMenu ();
#endif
	return exit;
}


void RunMenu (void)
{
#ifdef MARS
	int exit = ga_exitdemo;

	M_Start();
	if (!gameinfo.noAttractDemo) {
		do {
			int i;
			char demo[9];

			for (i = 1; i < 10; i++)
			{
				int lump;

				D_snprintf(demo, sizeof(demo), "DEMO%1d", i);
				lump = W_CheckNumForName(demo);
				if (lump == -1)
					break;

				exit = RunInputDemo(demo);
				if (exit == ga_exitdemo)
					break;
			}
		} while (exit != ga_exitdemo);
	}
	M_Stop();
#else
reselect:
	MiniLoop(M_Start, M_Stop, M_Ticker, M_Drawer, NULL);
#endif

	if (consoleplayer == 0)
	{
		if (starttype != gt_single && !startsplitscreen)
		{
			I_NetSetup();
#ifndef MARS
			if (starttype == gt_single)
				goto reselect;		/* aborted net startup */
#endif
		}
	}

	if (startsave != -1)
		G_LoadGame(startsave);
	else
		G_InitNew(startmap, starttype, startsplitscreen);

	G_RunGame ();
}

/*============================================================================ */


 
/* 
============= 
= 
= D_DoomMain 
= 
============= 
*/ 

int			startmap = 1;
gametype_t	starttype = gt_single;
int			startsave = -1;
boolean 	startsplitscreen = 0;

void D_DoomMain (void) 
{    
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

/*	MiniLoop (F_Start, F_Stop, F_Ticker, F_Drawer, UpdateBuffer); */

/*G_InitNew (startmap, gt_deathmatch, false); */
/*G_RunGame (); */

#ifdef NeXT
	if (M_CheckParm ("-dm") )
	{
		I_NetSetup ();
		G_InitNew (startmap, gt_deathmatch, false);
	}
	else if (M_CheckParm ("-dial") || M_CheckParm ("-answer") )
	{
		I_NetSetup ();
		G_InitNew (startmap, gt_coop, false);
	}
	else
		G_InitNew (startmap, gt_single, false);
	G_RunGame ();
#endif

#ifdef MARS
	while (1)
	{
		RunTitle();
		RunMenu();
	}
#else
	while (1)
	{
		RunTitle();
		RunInputDemo("DEMO1");
		RunCredits ();
		RunInputDemo("DEMO2");
	}
#endif
}
 
