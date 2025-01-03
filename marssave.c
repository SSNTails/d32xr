/*
  Victor Luchits

  The MIT License (MIT)

  Copyright (c) 2021 Victor Luchits

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

#define SRAM_MAGIC1		0xDE
#define SRAM_MAGIC2		0xAD

#define SRAM_VERSION	1
#define SRAM_OPTVERSION	3
#define SRAM_MAXSLOTS	10
#define SRAM_SLOTSIZE	200

typedef struct __attribute((packed))
{
	uint8_t version;
	uint8_t skill;
	uint8_t netgame;
	uint8_t mapnumber;
	uint8_t pad[12];

	playerresp_t resp[MAXPLAYERS];
} savegame_t;

typedef struct __attribute((packed))
{
	int8_t version;
	int8_t unused;
	int8_t controltype;
	int8_t viewport;
	int8_t sfxvolume;
	int8_t musicvolume;
	int8_t musictype;
	uint8_t magic1;
	int8_t unused3;
	uint8_t magic2;
	int8_t anamorphic;
	int8_t unused2;
	int8_t sfxdriver;
} saveopts_t;

static char saveslotguard[SRAM_SLOTSIZE - sizeof(savegame_t)] __attribute__((unused));
static char optslotguard[SRAM_SLOTSIZE - sizeof(saveopts_t)] __attribute__((unused));

const uint16_t optslotnumber = SRAM_MAXSLOTS - 1;
const uint16_t optslotoffset = optslotnumber * SRAM_SLOTSIZE;


void ReadGame(int slotnumber)
{
	savegame_t sg;
	const int offset = slotnumber * SRAM_SLOTSIZE;

	if (slotnumber >= optslotnumber)
		return;

	D_memset(&sg, 0, sizeof(savegame_t));

	Mars_ReadSRAM((void*)&sg, offset, sizeof(savegame_t));

	if (sg.version != SRAM_VERSION)
		return;

	starttype = sg.netgame;
	startmap = sg.mapnumber;
	starttype = sg.netgame;
	D_memcpy(playersresp, sg.resp, sizeof(playersresp));
}

static void SaveGameExt(int slotnumber, int mapnum)
{
	savegame_t sg;
	const int offset = slotnumber * SRAM_SLOTSIZE;

	if (slotnumber >= optslotnumber)
		return;

	sg.version = SRAM_VERSION;
	sg.skill = 0;
	sg.netgame = netgame;
	sg.mapnumber = mapnum & 0xFF;
	D_memcpy(sg.resp, playersresp, sizeof(playersresp));

	Mars_WriteSRAM((void*)&sg, offset, sizeof(savegame_t));
}

void SaveGame(int slotnumber)
{
	SaveGameExt(slotnumber, gamemapinfo.mapNumber);
}

void QuickSave(int nextmap)
{
	SaveGameExt(0, nextmap);
}

boolean GetSaveInfo(int slotnumber, VINT* mapnum, VINT* skill, VINT *mode)
{
	savegame_t sg;
	const int offset = slotnumber * SRAM_SLOTSIZE;

	if (slotnumber >= optslotnumber)
		return false;

	D_memset(&sg, 0, sizeof(savegame_t));

	Mars_ReadSRAM((void*)&sg, offset, sizeof(savegame_t));

	if (sg.version != SRAM_VERSION)
		return false;
	if (sg.netgame >= gt_deathmatch)
		sg.netgame = gt_single;

	*mapnum = sg.mapnumber;
	*skill = sg.skill;
	*mode = sg.netgame;
	return true;
}

int SaveCount(void)
{
	int i;
	uint8_t temp;
	int offset = 0;

	// the last slot is used for storing game options
	for (i = 0; i < optslotnumber; i++) {
		Mars_ReadSRAM(&temp, offset, 1);
		if (temp != SRAM_VERSION)
			break;
		offset += SRAM_SLOTSIZE;
	}

	return i;
}

int MaxSaveCount(void)
{
	return optslotnumber;
}

static void SaveOptions(void)
{
	saveopts_t so;

	D_memset(&so, 0, sizeof(saveopts_t));

	so.version = SRAM_OPTVERSION;
	so.controltype = controltype;
	so.viewport = viewportNum;
	so.sfxvolume = sfxvolume;
	so.musicvolume = musicvolume;
	so.musictype = musictype;
	so.anamorphic = anamorphicview;
	so.sfxdriver = sfxdriver;
	so.magic1 = SRAM_MAGIC1;
	so.magic2 = SRAM_MAGIC2;

	Mars_WriteSRAM((void*)&so, optslotoffset, sizeof(saveopts_t));
}

static void ReadOptions(void)
{
	saveopts_t so;

	D_memset(&so, 0, sizeof(saveopts_t));

	Mars_ReadSRAM((void*)&so, optslotoffset, sizeof(saveopts_t));

	if (so.version != SRAM_OPTVERSION)
		return;

	if (so.sfxvolume > 64)
		so.sfxvolume = 64;
	if (so.musicvolume > 64)
		so.musicvolume = 64;
	if (so.controltype >= NUMCONTROLOPTIONS)
		so.controltype = 0;
	if (so.musictype < mustype_none || so.musictype > mustype_cd)
		so.musictype = mustype_fm;
	if (so.musictype == mustype_cd && !S_CDAvailable())
		so.musictype = mustype_fm;
	if (so.viewport < 0 || so.viewport >= numViewports)
		so.viewport = R_DefaultViewportSize();
	if (so.anamorphic < 0 || so.anamorphic > 1)
		so.anamorphic = 0;
	if (so.sfxdriver < 0 || so.sfxdriver > 2)
		so.sfxdriver = 0;

	sfxvolume = so.sfxvolume;
	musicvolume = so.musicvolume;
	controltype = so.controltype;
	viewportNum = so.viewport;
	musictype = so.musictype;
	ticsperframe = MINTICSPERFRAME;
	anamorphicview = so.anamorphic;
	sfxdriver = so.sfxdriver;
}

void ClearEEProm(void)
{
	saveopts_t so;

	D_memset(&so, 0, sizeof(saveopts_t));

	Mars_WriteSRAM((void*)&so, optslotoffset, sizeof(saveopts_t));
}

void ReadEEProm(void)
{
	controltype = 0;
	sfxvolume = 64;
	musicvolume = 64;
	viewportNum = R_DefaultViewportSize();
	musictype = mustype_fm;
	ticsperframe = MINTICSPERFRAME;
	anamorphicview = 0;
	sfxdriver = 0;

	ReadOptions();
}

void WriteEEProm(void)
{
	SaveOptions();
}
