#include "st_inter.h"
#include "v_font.h"
#include <string.h>

typedef union
{
	struct
	{
		uint32_t score; // fake score
		int32_t timebonus, ringbonus, perfbonus, total;
		VINT min, sec;
		int tics;
		boolean gotperfbonus; // true if we should show the perfect bonus line
		VINT ttlnum; // act number being displayed
		VINT ptimebonus; // TIME BONUS
		VINT pringbonus; // RING BONUS
		VINT pperfbonus; // PERFECT BONUS
		VINT ptotal; // TOTAL
		char passed1[13]; // KNUCKLES GOT
		char passed2[16]; // THROUGH THE ACT
		VINT passedx1, passedx2;
		VINT gotlife; // Player # that got an extra life
	} coop;
} y_data;

static y_data data;

// graphics
static boolean gottimebonus;
static boolean gotemblem;

static int32_t intertic;
static int32_t endtic = -1;

static enum
{
	int_none,
	int_coop,     // Single Player/Cooperative
} inttype = int_none;

#define BASEVIDWIDTH 320

//
// Y_UnloadData
//
static void Y_UnloadData(void)
{
    // Nothing... for now
}

//
// Y_AwardCoopBonuses
//
// Awards the time and ring bonuses.
//
static void Y_AwardCoopBonuses(void)
{
	int i;
	int sharedringtotal = 0;

	for (i = 0; i < MAXPLAYERS; i++)
	{
		//for the sake of my sanity, let's get this out of the way first
		if (!playeringame[i])
			continue;

		sharedringtotal += players[i].health - 1;
	}

	// with that out of the way, go back to calculating bonuses like usual
	for (i = 0; i < MAXPLAYERS; i++)
	{
		int secs, bonus, oldscore;

		if (!playeringame[i])
			continue;

		// calculate time bonus
		secs = data.coop.tics / TICRATE;
		if (secs < 30)
			bonus = 50000;
		else if (secs < 45)
			bonus = 10000;
		else if (secs < 60)
			bonus = 5000;
		else if (secs < 90)
			bonus = 4000;
		else if (secs < 120)
			bonus = 3000;
		else if (secs < 180)
			bonus = 2000;
		else if (secs < 240)
			bonus = 1000;
		else if (secs < 300)
			bonus = 500;
		else
			bonus = 0;

		if (i == consoleplayer)
		{
			data.coop.timebonus = bonus;
			if (players[i].health)
				data.coop.ringbonus = (players[i].health-1) * 100;
			else
				data.coop.ringbonus = 0;
			data.coop.total = 0;
			data.coop.score = players[i].score;
/*
			if (sharedringtotal && sharedringtotal >= nummaprings) //perfectionist!
			{
				data.coop.perfbonus = 50000;
				data.coop.gotperfbonus = true;
			}
			else*/
			{
				data.coop.perfbonus = 0;
				data.coop.gotperfbonus = false;
			}
		}

		oldscore = players[i].score;

		players[i].score += bonus;
		if (players[i].health)
			players[i].score += (players[i].health-1) * 100; // ring bonus

		//todo: more conditions where we shouldn't award a perfect bonus?
/*		if (sharedringtotal && sharedringtotal >= nummaprings)
		{
			players[i].score += 50000; //perfect bonus
		}*/

		// grant extra lives right away since tally is faked
		// TODO: No, we will have a true tally!
		players[i].lives++;
//		P_GivePlayerLives(&players[i], (players[i].score/50000) - (oldscore/50000));

		if ((players[i].score/50000) - (oldscore/50000) > 0)
			data.coop.gotlife = i;
		else
			data.coop.gotlife = -1;
	}
}

//
// Y_IntermissionDrawer
//
// Called by D_Display. Nothing is modified here; all it does is draw.
// Neat concept, huh?
//
void Y_IntermissionDrawer(void)
{
	if (inttype == int_none)
		return;

	if (inttype == int_coop)
	{
		const int offset = 24;
		const int squeeze = 12;

		// draw the "got through act" lines and act number
		V_DrawStringCenter(&titleFont, data.coop.passedx1, 112-56+offset, data.coop.passed1);
		V_DrawStringCenter(&titleFont, data.coop.passedx2, 112-56+16+2+offset, data.coop.passed2);

		V_DrawValueLeft(&titleNumberFont, BASEVIDWIDTH-72, 112-56+6+offset, gamemapinfo.act);

		DrawJagobjLump(data.coop.ptimebonus, 68, 112+offset-squeeze, NULL, NULL);
		V_DrawValueRight(&hudNumberFont, BASEVIDWIDTH - 68, 112+1+offset-squeeze, data.coop.timebonus);

		DrawJagobjLump(data.coop.pringbonus, 68, 112+16+offset-squeeze, NULL, NULL);
		V_DrawValueRight(&hudNumberFont, BASEVIDWIDTH - 68, 112+16+1+offset-squeeze, data.coop.ringbonus);
/*
		//PERFECT BONUS
		if (data.coop.gotperfbonus)
		{
			V_DrawScaledPatch(56, 84 + ((9*SHORT(tallnum[0]->height))+1)/2, 0, data.coop.pperfbonus);
			Y_DrawNum(BASEVIDWIDTH - 68, 85 + ((9*SHORT(tallnum[0]->height))+1)/2, data.coop.perfbonus);
		}*/

		DrawJagobjLump(data.coop.ptotal, 88, 112+16+16+16+offset-squeeze, NULL, NULL);
		V_DrawValueRight(&hudNumberFont, BASEVIDWIDTH - 68, 112+16+16+16+1+offset-squeeze, data.coop.total);
	}
}

