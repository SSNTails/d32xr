/* f_main.c -- finale */

#include "doomdef.h"
#include "r_local.h"

typedef struct
{
	char		*name;
	mobjtype_t	type;
} castinfo_t;

static const castinfo_t castorder[] = {
{"zombieman", MT_POSSESSED},
{"shotgun guy", MT_SHOTGUY},
{"heavy weapon dude", MT_CHAINGUY},
{"imp", MT_TROOP},
{"demon", MT_SERGEANT},
{"spectre", MT_SERGEANT},
{"lost soul", MT_SKULL},
{"cacodemon", MT_HEAD},
{"hell knight", MT_KNIGHT},
{"baron of hell", MT_BRUISER},
{"arachnotron", MT_BABY},
{"revenant", MT_UNDEAD},
{"mancubus", MT_FATSO},
{"cyberdemon", MT_CYBORG},
{"spider mastermind", MT_SPIDER},
{"our hero", MT_PLAYER},

{NULL,0}
};

typedef enum
{
	fin_endtext,
	fin_charcast
} final_e;

#define TEXTTIME	4
#define STARTX		8
#define STARTY		8

#define SPACEWIDTH	8
#define NUMENDOBJ	29

typedef struct
{
	int			castnum;
	int			casttics;
	const state_t		*caststate;
	boolean		castdeath;
	int			castframes;
	int			castonmelee;
	boolean		castattacking;
	final_e		status;
	boolean 	textprint;	/* is endtext done printing? */
	int			textindex;
	VINT		textdelay;
	VINT		text_x;
	VINT		text_y;
	VINT		drawbg;
	jagobj_t	**endobj;
	drawcol_t	drcol;
	int 		endFlat;
} finale_t;

finale_t *fin;

static void F_DrawBackground(void);

/*
==================
=
= BufferedDrawSprite
=
= Cache and draw a game sprite to the 8 bit buffered screen
==================
*/

void BufferedDrawSprite (int sprite, int frame, int rotation, int top, int left)
{
	spritedef_t	*sprdef;
	spriteframe_t	*sprframe;
	VINT 		*sprlump;
	patch_t		*patch;
	byte		*pixels, *src;
	int			x, sprleft, sprtop, spryscale;
	fixed_t 	spriscale;
	int			lump;
	boolean		flip;
	int			texturecolumn;
	int			light = HWLIGHT(143);
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

	flip = false;
	if (lump < 0)
	{
		lump = -(lump + 1);
		flip = true;
	}

	if (lump <= 0)
		return;

	patch = (patch_t *)W_POINTLUMPNUM(lump);
	pixels = R_CheckPixels(lump + 1);

	S_UpdateSounds ();
	 	
/* */
/* coordinates are in a 160*112 screen (doubled pixels) */
/* */
	sprtop = top;
	sprleft = left;
	spryscale = 2;
	spriscale = FRACUNIT/spryscale;

	sprtop -= patch->topoffset;
	sprleft -= patch->leftoffset;

	I_SetThreadLocalVar(DOOMTLS_COLORMAP, dc_colormaps);

/* */
/* draw it by hand */
/* */
	for (x=0 ; x<patch->width ; x++)
	{
		int 	colx;
		byte	*columnptr;

		if (sprleft+x < 0)
			continue;
		if (sprleft+x >= 160)
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
			colx += colx;

			fin->drcol(colx, top, bottom, light, 0, spriscale, src, 128);
			fin->drcol(colx+1, top, bottom, light, 0, spriscale, src, 128);
		}
	}
}


/*============================================================================ */

#if 0
/* '*' = newline */
static const char	endtextstring[] =
	"you did it! by turning*"
	"the evil of the horrors*"
	"of hell in upon itself*"
	"you have destroyed the*"
	"power of the demons.**"
	"their dreadful invasion*"
	"has been stopped cold!*"
	"now you can retire to*"
	"a lifetime of frivolity.**"
	"  congratulations!";

/* '*' = newline */
static const char	endtextstring[] =
	"     id software*"
	"     salutes you!*"
	"*"
	"  the horrors of hell*"
	"  could not kill you.*"
	"  their most cunning*"
	"  traps were no match*"
	"  for you. you have*"
	"  proven yourself the*"
	"  best of all!*"
	"*"
	"  congratulations!";
