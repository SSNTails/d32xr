/* O_local.h */

#ifndef __O_LOCAL__
#define __O_LOCAL__

#include "doomdef.h"

typedef enum
{
	ms_none,
	ms_main,
	ms_game,
	ms_controls,
	ms_audio,
	ms_video,
	ms_help,

	NUMMENUSCREENS
} screenpos_t;

extern VINT screenpos;

#endif	/* __O_LOCAL__ */