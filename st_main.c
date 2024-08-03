/* st_main.c -- status bar */

#include "doomdef.h"
#include "st_main.h"
#include "v_font.h"
#include <stdio.h>

stbar_t	*stbar;
int stbar_tics;

static short stbarframe;
static short   stbar_y;

static short score, time, rings, rrings;
static short snums; // Numbers
static short timecolon; // : for TIME
static short face, livex;

static short ltzz_blue_lump, chev_blue_lump, lt_blue_lump;
static short ltzz_red_lump, chev_red_lump, lt_red_lump;

#ifndef MARS
byte		*sbartop;
#endif

#ifndef MARS
//#define ST_EraseBlock EraseBlock
#endif

//static void ST_EraseBlock(int x, int y, int width, int height);

/*
====================
=
= ST_Init
=
= Locate and load all needed graphics
====================
*/

void ST_Init (void)
{
	V_FontInit();

	stbar = Z_Malloc(sizeof(*stbar)*MAXPLAYERS, PU_STATIC);

	score = W_CheckNumForName("STTSCORE");
	time = W_CheckNumForName("STTTIME");
	rings = W_CheckNumForName("STTRINGS");
	rrings = W_CheckNumForName("STTRRING");
	snums = W_CheckNumForName("STTNUM0");
	timecolon = W_CheckNumForName("STTCOLON");
	face = W_CheckNumForName("STFACE");
	livex = W_CheckNumForName("STLIVEX");

	ltzz_blue_lump = W_CheckNumForName("LTZZTEXT");
	chev_blue_lump = W_CheckNumForName("CHEVBLUE");
	lt_blue_lump = W_CheckNumForName("LTACTBLU");
	ltzz_red_lump = W_CheckNumForName("LTZZWARN");
	chev_red_lump = W_CheckNumForName("CHEVRED");
	lt_red_lump	= W_CheckNumForName("LTACTRED");
}

void ST_ForceDraw(void)
{
	int p;

	stbarframe = 0;
	for (p = 0; p < MAXPLAYERS; p++)
	{
		if (playeringame[p])
			stbar[p].forcedraw = true;
	}
	ST_Ticker();
}

/*================================================== */
/* */
/*  Init this stuff EVERY LEVEL */
/* */
/*================================================== */
void ST_InitEveryLevel(void)
{
	int		i, p;
	int		numplayers;

	stbarframe = 0;

	numplayers = 0;
	for (i = 0; i < MAXPLAYERS; i++)
		numplayers += playeringame[i] ? 1 : 0;

	D_memset(stbar, 0, sizeof(stbar[0])*MAXPLAYERS);

	for (p = 0; p < numplayers; p++)
	{
		stbar_t* sb = &stbar[p];

		/* force everything to be updated on next ST_Update */
		sb->forcedraw = true;

		sb->score = 0;
	}
	stbar_tics = 0;
}

/*
====================
=
= ST_Ticker
=
====================
*/

static void ST_Ticker_(stbar_t* sb)
{
	player_t* p;
	int			pnum = sb - &stbar[0];

#ifdef MARS
	// double-buffered renderer on MARS
	if ((stbarframe & 1) == 1)
		return;
#endif

	p = &players[pnum];

	sb->rings = p->mo->health - 1;
	sb->score = p->score;
	sb->lives = p->lives;

	sb->forcedraw = false;
}

void ST_Ticker(void)
{
	int	p = splitscreen ? 0 : consoleplayer;
	int e = splitscreen ? MAXPLAYERS : consoleplayer + 1;

	while (p < e)
	{
		if (playeringame[p])
			ST_Ticker_(&stbar[p]);
		p++;
	}

	stbarframe++;
	stbar_tics++;
}

/*
====================
=
= ST_DrawTitleCard
=
====================
*/