#endif

/*=============================================== */
/* */
/* Print a string in big font - LOWERCASE INPUT ONLY! */
/* */
/*=============================================== */
void F_PrintString1(const char *string)
{
	int		index;
	int		val;

	index = 0;
	while(string[index])
	{
		switch(string[index])
		{
			case ' ':
				fin->text_x += SPACEWIDTH;
				val = 30;
				break;
			case '.':
				val = 26;
				break;
			case '!':
				val = 27;
				break;
			case '-':
				val = 28;
				break;
			case '*':
				val = 30;
				fin->text_x = STARTX;
				fin->text_y += fin->endobj[0]->height + 4;
				break;
			default:
				val = string[index] >= 'a' ? string[index] - 'a' : string[index] - 'A';			
				break;
		}
		if (val < NUMENDOBJ)
		{
			DrawJagobj(fin->endobj[val],fin->text_x,fin->text_y);
			fin->text_x += fin->endobj[val]->width;
			if (fin->text_x > 316 && string[index+1] != '*')
			{
				fin->text_x = STARTX;
				fin->text_y += fin->endobj[val]->height + 4;
			}
		}
		index++;
	}
}

// Prints to both framebuffers, if needed.
static void F_PrintString2(const char* string)
{
#ifdef MARS
	int		btext_x, btext_y;

	btext_x = fin->text_x;
	btext_y = fin->text_y;
	F_PrintString1(string);

	UpdateBuffer();

	fin->text_x = btext_x;
	fin->text_y = btext_y;
#endif
	F_PrintString1(string);
}

/*=============================================== */
/* */
/* Print character cast strings */
/* */
/*=============================================== */
static void F_CastPrint(const char *string)
{
	int		i,width,chr;
	
	width = 0;
	for (i = 0; string[i]; i++)
		switch(string[i])
		{
			case ' ':
				width += SPACEWIDTH;
				break;
			default:
				chr = string[i] > 'a' ? string[i] - 'a' : string[i] - 'A';
				width += fin->endobj[chr]->width;
				break;
		}

	fin->text_x = 160 - (width >> 1);
	fin->text_y = 10;
	F_PrintString1(string);
}

/*
=======================
=
= F_Start
=
=======================
*/

void F_Start (void)
{
	int	i;
	int	l;
	extern boolean canwipe;

	if (!gameinfo.endMus || !*gameinfo.endMus)
		S_StartSongByName(gameinfo.victoryMus, 1, cdtrack_end);
	else
		S_StartSongByName(gameinfo.endMus, 1, cdtrack_end);

	fin = Z_Malloc(sizeof(*fin), PU_STATIC);
	D_memset(fin, 0, sizeof(*fin));

	fin->status = fin_endtext;		/* END TEXT PRINTS FIRST */
	fin->textdelay = TEXTTIME;
	fin->text_x = STARTX;
	fin->text_y = STARTY;
	fin->drawbg = 3;
	fin->endobj = Z_Malloc(sizeof(*fin->endobj) * NUMENDOBJ, PU_STATIC);

	l = W_GetNumForName ("CHAR_097");
	for (i = 0; i < NUMENDOBJ; i++)
		fin->endobj[i] = W_CacheLumpNum(l+i, PU_STATIC);

	fin->caststate = &states[mobjinfo[castorder[fin->castnum].type].seestate];
	fin->casttics = fin->caststate->tics;

	fin->endFlat = -1;
	if (gameinfo.endFlat && *gameinfo.endFlat)
		fin->endFlat = W_CheckNumForName(gameinfo.endFlat);

#ifndef MARS
	backgroundpic = W_POINTLUMPNUM(W_GetNumForName("M_TITLE"));
	DoubleBufferSetup ();
#endif

	I_SetPalette(W_POINTLUMPNUM(W_GetNumForName("PLAYPALS")));

	canwipe = true;
}

void F_Stop (void)
{
	int	i;

	for (i = 0;i < NUMENDOBJ; i++)
		Z_Free(fin->endobj[i]);
	Z_Free(fin->endobj);
	Z_Free(fin);
}




/*
=======================
=
= F_Ticker
=
=======================
*/

