/*
  Victor Luchits, Samuel Villarreal and Fabien Sanglard

  The MIT License (MIT)

  Copyright (c) 2021 Victor Luchits, Derek John Evans, id Software and ZeniMax Media

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/

#include "doomdef.h"
#include "mars.h"
#include "r_local.h"

// based on work by Samuel Villarreal and Fabien Sanglard

#define FIRE_STOP_TICON 90

static jagobj_t *intro_titlepic;
static jagobj_t *intro_titlepic2;
static int titlepic_start;

/* 
================ 
= 
= I_InitMenuFire  
=
================ 
*/ 
void I_InitMenuFire(jagobj_t *titlepic, jagobj_t *titlepic2)
{
	titlepic_start = I_GetTime();

	R_FadePalette(dc_playpals, (PALETTE_SHIFT_CONVENTIONAL_FADE_TO_BLACK + 4), dc_cshift_playpals);

	intro_titlepic = titlepic;
	intro_titlepic2 = titlepic2;

	clearscreen = 2;
}

/*
================
=
= I_StopMenuFire
=
================
*/
void I_StopMenuFire(void)
{
	Mars_ClearCache();
}

/*
================
=
= I_DrawMenuFire
=
================
*/
void I_DrawMenuFire(void)
{
	if (clearscreen > 0) {
		I_ResetLineTable();
		I_ClearFrameBuffer();
		clearscreen--;
	}

	const int y = 16;
	jagobj_t* titlepic = intro_titlepic;

	// scroll the title pic from bottom to top
	if (titlepic != NULL)
	{
		const int titlepic_pos_x = (320 - titlepic->width) / 2;
		DrawJagobj3(titlepic, titlepic_pos_x, y, 0, 0, 0, titlepic->height, 320, I_OverwriteBuffer());

		if (intro_titlepic2 != NULL)
			DrawJagobj3(intro_titlepic2, titlepic_pos_x, y, 0, 0, 0, titlepic->height, 320, I_OverwriteBuffer());
	}

	int duration = I_GetTime() - titlepic_start;
	if (duration > FIRE_STOP_TICON - 20)
	{
		// Fade to white
		int palIndex = duration - (FIRE_STOP_TICON - 20);
		palIndex /= 4;
		if (palIndex > 5)
			palIndex = 5;

		R_FadePalette(dc_playpals, palIndex, dc_cshift_playpals);
	}
	else if (duration < 20 && titlepic_start >= 0)
	{
		// Fade in from black
		int palIndex = 10 - (duration / 4);

		R_FadePalette(dc_playpals, palIndex, dc_cshift_playpals);
	}
	else if (duration < 22 && titlepic_start >= 0)
	{
		I_SetPalette(dc_playpals);
	}
}