static void ST_DrawTitleCard()
{
	short ltzz_lump;
	short chev_lump;
	short lt_lump;

	if (gamemapinfo.act == 3) {
		ltzz_lump = ltzz_red_lump;
		chev_lump = chev_red_lump;
		lt_lump = lt_red_lump;
	}
	else {
		ltzz_lump = ltzz_blue_lump;
		chev_lump = chev_blue_lump;
		lt_lump = lt_blue_lump;
	}

	if (leveltime < 30)
		DrawFillRect(0, 22, 320, 180, COLOR_BLACK);

	if (stbar_tics < 16) {
		// Title card moving into the frame.

		DrawScrollingBanner(ltzz_lump, (stbar_tics-16) << 4, stbar_tics << 1);

		DrawScrollingChevrons(chev_lump, 16 + ((stbar_tics-16) << 4), -stbar_tics << 1);

		if (gamemapinfo.act >= 1 && gamemapinfo.act <= 3) {
			DrawJagobjLump(lt_lump, 160+70-24 + ((16 - stbar_tics) << 5), 100 - ((16 - stbar_tics) << 5), NULL, NULL);
			V_DrawValueLeft(&titleNumberFont, 160+70 + ((16 - stbar_tics) << 5), 124-4, gamemapinfo.act);
		}
		V_DrawStringRight(&titleFont, 160+68 - ((16 - stbar_tics) << 5), 100, gamemapinfo.name);
		V_DrawStringLeft(&titleFont, 160 + ((16 - stbar_tics) << 5), 124, "Zone");
	}
	else if (stbar_tics < 80) {
		// Title card at rest in the frame.

		DrawScrollingBanner(ltzz_lump, 0, stbar_tics << 1);

		DrawScrollingChevrons(chev_lump, 16, -stbar_tics << 1);

		if (gamemapinfo.act >= 1 && gamemapinfo.act <= 3) {
			DrawJagobjLump(lt_lump, 160+68-24, 100, NULL, NULL);

			V_DrawValueLeft(&titleNumberFont, 160+68, 124-4, gamemapinfo.act);
		}
		V_DrawStringRight(&titleFont, 160+68, 100, gamemapinfo.name);
		V_DrawStringLeft(&titleFont, 160, 124, "Zone");
	}
	else {
		// Title card moving out of the frame.
		DrawScrollingBanner(ltzz_lump, (80-stbar_tics) << 4, stbar_tics << 1);

		DrawScrollingChevrons(chev_lump, 16 + ((80-stbar_tics) << 4), -stbar_tics << 1);

		if (gamemapinfo.act >= 1 && gamemapinfo.act <= 3) {
			jagobj_t *lt_obj = (jagobj_t*)W_POINTLUMPNUM(lt_lump);
			VINT lt_y = 100 + ((stbar_tics - 80) << 5);
			VINT lt_height = lt_y + lt_obj->height > (180+22)
					? lt_obj->height - ((lt_y + lt_obj->height) - (180+22))
					: lt_obj->height;
			DrawJagobj2(lt_obj, 160+68-24 - ((stbar_tics - 80) << 5), lt_y, 0, 0,
					lt_obj->width, lt_height, I_OverwriteBuffer());
			
			V_DrawValueLeft(&titleNumberFont, 160+68 - ((stbar_tics - 80) << 5), 124-4, gamemapinfo.act);
		}
		V_DrawStringRight(&titleFont, 160+68 + ((stbar_tics - 80) << 5), 100, gamemapinfo.name);
		V_DrawStringLeft(&titleFont, 160 - ((stbar_tics - 80) << 5), 124, "Zone");
	}
}

/*
====================
=
= ST_Drawer
=
====================
*/

static void ST_Drawer_ (stbar_t* sb)
{
	if (stbar_tics < 96) {
		ST_DrawTitleCard();
	}
	else
	{
		DrawJagobjLump(score, 16, 10+22, NULL, NULL);
		V_DrawValuePaddedRight(&hudNumberFont, 16 + 120, 10+22, sb->score, 0);

		const int minutes = stbar_tics/(60*2*TICRATE);
		const int seconds = (stbar_tics/(2*TICRATE))%60;
		DrawJagobjLump(time, 16, 26+22, NULL, NULL);
		V_DrawValuePaddedRight(&hudNumberFont, 72, 26+22, minutes, 0);
		DrawJagobjLump(timecolon, 72, 26+22, NULL, NULL);
		V_DrawValuePaddedRight(&hudNumberFont, 72+8+16, 26+22, seconds, 2);

		DrawJagobjLump(sb->rings <= 0 && (gametic / 4 & 1) ? rrings : rings, 16, 42+22, NULL, NULL);
		V_DrawValuePaddedRight(&hudNumberFont, 96, 42+22, sb->rings, 0);

		DrawJagobjLump(face, 16, 176, NULL, NULL);
		V_DrawStringLeft(&menuFont, 16 + 20, 176, "SONIC");
		DrawJagobjLump(livex, 16 + 22, 176 + 10, NULL, NULL);
		V_DrawValuePaddedRight(&menuFont, 16 + 58, 176+8, sb->lives, 0);
	}
}

void CONS_Printf(char *msg, ...) 
{
	va_list argptr;

	va_start(argptr, msg);
	vsprintf(stbar->msg, msg, argptr);
	va_end(argptr);

	stbar->msgTics = 4 * TICRATE;
} 

void ST_Drawer(void)
{
	int	p = splitscreen ? 0 : consoleplayer;
	int e = splitscreen ? MAXPLAYERS : consoleplayer + 1;
	int y[MAXPLAYERS];

	if (debugmode == DEBUGMODE_NODRAW)
		return;

	if (demoplayback)
		return;

	y[consoleplayer] = I_FrameBufferHeight();
	y[consoleplayer^1] = 0;

	while (p < e)
	{
#ifdef MARS
		stbar_y = y[p];
#else
		stbar_y = 0;
		bufferpage = sbartop;		/* draw into status bar overlay */
#endif

		if (playeringame[p])
			ST_Drawer_(&stbar[p]);
		p++;
	}

	if (stbar->msgTics)
	{
		V_DrawStringLeft(&menuFont, 0, 24, stbar->msg);
		stbar->msgTics--;
	}
}

#ifdef MARS
/*
void ST_EraseBlock(int x, int y, int width, int height)
{
	if (debugmode == DEBUGMODE_NODRAW)
		return;

	if (x & 1)
		x -= 1;
	if (width & 1)
		width -= 1;

	DrawJagobj2(sbar, x, stbar_y + y, x, y, width, height, I_FrameBuffer());
}*/

#endif