int F_Ticker (void)
{
	int		st;
	int		buttons, oldbuttons;

	
/* */
/* check for press a key to kill actor */
/* */
	buttons = players[consoleplayer].ticbuttons;
	oldbuttons = players[consoleplayer].oldticbuttons;

	if (ticon <= 10)
		return 0;
	if ((buttons & BT_START) && !(oldbuttons & BT_START))
		return 1;

	if (fin->status == fin_endtext)
	{
		if (fin->textindex == (3*15)/TEXTTIME)
			fin->textprint = true;
			
		if (( ((buttons & BT_ATTACK) && !(oldbuttons & BT_ATTACK) )
		|| ((buttons & BT_SPEED) && !(oldbuttons & BT_SPEED) )
		|| ((buttons & BT_USE) && !(oldbuttons & BT_USE) ) ) &&
		fin->textprint == true)
		{
			if (finale && gameinfo.endShowCast)
			{
				fin->status = fin_charcast;

				if (!gameinfo.victoryMus || !*gameinfo.victoryMus)
					S_StartSongByName(gameinfo.endMus, 1, cdtrack_victory);
				else
					S_StartSongByName(gameinfo.victoryMus, 1, cdtrack_victory);

#ifndef JAGUAR
				if (mobjinfo[castorder[fin->castnum].type].seesound)
					S_StartSound (NULL, mobjinfo[castorder[fin->castnum].type].seesound); 
#endif
			}
			else
			{
				return 1;
			}
		}
		return 0;
	}
	
	if (!fin->castdeath)
	{
		if ( ((buttons & BT_ATTACK) && !(oldbuttons & BT_ATTACK) )
		|| ((buttons & BT_SPEED) && !(oldbuttons & BT_SPEED) )
		|| ((buttons & BT_USE) && !(oldbuttons & BT_USE) ) )
		{
		/* go into death frame */
#ifndef JAGUAR
			if (mobjinfo[castorder[fin->castnum].type].deathsound)
				S_StartSound(NULL, mobjinfo[castorder[fin->castnum].type].deathsound);
#endif
			fin->castdeath = true;
			fin->caststate = &states[mobjinfo[castorder[fin->castnum].type].deathstate];
			fin->casttics = fin->caststate->tics;
			fin->castframes = 0;
			fin->castattacking = false;
		}
	}

	if (ticon & 1)
		return 0;

/* */
/* advance state */
/* */
	if (--fin->casttics > 0)
		return 0;			/* not time to change state yet */

	if (fin->caststate->tics == -1 || fin->caststate->nextstate == S_NULL)
	{	/* switch from deathstate to next monster */
		int oldnum = fin->castnum;
		do {
			fin->castnum++;
			if (castorder[fin->castnum].name == NULL)
				fin->castnum = 0;
			if (sprites[states[mobjinfo[castorder[fin->castnum].type].seestate].sprite].numframes == 0)
				continue;
			break;
		} while (fin->castnum != oldnum);
		fin->castdeath = false;
#ifndef JAGUAR
		if (mobjinfo[castorder[fin->castnum].type].seesound)
			S_StartSound (NULL, mobjinfo[castorder[fin->castnum].type].seesound);
#endif
		fin->caststate = &states[mobjinfo[castorder[fin->castnum].type].seestate];
		fin->castframes = 0;
	}
	else
	{	/* just advance to next state in animation */
		if (fin->caststate == &states[S_PLAY_ATK2])
			goto stopattack;	/* Oh, gross hack! */
		st = fin->caststate->nextstate;
		fin->caststate = &states[st];
		fin->castframes++;
#if 1
/*============================================== */
/* sound hacks.... */
{
	int sfx;
	
		switch (st)
		{
		case S_PLAY_ATK2: sfx = sfx_shotgn; break;
		case S_POSS_ATK2: sfx = sfx_pistol; break;
		case S_SPOS_ATK2: sfx = sfx_shotgn; break;
		//case S_VILE_ATK2: sfx = sfx_vilatk; break;
		case S_SKEL_FIST2: sfx = sfx_skeswg; break;
		case S_SKEL_FIST4: sfx = sfx_skepch; break;
		case S_SKEL_MISS2: sfx = sfx_skeatk; break;
		case S_FATT_ATK8:
		case S_FATT_ATK5:
		case S_FATT_ATK2: sfx = sfx_firsht; break;
		case S_CPOS_ATK2:
		case S_CPOS_ATK3:
		case S_CPOS_ATK4: sfx = sfx_shotgn; break;
		case S_TROO_ATK3: sfx = sfx_claw; break;
		case S_SARG_ATK2: sfx = sfx_sgtatk; break;
		case S_BOSS_ATK2: 
		case S_HEAD_ATK2: sfx = sfx_firsht; break;
		case S_SKULL_ATK2: sfx = sfx_sklatk; break;
		case S_SPID_ATK4:
		case S_SPID_ATK3: sfx = sfx_shotgn; break;
		case S_BSPI_ATK2: sfx = sfx_plasma; break;
		case S_CYBER_ATK2:
		case S_CYBER_ATK4:
		case S_CYBER_ATK6: sfx = sfx_rlaunc; break;
		//case S_PAIN_ATK3: sfx = sfx_sklatk; break;
		default: sfx = 0; break;
		}
		
		if (sfx)
			S_StartSound (NULL, sfx);
}
#endif
/*============================================== */
	}
	
	if (fin->castframes == 12)
	{	/* go into attack frame */
		fin->castattacking = true;
		if (fin->castonmelee)
			fin->caststate=&states[mobjinfo[castorder[fin->castnum].type].meleestate];
		else
			fin->caststate=&states[mobjinfo[castorder[fin->castnum].type].missilestate];
		fin->castonmelee ^= 1;
		if (fin->caststate == &states[S_NULL])
		{
			if (fin->castonmelee)
				fin->caststate=
				&states[mobjinfo[castorder[fin->castnum].type].meleestate];
			else
				fin->caststate=
				&states[mobjinfo[castorder[fin->castnum].type].missilestate];
		}
	}
	
	if (fin->castattacking)
	{
		if (fin->castframes == 24 
		||	fin->caststate == &states[mobjinfo[castorder[fin->castnum].type].seestate] )
		{
stopattack:
			fin->castattacking = false;
			fin->castframes = 0;
			fin->caststate = &states[mobjinfo[castorder[fin->castnum].type].seestate];
		}
	}
	
	fin->casttics = fin->caststate->tics;
	if (fin->casttics == -1)
		fin->casttics = 15;
		
	return 0;
}