//
// Y_EndIntermission
//
void Y_EndIntermission(void)
{
	Y_UnloadData();

	endtic = -1;
	inttype = int_none;
}

//
// Y_EndGame
//
// Why end the game?
// Because Y_FollowIntermission and F_EndCutscene would
// both do this exact same thing *in different ways* otherwise,
// which made it so that you could only unlock Ultimate mode
// if you had a cutscene after the final level and crap like that.
// This function simplifies it so only one place has to be updated
// when something new is added.
void Y_EndGame(void)
{
	CONS_Printf("Y_EndGame()");
/*	if (nextmap == 1102-1) // end game with credits
		F_StartCredits();
	else if (nextmap == 1101-1) // end game with evaluation
		F_StartGameEvaluation();
    else
	    D_StartTitle();	// 1100 or competitive multiplayer, so go back to title screen.
		*/
}

//
// Y_FollowIntermission
//
static void Y_FollowIntermission(void)
{
	CONS_Printf("Y_FollowIntermission");
/*	if (nextmap < 1100-1)
	{
		// normal level
		G_AfterIntermission();
		return;
	}*/

	Y_EndGame();
}

//
// Y_Ticker
//
// Manages fake score tally for single player end of act, and decides when intermission is over.
//
void Y_Ticker(void)
{
	if (inttype == int_none)
		return;

	intertic++;

	// single player is hardcoded to go away after awhile
	if (intertic == endtic)
	{
		Y_EndIntermission();
		Y_FollowIntermission();
		return;
	}

	if (endtic != -1)
		return; // tally is done

	if (inttype == int_coop) // coop or single player, normal level
	{
		if (!intertic) // first time only
			S_StartSong(gameinfo.intermissionMus, 0, cdtrack_intermission);

		if (intertic < TICRATE) // one second pause before tally begins
			return;

		if (data.coop.ringbonus || data.coop.timebonus || data.coop.perfbonus)
		{
			if (!(intertic & 1))
				S_StartSound(NULL, sfx_s3k_5b); // tally sound effect

			// ring and time bonuses count down by 333 each tic
			if (data.coop.ringbonus)
			{
				data.coop.ringbonus -= 333;
				data.coop.total += 333;
				data.coop.score += 333;
				if (data.coop.ringbonus < 0) // went too far
				{
					data.coop.score += data.coop.ringbonus;
					data.coop.total += data.coop.ringbonus;
					data.coop.ringbonus = 0;
				}
			}
			if (data.coop.timebonus)
			{
				data.coop.timebonus -= 333;
				data.coop.total += 333;
				data.coop.score += 333;
				if (data.coop.timebonus < 0)
				{
					data.coop.score += data.coop.timebonus;
					data.coop.total += data.coop.timebonus;
					data.coop.timebonus = 0;
				}
			}
			if (data.coop.perfbonus)
			{
				data.coop.perfbonus -= 333;
				data.coop.total += 333;
				data.coop.score += 333;
				if (data.coop.perfbonus < 0)
				{
					data.coop.score += data.coop.perfbonus;
					data.coop.total += data.coop.perfbonus;
					data.coop.perfbonus = 0;
				}
			}
			if (!data.coop.timebonus && !data.coop.ringbonus && !data.coop.perfbonus)
			{
				endtic = intertic + 3*TICRATE; // 3 second pause after end of tally
				S_StartSound(NULL, sfx_s3k_b0); // cha-ching!
			}

			if (data.coop.score % 50000 < 333) // just passed a 50000 point mark
			{
				S_StopSong();
				S_StartSong(gameinfo.xtlifeMus, 0, cdtrack_xtlife);
			}
		}
		else
		{
			endtic = intertic + 3*TICRATE; // 3 second pause after end of tally
			S_StartSound(NULL, sfx_s3k_b0); // cha-ching!
		}
	}
}

//
// Y_StartIntermission
//
// Called by G_DoCompleted. Sets up data for intermission drawer/ticker.
//
void Y_StartIntermission(void)
{
	const int delaytime = gamemapinfo.act == 3 ? 2*TICRATE : 3*TICRATE;
	int worldTime = leveltime - delaytime + TICRATE - players[consoleplayer].exiting;

	intertic = -1;

	inttype = int_coop;

	switch (inttype)
	{
		case int_coop: // coop or single player, normal level
		{
			gottimebonus = false;
			gotemblem = false;

			// award time and ring bonuses
			Y_AwardCoopBonuses();

			// setup time data
			data.coop.tics = worldTime; // used if cv_timetic is on
			data.coop.sec = worldTime / TICRATE;
			data.coop.min = data.coop.sec / 60;
			data.coop.sec %= 60;

			// get single player specific patches
			data.coop.ptotal = W_CheckNumForName("YB_TOTAL");
			data.coop.ptimebonus = W_CheckNumForName("YB_TIME");
			data.coop.pringbonus = W_CheckNumForName("YB_RING");
//			data.coop.pperfbonus = W_CheckNumForName("YPFBONUS");

			strcpy(data.coop.passed1, "sonic got");
			strcpy(data.coop.passed2, "through act");
			
			data.coop.passedx1 = 150;
			data.coop.passedx2 = 150;
			break;
		}

		case int_none:
		default:
			break;
	}
}