static void F_DrawBackground(void)
{
#ifdef MARS
	DrawTiledBackground2(fin->endFlat);
#else
	EraseBlock(0, 0, 320, 200);
#endif
}

/*
=======================
=
= F_Drawer
=
=======================
*/

void F_Drawer (void)
{
	int	top, left;

	fin->drcol = I_DrawColumn;
	if (D_strcasecmp(castorder[fin->castnum].name, "spectre") == 0)
		fin->drcol = I_DrawFuzzColumn;

	// HACK
	if (finale)
	{
		viewportWidth = 320;
		viewportHeight = I_FrameBufferHeight();
		viewportbuffer = (pixel_t*)I_FrameBuffer();
	}

	if (fin->drawbg) {
		fin->drawbg--;
		F_DrawBackground();
	}

	switch(fin->status)
	{
		case fin_endtext:
			if (!--fin->textdelay)
			{
				char	str[2];
				const char *text = finale ? gameinfo.endText : gamemapinfo.interText;

				if (!text)
					return;
				str[1] = 0;
				str[0] = text[fin->textindex];
				if (!str[0])
					return;
				F_PrintString2(str);
				fin->textdelay = TEXTTIME;
				fin->textindex++;
			}
			break;
			
		case fin_charcast:
			fin->drawbg = 2;
			switch (castorder[fin->castnum].type) {
				case MT_CYBORG:
					top = 115;
					left = 80;
					break;
				case MT_SPIDER:
					top = 110;
					left = 90;
					break;
				case MT_UNDEAD:
					top = 100;
					left = 80;
					break;
				default:
					top = 90;
					left = 80;
					break;
			}
			BufferedDrawSprite (fin->caststate->sprite,
					fin->caststate->frame&FF_FRAMEMASK,0,top,left);
			F_CastPrint (castorder[fin->castnum].name);
			break;
	}
}

