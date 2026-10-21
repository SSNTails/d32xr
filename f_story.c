#include "doomdef.h"
#include "f_story.h"
#include "v_font.h"
#include "marshw.h"
#include "r_local.h"

boolean test_always_zoom = false;
fixed_t test_x_zoom = 0;
fixed_t test_y_zoom = 0;
fixed_t test_x_pos = 0;
fixed_t test_y_pos = 0;

boolean transitionInProgress = false;
boolean transitionDirection = 0;
int8_t transitionCount = 0;
int currentPhase = 0;
int currentScene = 0;

short sceneFrameCount = 0;
short phaseFrameCount = 0;

typedef struct
{
	int16_t x, y;
} coord_t;

typedef struct
{
    int16_t x, y;
    int16_t width, height;
} rect_t;

typedef struct storyscene_s
{
	VINT transitionOutHeight;
	VINT picLump;
	jagobj_t *background;
	VINT backgroundX;
	VINT backgroundY;
    const char *text;
    int16_t textPos; // Position in the string of how far along the text has been printed
    int16_t textCharDelayTics; // Number of tics to wait before incrementing textPos
    int16_t textCharDelayCounter; // Decrement this. When it hits zero, textPos++
    int16_t postTextDelay; // Number of tics to wait after printing all text before transitioning to the next scene
	int16_t preTextDelay; // Number of tics to wait before beginning text printing
    rect_t textBox; // Bounding box to print text inside
	int16_t curTextPosX;
	int16_t curTextPosY;
	int16_t drawText; // Flag to indicate that the text should be drawn this frame

	void (*init)(struct storyscene_s *self);
    void (*tic)(struct storyscene_s *self);
    void (*draw)(struct storyscene_s *self);
	void (*stop)(struct storyscene_s *self);
} storyscene_t;

typedef struct
{
	storyscene_t scene;
	jagobj_t *satellite;
	VINT picSatellite;
	fixed_t satX, satY, satZ;
	VINT prevSatX[2], prevSatY[2];
	fixed_t prevSatZ[2];
} scene_1_t;

typedef struct
{
	storyscene_t scene;
	jagobj_t *asteroid[35];
	int asteroidIndex;
} scene_4_t;

typedef struct
{
	storyscene_t scene;
	VINT picRadar;
	VINT picSweep;
	jagobj_t *radar;
	jagobj_t *sweep;
	jagobj_t *radWave[4];
	jagobj_t *actBar[4];
	jagobj_t *warning;
	jagobj_t *blip;
	int blipShift;
	int animationCounter;
} scene_5_t;

typedef struct
{
	storyscene_t scene;
	jagobj_t *flicker[3];
	jagobj_t *stache[2];
	jagobj_t *metal;
	jagobj_t *noflash;
	fixed_t metalX, metalY;
	boolean talking;
	int stacheCounter;
	int flickerCounter;
} scene_6_t;

typedef struct
{
	storyscene_t scene;
	jagobj_t *sleep[3]; // SLEEP2, SLEEP3, SLEEP4
	int sleepCounter; // For eye animation
} scene_7_t;

typedef struct
{
	storyscene_t scene;
	jagobj_t *crosshair;
	jagobj_t *targetAcquired;
	jagobj_t *gfzMountainText;
	jagobj_t *compass;
	jagobj_t *bar;
	jagobj_t *bottomLeftInd[4];
	int barAnim;
	int acquiredWidth;
	int gfzWidth;
	fixed_t crosshairScale;
} scene_8_t;

typedef struct
{
	storyscene_t scene;
	jagobj_t *sonic;
	jagobj_t *aura;
	jagobj_t *zoom[14];
	coord_t zoomCoords[14];
	fixed_t sonicPosX;
	fixed_t sonicPosY;
	fixed_t sonicScale;
	fixed_t auraScale;
	int16_t zoomStartDelay;
	int16_t zoomChangeInterval;
	int16_t zoomCurIndex;
	int16_t sonicCounter;
	int16_t auraCounter;
	int16_t sonicStep;
} scene_9_t;

typedef struct 
{
	storyscene_t scene;
} scene_10_t;

typedef struct 
{
	storyscene_t scene;
} scene_11_t;

typedef struct 
{
	storyscene_t scene;
} scene_12_t;

typedef struct 
{
	storyscene_t scene;
} scene_13_t;

typedef struct 
{
	storyscene_t scene;
} scene_14_t;

typedef struct 
{
	storyscene_t scene;
} scene_15_t;

typedef struct 
{
	storyscene_t scene;
} scene_16_t;

typedef struct
{
	storyscene_t scene;
} scene_2_t, scene_3_t;

//#define NUMSCENES 12
#define NUMSCENES	16
storyscene_t *introScenes[NUMSCENES];


/*
void DebugControls()
{
	if ((ticrealbuttons & BT_ACTION_MODE) && !(oldticrealbuttons & BT_ACTION_MODE)) {
		test_always_zoom ^= true;
	}

	if (ticrealbuttons & BT_ACTION_START) {
		test_always_zoom = false;
		test_x_pos = 0;
		test_y_pos = 0;
		test_x_zoom = 0;
		test_y_zoom = 0;
	}

	if (ticrealanalogx == 0 && ticrealanalogy == 0) {
		if (ticrealbuttons & BT_ACTION_UP) {
			test_y_pos -= 0x40000;
		}
		else if (ticrealbuttons & BT_ACTION_DOWN) {
			test_y_pos += 0x40000;
		}

		if (ticrealbuttons & BT_ACTION_LEFT) {
			test_x_pos -= 0x40000;
		}
		else if (ticrealbuttons & BT_ACTION_RIGHT) {
			test_x_pos += 0x40000;
		}
	}
	else {
		if (D_abs(ticrealanalogx) > 0x1F || (D_abs(ticrealanalogy) > 0x1F && D_abs(ticrealanalogx) > 0x0F)) {
			test_x_pos += (ticrealanalogx << 10);
		}

		if (D_abs(ticrealanalogy) > 0x1F || (D_abs(ticrealanalogx) > 0x1F && D_abs(ticrealanalogy) > 0x0F)) {
			test_y_pos += (ticrealanalogy << 10);
		}
	}

	if (ticrealanalogt == 0) {
		if (ticrealbuttons & BT_ACTION_CAMLEFT) {
			test_x_zoom -= 0x1800;
			test_y_zoom -= 0x1800;
		}
		else if (ticrealbuttons & BT_ACTION_CAMRIGHT) {
			test_x_zoom += 0x1800;
			test_y_zoom += 0x1800;
		}
	}
	else if (ticrealanalogt > 0x1F) {
		test_x_zoom += ((ticrealanalogt-0x1F) << 6);
		test_y_zoom += ((ticrealanalogt-0x1F) << 6);
	}
	else if (ticrealanalogt < -0x1F) {
		test_x_zoom += ((ticrealanalogt+0x1F) << 6);
		test_y_zoom += ((ticrealanalogt+0x1F) << 6);
	}

	if (test_x_zoom < -0xFE00) {
		test_x_zoom = -0xFE00;
	}
	else if (test_x_zoom > 0xFFFFFF) {
		test_x_zoom = 0xFFFFFF;
	}

	if (test_y_zoom < -0xFE00) {
		test_y_zoom = -0xFE00;
	}
	else if (test_y_zoom > 0xFFFFFF) {
		test_y_zoom = 0xFFFFFF;
	}
}
*/


void NextScene()
{
	introScenes[currentScene]->stop(introScenes[currentScene]);

	currentScene++;
	//if (currentScene >= NUMSCENES) // We're done. How to signal?
	//	currentScene = currentScene-1;

	if (currentScene < NUMSCENES) {
		currentPhase = 0;
		introScenes[currentScene]->init(introScenes[currentScene]);
	}

	sceneFrameCount = 0;
	phaseFrameCount = 0;

	// A transition or something?
	StartTransition();
}

void NextPhase()
{
	currentPhase++;

	phaseFrameCount = 0;
}

void TIC_Text(storyscene_t *scene)
{
	if (scene->preTextDelay > 0)
	{
		scene->preTextDelay--;
		if (scene->preTextDelay <= 0)
			scene->drawText = 2; // Start drawing text

		return;
	}

	// If not at the end of the string, run the char counter
	if (scene->text[scene->textPos] != '\0')
	{
		scene->textCharDelayCounter--;
		if (scene->textCharDelayCounter <= 0)
		{
			scene->textCharDelayCounter = scene->textCharDelayTics;

			char c = scene->text[scene->textPos];
			if (c == 0x20) // Space
				scene->curTextPosX += menuFont.spaceWidthSize;
			else if (c == '\n') // Newline
			{
				scene->curTextPosX = 0;
				scene->curTextPosY += menuFont.verticalOffset;
			}
			else if (c >= menuFont.minChar && c <= menuFont.maxChar)
			{
				if (menuFont.fixedWidth)
					scene->curTextPosX += menuFont.fixedWidthSize;
				else
				{
					jagobj_t *charObj = menuFont.charCache[c - menuFont.lumpStartChar];
					if (charObj != NULL)
						scene->curTextPosX += charObj->width;
					else
						scene->curTextPosX += menuFont.fixedWidthSize;
				}
			}

			scene->textPos++;
			scene->drawText = 2;
		}
	}
	/*else // We're at the end
	{
		if (scene->postTextDelay > 0)
			scene->postTextDelay--;

		if (scene->postTextDelay <= 0)
			NextScene();
	}*/
}

void DrawText(storyscene_t *scene)
{
    // Common function to handle drawing the text, including how much of it to draw
	char c = scene->text[scene->textPos];

	if (c == '\0')
		return;

	// Constantly draw the last character in the string.
	if (scene->drawText > 0)
	{
		V_DrawChar_15bpp(&menuFont, scene->textBox.x + scene->curTextPosX, scene->textBox.y + scene->curTextPosY, c);
		scene->drawText--;
	}
}

void StartTransition(void) {
	transitionCount = 8;
	transitionDirection = 0;
	transitionInProgress = true;
}

void RunTransition(void) {
	if (transitionDirection == 0) {
		if (transitionCount >= 0) {
			TransitionOut();
			if (transitionCount == -1) {
				transitionDirection = 1;
			}
		}
	}
	else if (transitionCount < 8) {
		TransitionIn();
		if (transitionCount == 8) {
			transitionInProgress = false;
		}
	}
}

void TransitionOut(void) {
	transitionCount--;

	int transitionOutHeight = introScenes[currentScene]->transitionOutHeight;
	pixel_t *framebuffer;

	if (transitionCount >= 0) {
		framebuffer = I_FrameBuffer() + (transitionCount * 320);

		for (int section = transitionCount; section < transitionOutHeight; section += 8) {
			for (int x=0; x < 320; x += 4) {
				*framebuffer++ = 0;
				*framebuffer++ = 0;
				*framebuffer++ = 0;
				*framebuffer++ = 0;
			}
			framebuffer += (7 * 320);
		}
	}

	if (transitionCount < 7) {
		framebuffer = I_FrameBuffer() + ((transitionCount+1) * 320);

		for (int section = transitionCount+1; section < transitionOutHeight; section += 8) {
			for (int x=0; x < 320; x += 4) {
				*framebuffer++ = 0;
				*framebuffer++ = 0;
				*framebuffer++ = 0;
				*framebuffer++ = 0;
			}
			framebuffer += (7 * 320);
		}
	}
}

void TransitionIn(void) {
	transitionCount++;

	jagobj_t *background = introScenes[currentScene]->background;
	int backgroundX = introScenes[currentScene]->backgroundX;
	int backgroundY = introScenes[currentScene]->backgroundY;

	for (int section = transitionCount; section < background->height; section += 8) {
		DrawJagobj3_15bpp(
			background,
			backgroundX,
			backgroundY + section,
			0,
			section,
			background->width,
			1,
			320,
			true,
			I_FrameBuffer()
		);
	}

	if (transitionCount > 0) {
		for (int section = transitionCount-1; section < background->height; section += 8) {
			DrawJagobj3_15bpp(
				background,
				backgroundX,
				backgroundY + section,
				0,
				section,
				background->width,
				1,
				320,
				true,
				I_FrameBuffer()
			);
		}
	}
}

void Scene_1_Init(scene_1_t *scene)
{
	// Cache any graphics, etc.
	scene->scene.background = W_CacheLumpNum(scene->scene.picLump, PU_LEVEL);
	scene->scene.backgroundX = 0;
	scene->scene.backgroundY = 0;
	scene->satellite = W_CacheLumpNum(scene->picSatellite, PU_LEVEL);
	scene->satX = (24<<16);
	scene->satY = (32<<16);
	scene->satZ = (1<<16);

	scene->prevSatX[0] = 0;
	scene->prevSatX[1] = 0;
	scene->prevSatY[0] = 0;
	scene->prevSatY[1] = 0;
	scene->prevSatZ[0] = 0;
	scene->prevSatZ[1] = 0;
}

void Scene_1_Tick(scene_1_t *scene)
{
	TIC_Text(&scene->scene);

	scene->satX += (finesine(2048 - (sceneFrameCount<<1)) >> 2);
	scene->satY -= (finesine(2048 - (sceneFrameCount<<2)) >> 5);
	scene->satZ -= 96 - (sceneFrameCount >> 4) - (sceneFrameCount >> 5);
}

void Scene_1_Draw(scene_1_t *scene)
{
	if (sceneFrameCount <= 2) {
		// Draw background
		DrawJagobj3_15bpp(
			scene->scene.background,
			scene->scene.backgroundX,
			scene->scene.backgroundY,
			0,
			0,
			scene->scene.background->width,
			scene->scene.background->height,
			320,
			false,
			I_FrameBuffer()
		);
	}
	else {
		// Draw background fragment
		if (scene->prevSatZ[1] != 0) {
			int width = (int)FixedMul((fixed_t)(scene->satellite->width << FRACBITS), scene->prevSatZ[1]) >> FRACBITS;
			int height = (int)FixedMul((fixed_t)(scene->satellite->height << FRACBITS), scene->prevSatZ[1]) >> FRACBITS;

			DrawJagobj3_15bpp(
				scene->scene.background,
				scene->prevSatX[1],
				scene->prevSatY[1],
				scene->prevSatX[1],
				scene->prevSatY[1],
				width,
				height,
				320,
				false,
				I_FrameBuffer()
			);
		}
	}

	// Draw satellite drifting overtop
	DrawScaledJagobj_15bpp(
		scene->satellite,
		(scene->satX >> 16) & 0x1FF,
		(scene->satY >> 16) & 0xFF,
		scene->satZ,
		scene->satZ,
		true,
		I_FrameBuffer()
	);

	// Keep track of the previous two satellite positions for clearing it on new frames.
	scene->prevSatX[1] = scene->prevSatX[0];
	scene->prevSatY[1] = scene->prevSatY[0];
	scene->prevSatZ[1] = scene->prevSatZ[0];

	scene->prevSatX[0] = (scene->satX >> 16) & 0x1FF;
	scene->prevSatY[0] = (scene->satY >> 16) & 0xFF;
	scene->prevSatZ[0] = scene->satZ;

	DrawText(&scene->scene);
}

void Scene_1_Stop(scene_1_t *scene)
{
	// Free any resources
	Z_Free(scene->satellite);
	Z_Free(scene->scene.background);
}

void Scene_2_Init(scene_2_t *scene)
{
	// Cache any graphics, etc.
	scene->scene.background = W_CacheLumpNum(scene->scene.picLump, PU_LEVEL);
	scene->scene.backgroundX = 0;
	scene->scene.backgroundY = 0;
}

void Scene_2_Tick(scene_2_t *scene)
{
	TIC_Text(&scene->scene);
}

void Scene_2_Draw(scene_2_t *scene)
{
	if (sceneFrameCount <= 2) {
		// Draw background
		DrawJagobj3_15bpp(
			scene->scene.background,
			scene->scene.backgroundX,
			scene->scene.backgroundY,
			0,
			0,
			scene->scene.background->width,
			scene->scene.background->height,
			320,
			false,
			I_FrameBuffer()
		);
	}

	DrawText(&scene->scene);

}

void Scene_2_Stop(scene_2_t *scene)
{
	// Free any resources
	Z_Free(scene->scene.background);
}

void Scene_3_Init(scene_3_t *scene)
{
	// Cache any graphics, etc.
	scene->scene.background = W_CacheLumpNum(scene->scene.picLump, PU_LEVEL);
	scene->scene.backgroundX = 0;
	scene->scene.backgroundY = 0;
}

void Scene_3_Tick(scene_3_t *scene)
{
	TIC_Text(&scene->scene);
}

void Scene_3_Draw(scene_3_t *scene)
{
	if (sceneFrameCount <= 2) {
		// Draw background
		DrawJagobj3_15bpp(
			scene->scene.background,
			scene->scene.backgroundX,
			scene->scene.backgroundY,
			0,
			0,
			scene->scene.background->width,
			scene->scene.background->height,
			320,
			false,
			I_FrameBuffer()
		);
	}

	DrawText(&scene->scene);
}

void Scene_3_Stop(scene_3_t *scene)
{
	// Free any resources
	Z_Free(scene->scene.background);
}

void Scene_4_Init(scene_4_t *scene)
{
	// Cache any graphics, etc.
	scene->scene.background = W_CacheLumpNum(scene->scene.picLump, PU_LEVEL);
	scene->scene.backgroundX = 0;
	scene->scene.backgroundY = 0;
}

void Scene_4_Tick(scene_4_t *scene)
{
	if (sceneFrameCount == 24) {
		// Need to free memory to store the asteroid frames.
		Z_Free(scene->scene.background);
		scene->scene.background = NULL;

		// Load all 35 asteroid frames into RAM.
		for (int i = 0; i < 35; i++)
		{
			char ln[9];
			D_snprintf(ln, 8, "ROID00%02d", i);
			scene->asteroid[i] = W_CacheLumpName(ln, PU_LEVEL);
		}
	}

	if (sceneFrameCount & 1) {
		scene->asteroidIndex++;
		if (scene->asteroidIndex == 35) {
			scene->asteroidIndex = 0;
		}
	}

	TIC_Text(&scene->scene);
}

void Scene_4_Draw(scene_4_t *scene)
{
	if (sceneFrameCount <= 2) {
		// Draw background
		DrawJagobj3_15bpp(
			scene->scene.background,
			scene->scene.backgroundX,
			scene->scene.backgroundY,
			0,
			0,
			scene->scene.background->width,
			scene->scene.background->height,
			320,
			false,
			I_FrameBuffer()
		);
	}

	DrawText(&scene->scene);
}

void Scene_4_Stop(scene_4_t *scene)
{
	// Free any resources
	if (scene->scene.background != NULL) {
		Z_Free(scene->scene.background);
	}
	else {
		for (int i = 35-1; i >= 0; i--)
		{
			Z_Free(scene->asteroid[i]);
		}
	}
}

void Scene_5_Init(scene_5_t *scene)
{
	// Cache any graphics, etc.
	scene->scene.background = W_CacheLumpNum(scene->scene.picLump, PU_LEVEL);
	scene->scene.backgroundX = 0;
	scene->scene.backgroundY = 0;
	scene->radar = W_CacheLumpNum(scene->picRadar, PU_LEVEL);
	scene->sweep = W_CacheLumpNum(scene->picSweep, PU_LEVEL);

	scene->radWave[0] = W_CacheLumpName("RADWAVE1", PU_LEVEL);
	scene->radWave[1] = W_CacheLumpName("RADWAVE2", PU_LEVEL);
	scene->radWave[2] = W_CacheLumpName("RADWAVE3", PU_LEVEL);
	scene->radWave[3] = W_CacheLumpName("RADWAVE4", PU_LEVEL);
	scene->actBar[0] = W_CacheLumpName("ACTBAR1", PU_LEVEL);
	scene->actBar[1] = W_CacheLumpName("ACTBAR2", PU_LEVEL);
	scene->actBar[2] = W_CacheLumpName("ACTBAR3", PU_LEVEL);
	scene->actBar[3] = W_CacheLumpName("ACTBAR4", PU_LEVEL);
	scene->warning = W_CacheLumpName("ANOMALY", PU_LEVEL);
	scene->blip = W_CacheLumpName("RADBLIP", PU_LEVEL);

	scene->blipShift = 6; // <= 5 means it draws

	S_StartSound(NULL, sfx_s3k_5f);
}

void Scene_5_Tick(scene_5_t *scene)
{
	TIC_Text(&scene->scene);

	if ((sceneFrameCount & 7) == 0)
	{
		scene->animationCounter++;
		if (scene->animationCounter > 3)
			scene->animationCounter = 0;
	}

	if (scene->blipShift < 6 && (phaseFrameCount & 3) == 0)
		scene->blipShift++;

	if (scene->blipShift >= 6)
		scene->blipShift = 6;

	if (currentPhase >= 1 && (phaseFrameCount & 127) == 0) // Just a test
		scene->blipShift = 0;

	if (currentPhase == 1 && phaseFrameCount == 0)
	{
		jagobj_t *swap = scene->scene.background;
		scene->scene.background = scene->radar;
		scene->radar = swap;
		scene->scene.backgroundX = 160 - (scene->scene.background->width >> 1);
		scene->scene.backgroundY = 128 - scene->scene.background->height;

		scene->sweep = W_CacheLumpNum(scene->picSweep, PU_LEVEL);
	}
}

void Scene_5_Draw(scene_5_t *scene)
{
	uint8_t displacement[] = { 2, 1, 3, 0 };

	if (sceneFrameCount < 26) {
		if (sceneFrameCount <= 2 || sceneFrameCount >= 24) {
			// Draw background
			DrawJagobj3_15bpp(
				scene->scene.background,
				scene->scene.backgroundX,
				scene->scene.backgroundY,
				0,
				0,
				scene->scene.background->width,
				scene->scene.background->height,
				320,
				false,
				I_FrameBuffer()
			);
		}
		else {
			// Draw background shaking
			int verticalShift = displacement[(int)(sceneFrameCount & 3)];
			int bottomEdge = scene->scene.background->height - displacement[(int)(sceneFrameCount & 3)];

			DrawJagobj3_15bpp(
				scene->scene.background,
				scene->scene.backgroundX,
				scene->scene.backgroundY,
				0,
				verticalShift,
				scene->scene.background->width,
				bottomEdge,
				320,
				false,
				I_FrameBuffer()
			);
			
			pixel_t *framebuffer = I_FrameBuffer() + (bottomEdge * 320);

			for (int i = 0; i < verticalShift; i++) {
				for (int x=0; x < 320; x += 4) {
					*framebuffer++ = 0;
					*framebuffer++ = 0;
					*framebuffer++ = 0;
					*framebuffer++ = 0;
				}
			}
		}
	}
	else if (currentPhase == 1) {
		if (phaseFrameCount <= 2) {
			if (phaseFrameCount == 0)
			{
				StartTransition();
			}
			else {
				// Clear to black
				pixel_t *framebuffer = I_FrameBuffer();

				for (int i = 0; i < (scene->scene.backgroundY + scene->scene.background->height); i++) {
					// Top
					for (int x=0; x < 320; x += 4) {
						*framebuffer++ = 0;
						*framebuffer++ = 0;
						*framebuffer++ = 0;
						*framebuffer++ = 0;
					}
				}

				// Draw radar screen
				DrawJagobj3_15bpp(
					scene->scene.background,
					scene->scene.backgroundX,
					scene->scene.backgroundY,
					0,
					0,
					scene->scene.background->width,
					scene->scene.background->height,
					320,
					true,
					I_FrameBuffer()
				);
			}
		}
		else {
			angle_t sweepAngle = 0x1FFF - (((phaseFrameCount << 6) + (phaseFrameCount << 5)) & 0x1FFF);
			// Draw radar sweep
			DrawRotatedJagobj_15bpp(
				scene->sweep,
				scene->scene.backgroundX + (scene->scene.background->width >> 1) - 1,
				scene->scene.backgroundY + (scene->scene.background->height >> 1),
				sweepAngle,
					false,
				true,
					I_FrameBuffer()
				);

			// Draw radar screen overlay
			int overlayWidth = scene->scene.background->width;
			int overlayHeight = scene->scene.background->height;
			int srcX = 0;
			int srcY = 0;

			if (sweepAngle < 0x800) {
				overlayWidth >>= 1;
				srcX = overlayWidth;

				if (sweepAngle == 0) {
					overlayHeight >>= 1;
					srcY = overlayHeight;
				}
			}
			else if (sweepAngle < 0x1000) {
				overlayHeight >>= 1;

				if (sweepAngle == 0x800) {
					overlayWidth >>= 1;
					srcX = overlayWidth;
				}
			}
			else if (sweepAngle < 0x1800) {
				overlayWidth >>= 1;

				if (sweepAngle == 0x1000) {
					overlayHeight >>= 1;
				}
			}
			else /*if (sweepAngle < 0x2000)*/ {
				overlayHeight >>= 1;
				srcY = overlayHeight;

				if (sweepAngle == 0x1800) {
					overlayWidth >>= 1;
				}
			}

			const int radarPosX = scene->scene.backgroundX;
			const int radarPosY = scene->scene.backgroundY;

			DrawJagobj3_15bpp(
				scene->scene.background,
				scene->scene.backgroundX + srcX,
				scene->scene.backgroundY + srcY,
				srcX,
				srcY,
				overlayWidth,
				overlayHeight,
				320,
				true,
				I_FrameBuffer()
			);

			DrawJagobj3_15bpp(
				scene->actBar[scene->animationCounter],
				radarPosX + 5,
				radarPosY + 108,
				0, 0,
				scene->actBar[scene->animationCounter]->width,
				scene->actBar[scene->animationCounter]->height,
				320,
				false,
				I_FrameBuffer()
			);

			DrawJagobj3_15bpp(
				scene->radWave[scene->animationCounter],
				radarPosX + 108,
				radarPosY + 3,
				0, 0,
				scene->radWave[scene->animationCounter]->width,
				scene->radWave[scene->animationCounter]->height,
				320,
				false,
				I_FrameBuffer()
			);

			DrawJagobj3_15bpp(
				scene->warning,
				radarPosX + 109,
				radarPosY + 108,
				0, 0,
				scene->warning->width,
				scene->warning->height,
				320,
				false,
				I_FrameBuffer()
			);

			if (scene->blipShift <= 5)
			{
				DrawJagobj3_15bpp(
					scene->scene.background,
					scene->scene.backgroundX + 96,
					scene->scene.backgroundY + 32,
					96,
					32,
					scene->blip->width,
					scene->blip->height,
					320,
					true,
					I_FrameBuffer()
				);

				DrawScaledJagobj_15bppBlend(
					scene->blip,
					radarPosX + 96,
					radarPosY + 32,
					FRACUNIT, FRACUNIT,
					scene->blipShift, true, I_FrameBuffer()
				);
			}
		}
	}

	DrawText(&scene->scene);
}

void Scene_5_Stop(scene_5_t *scene)
{
	// Free any resources
	Z_Free(scene->scene.background);
	Z_Free(scene->sweep);
	Z_Free(scene->radar);
}

void Scene_6_Init(scene_6_t *scene)
{
	// Cache any graphics, etc.
	scene->scene.background = W_CacheLumpNum(scene->scene.picLump, PU_LEVEL);
	scene->scene.backgroundX = 0;
	scene->scene.backgroundY = 0;

	scene->flicker[0] = W_CacheLumpName("THKFLIK1", PU_LEVEL);
	scene->flicker[1] = W_CacheLumpName("THKFLIK2", PU_LEVEL);
	scene->flicker[2] = W_CacheLumpName("THKFLIK3", PU_LEVEL);
	scene->stache[0] = W_CacheLumpName("THKTALK1", PU_LEVEL);
	scene->stache[1] = W_CacheLumpName("THKTALK2", PU_LEVEL);
	scene->metal = W_CacheLumpName("THKMETAL", PU_LEVEL);
	scene->noflash = W_CacheLumpName("THFLASH", PU_LEVEL);
}

void Scene_6_Tick(scene_6_t *scene)
{
	int16_t lastTextPos = scene->scene.textPos;
	TIC_Text(&scene->scene);

	if (scene->scene.textPos != lastTextPos)
	{
		if (scene->scene.text[scene->scene.textPos] == '\xA7')
		{
			scene->talking = !scene->talking;
			scene->stacheCounter = 0;
		}
	}

	if ((sceneFrameCount & 7) == 0)
		scene->stacheCounter++;

	if ((sceneFrameCount & 3) == 0)
	{
		scene->flickerCounter++;
		if (scene->flickerCounter > 2)
			scene->flickerCounter = 0;
	}
}

void Scene_6_Draw(scene_6_t *scene)
{
	DrawJagobj3_15bpp(
		scene->scene.background,
		scene->scene.backgroundX,
		scene->scene.backgroundY,
		0,
		0,
		scene->scene.background->width,
		scene->scene.background->height,
		320,
		false,
		I_FrameBuffer()
	);

	if (sceneFrameCount & 1)
		DrawJagobj3_15bpp(
			scene->noflash,
			105,
			55,
			0,
			0,
			scene->noflash->width,
			scene->noflash->height,
			320,
			false,
			I_FrameBuffer()
		);

	jagobj_t *flicker = scene->flicker[scene->flickerCounter];
	DrawJagobj3_15bpp(
		flicker,
		50,
		6,
		0,
		0,
		flicker->width,
		flicker->height,
		320,
		true,
		I_FrameBuffer()
	);

	int metalY = scene->metalY + (finesine(sceneFrameCount<<5) >> 15);

	DrawJagobj3_15bpp(
		scene->metal,
		scene->metalX,
		metalY,
		0,
		0,
		scene->metal->width,
		scene->metal->height,
		320,
		true,
		I_FrameBuffer()
	);

	jagobj_t *stache = (scene->talking && (scene->stacheCounter & 1)) ? scene->stache[1] : scene->stache[0];
	DrawJagobj3_15bpp(
		stache,
		55,
		70,
		0,
		0,
		stache->width,
		stache->height,
		320,
		true,
		I_FrameBuffer()
	);

	DrawText(&scene->scene);
}

void Scene_6_Stop(scene_6_t *scene)
{
	// Free any resources
	Z_Free(scene->scene.background);
	Z_Free(scene->flicker[0]);
	Z_Free(scene->flicker[1]);
	Z_Free(scene->flicker[2]);
	Z_Free(scene->stache[0]);
	Z_Free(scene->stache[1]);
	Z_Free(scene->metal);
	Z_Free(scene->noflash);
}

void Scene_7_Init(scene_7_t *scene)
{
	scene->scene.background = W_CacheLumpNum(scene->scene.picLump, PU_LEVEL);
	scene->sleep[0] = W_CacheLumpName("SLEEP2", PU_LEVEL);
	scene->sleep[1] = W_CacheLumpName("SLEEP3", PU_LEVEL);
	scene->sleep[2] = W_CacheLumpName("SLEEP4", PU_LEVEL);

	scene->sleepCounter = 0;
}

void Scene_7_Tick(scene_7_t *scene)
{
	TIC_Text(&scene->scene);

	if (currentPhase == 0)
	{
		if (phaseFrameCount >= 4 * 60)
		{
			NextPhase();
			Z_Free(scene->scene.background);
			scene->scene.background = W_CacheLumpName("SLEEP1D", PU_LEVEL);
		}
	}
	else if (currentPhase == 1) // Fade to dark
	{
		if (phaseFrameCount >= 30)
			NextPhase();
	}
	else if (currentPhase == 2) // Surprised
	{
		if ((sceneFrameCount & 31) == 0)
			scene->sleepCounter++;

		if (scene->sleepCounter >= 3)
			NextPhase();
	}
}

void Scene_7_Draw(scene_7_t *scene)
{
	if (currentPhase == 0)
	{
		DrawJagobj3_15bpp(
				scene->scene.background,
				scene->scene.backgroundX,
				scene->scene.backgroundY,
				0,
				0,
				scene->scene.background->width,
				scene->scene.background->height,
				320,
				false,
				I_FrameBuffer()
			);
	}
	else if (currentPhase == 1) // Fade to dark.. we can just keep redrawing!
	{
		DrawJagobj_15bppBlendNomask(
			scene->scene.background,
			scene->scene.backgroundX,
			scene->scene.backgroundY,
			4,
			I_FrameBuffer());
	}
	else if (currentPhase == 2) // Surprised
	{
		VINT x, y;
		if (scene->sleepCounter == 0)
		{
			x = 126;
			y = 4;
		}
		else if (scene->sleepCounter == 1)
		{
			x = 94;
			y = 1;
		}
		else // 2
		{
			x = 94;
			y = 1;
		}

		jagobj_t *gfx = scene->sleep[scene->sleepCounter];
		DrawJagobj3_15bpp(gfx, x, y, 0, 0,
			gfx->width, gfx->height,
			320, false, I_FrameBuffer());
	}

	DrawText(&scene->scene);
}

void Scene_7_Stop(scene_7_t *scene)
{
	Z_Free(scene->scene.background);
	Z_Free(scene->sleep[0]);
	Z_Free(scene->sleep[1]);
	Z_Free(scene->sleep[2]);
}

void Scene_8_Init(scene_8_t *scene)
{
	scene->scene.background = W_CacheLumpNum(scene->scene.picLump, PU_LEVEL);
	scene->scene.backgroundX = 68;
	scene->scene.backgroundY = 0;

	scene->crosshair = W_CacheLumpName("TGCROSSH", PU_LEVEL);
	scene->targetAcquired = W_CacheLumpName("TGACQ", PU_LEVEL);
	scene->gfzMountainText = W_CacheLumpName("TGGFZ", PU_LEVEL);
	scene->compass = W_CacheLumpName("TGCOMPAS", PU_LEVEL);
	scene->bar = W_CacheLumpName("TGBAR", PU_LEVEL);
	scene->bottomLeftInd[0] = W_CacheLumpName("TGBAR1", PU_LEVEL);
	scene->bottomLeftInd[1] = W_CacheLumpName("TGBAR2", PU_LEVEL);
	scene->bottomLeftInd[2] = W_CacheLumpName("TGBAR3", PU_LEVEL);
	scene->bottomLeftInd[3] = W_CacheLumpName("TGBAR4", PU_LEVEL);
	scene->gfzWidth = 0;
	scene->acquiredWidth = 0;
}

void Scene_8_Tick(scene_8_t *scene)
{
	TIC_Text(&scene->scene);

	if ((sceneFrameCount & 7) == 0)
	{
		scene->barAnim++;
		if (scene->barAnim > 3)
			scene->barAnim = 0;		
	}

	if (currentPhase == 0 && phaseFrameCount > 120)
	{
		// Play sound
		NextPhase();
	}
	else if (currentPhase == 1)
	{
		scene->acquiredWidth++;
		if (scene->acquiredWidth > scene->targetAcquired->width)
			scene->acquiredWidth = scene->targetAcquired->width;

		if (phaseFrameCount > scene->targetAcquired->width * 2)
			NextPhase();
	}
	else if (currentPhase == 2)
	{
		scene->gfzWidth++;
		if (scene->gfzWidth > scene->gfzMountainText->width)
			scene->gfzWidth = scene->gfzMountainText->width;

		if (phaseFrameCount > scene->gfzMountainText->width * 2)
		{
			NextPhase();
			scene->crosshairScale = 8*FRACUNIT;
		}
	}
	else if (currentPhase == 3)
	{
		if (scene->crosshairScale > FRACUNIT)
			scene->crosshairScale -= FRACUNIT;

		if (scene->crosshairScale <= FRACUNIT)
		{
			scene->crosshairScale = FRACUNIT;
			NextPhase();
		}
	}
	else if (currentPhase == 4)
	{
		// Things!
	}
}

void Scene_8_Draw(scene_8_t *scene)
{
	DrawJagobj3_15bpp(
		scene->scene.background,
		scene->scene.backgroundX,
		scene->scene.backgroundY,
		0,
		0,
		scene->scene.background->width,
		scene->scene.background->height,
		320,
		false,
		I_FrameBuffer()
	);

	if (currentPhase > 0)
	{
		if (scene->acquiredWidth > 0)
		{
			DrawJagobj3_15bpp(
				scene->targetAcquired,
				scene->scene.backgroundX + 2,
				scene->scene.backgroundY + 4,
				0, 0,
				scene->acquiredWidth,
				scene->targetAcquired->height,
				320,
				true,
				I_FrameBuffer()
			);
		}

		if (scene->gfzWidth > 0)
		{
			DrawJagobj3_15bpp(
				scene->gfzMountainText,
				scene->scene.backgroundX + 2,
				scene->scene.backgroundY + 10,
				0, 0,
				scene->gfzWidth,
				scene->gfzMountainText->height,
				320,
				true,
				I_FrameBuffer()
			);
		}

		DrawJagobj3_15bpp(
			scene->compass,
			scene->scene.backgroundX + 155,
			scene->scene.backgroundY + 98,
			0, 0,
			scene->compass->width,
			scene->compass->height,
			320,
			true,
			I_FrameBuffer()
		);
		
		DrawJagobj3_15bpp(
			scene->bottomLeftInd[scene->barAnim],
			scene->scene.backgroundX + 2,
			scene->scene.backgroundY + 114,
			0, 0,
			scene->bottomLeftInd[scene->barAnim]->width,
			scene->bottomLeftInd[scene->barAnim]->height,
			320,
			true,
			I_FrameBuffer()
		);

		if (currentPhase >= 3)
		{
			if (currentPhase >= 4)
			{
				if ((phaseFrameCount & 15) < 8)
				{
					DrawScaledJagobj_15bppBlendWithOrigin(
						scene->crosshair,
						scene->scene.backgroundX + (scene->scene.background->width / 2),
						scene->scene.backgroundY + (scene->scene.background->height / 2),
						scene->crosshair->width / 2,
						scene->crosshair->height / 2,
						scene->crosshairScale,
						scene->crosshairScale,
						1,
						true,
						I_FrameBuffer()
					);
				}
				else
				{
					DrawJagobj3_15bpp(
						scene->scene.background,
						scene->scene.backgroundX + 68,
						scene->scene.backgroundY + 39,
						68, 39,
						scene->crosshair->width,
						scene->crosshair->height,
						320,
						false,
						I_FrameBuffer()
					);
				}
			}
			else
				DrawScaledJagobj_15bppBlendWithOrigin(
					scene->crosshair,
					scene->scene.backgroundX + (scene->scene.background->width / 2),
					scene->scene.backgroundY + (scene->scene.background->height / 2),
					scene->crosshair->width / 2,
					scene->crosshair->height / 2,
					scene->crosshairScale,
					scene->crosshairScale,
					1,
					true,
					I_FrameBuffer()
				);
		}
	}

	DrawText(&scene->scene);
}

void Scene_8_Stop(scene_8_t *scene)
{
	Z_Free(scene->scene.background);
	Z_Free(scene->crosshair);
	Z_Free(scene->targetAcquired);
	Z_Free(scene->gfzMountainText);
	Z_Free(scene->compass);
	Z_Free(scene->bar);
	Z_Free(scene->bottomLeftInd[0]);
	Z_Free(scene->bottomLeftInd[1]);
	Z_Free(scene->bottomLeftInd[2]);
	Z_Free(scene->bottomLeftInd[3]);
}

void Scene_9_Init(scene_9_t *scene)
{
	scene->scene.background = W_CacheLumpNum(scene->scene.picLump, PU_LEVEL);
	scene->scene.backgroundX = 0;
	scene->scene.backgroundY = 0;

	scene->sonic = W_CacheLumpName("SPRSONIC", PU_LEVEL);
	scene->aura = W_CacheLumpName("SPRAURA", PU_LEVEL);

	for (int i = 1; i <= 14; i++)
	{
		char ln[12];
		D_snprintf(ln, 10, "SPRZ%02d", i);
		scene->zoom[i - 1] = W_CacheLumpName(ln, PU_LEVEL);
	}

	scene->zoomStartDelay = 2*TICRATE;
	scene->zoomChangeInterval = 4;
	scene->sonicScale = 0;
	scene->auraScale = 0;

	scene->sonicPosX = 234 << FRACBITS;
	scene->sonicPosY = 110 << FRACBITS;
	scene->sonicStep = 0;
}

void Scene_9_Tick(scene_9_t *scene)
{
	TIC_Text(&scene->scene);

	if (scene->zoomStartDelay)
	{
		scene->zoomStartDelay--;
		return;
	}

	if (scene->zoomCurIndex < 14)
	{
		if (scene->zoomChangeInterval)
		{
			if (--scene->zoomChangeInterval <= 0)
			{
				scene->zoomChangeInterval = 2;
				scene->zoomCurIndex++;

				if (scene->zoomCurIndex >= 14)
					S_StartSound(NULL, sfx_s3k_b1);
			}
		}

		return;
	}

	if (scene->sonicStep == 0)
	{
		scene->auraScale += 1 << 12;
		scene->sonicScale = scene->auraScale;
		scene->sonicPosX -= 4 << 16; 
		scene->sonicPosY += 3 << 15;

		if (scene->sonicScale > FRACUNIT)
		{
			scene->sonicScale = FRACUNIT;
			scene->auraScale = FRACUNIT;
			scene->sonicPosX += 4 << 16; 
			scene->sonicPosY -= 3 << 15;
			scene->sonicStep = 1;
			scene->sonicCounter = 0;
		}
	}
	else if (scene->sonicStep == 1)
	{
		scene->sonicCounter++;
		scene->sonicPosX -= 1 << 16;

		if (scene->sonicCounter > 4)
		{
			scene->sonicStep = 2;
			scene->sonicCounter = 0;
		}
	}
	else if (scene->sonicStep == 2)
	{
		scene->sonicPosX -= 1 << 16;
		scene->sonicCounter++;

		if (scene->sonicCounter > 5)
		{
			scene->sonicCounter = 0;
			scene->sonicStep = 3;
		}
	}
	else if (scene->sonicStep == 3)
	{
		scene->sonicScale += 10 << 10;
		scene->sonicPosX -= 8 << 16; 
		scene->sonicPosY += 5 << 15;
		scene->sonicCounter++;

		if (scene->sonicCounter > 10)
			scene->sonicStep = 4; // No drawing of Sonic
	}
}

void Scene_9_Draw(scene_9_t *scene)
{
	DrawJagobj3_15bpp(
		scene->scene.background,
		scene->scene.backgroundX,
		scene->scene.backgroundY,
		0,
		0,
		scene->scene.background->width,
		scene->scene.background->height,
		320,
		false,
		I_FrameBuffer()
	);

	if (scene->zoomStartDelay <= 0 && scene->zoomCurIndex < 14)
	{
		const int drawX = 182;
		const int drawY = 62;

		jagobj_t *zoom = scene->zoom[scene->zoomCurIndex];
		DrawJagobj3_15bpp(
			zoom,
			drawX + scene->zoomCoords[scene->zoomCurIndex].x,
			drawY + scene->zoomCoords[scene->zoomCurIndex].y,
			0,
			0,
			zoom->width,
			zoom->height,
			320,
			true,
			I_FrameBuffer()
		);
	}

	if (scene->zoomCurIndex >= 14 && scene->sonicStep < 4)
	{
		const coord_t auraPos = { 234, 110 };

		// Spring OUT!
		if (scene->sonicStep == 2)
		{
			if (scene->sonicCounter < 5)
				DrawScaledJagobj_15bppBlendWithOrigin(
					scene->aura, auraPos.x, auraPos.y, 93, 92, scene->auraScale, scene->auraScale, scene->sonicCounter + 1, true, I_FrameBuffer());
		}
		else if (scene->sonicStep < 2)
		{
			DrawScaledJagobj_15bppBlendWithOrigin(
				scene->aura, auraPos.x, auraPos.y, 93, 92, scene->auraScale, scene->auraScale, 1, true, I_FrameBuffer());
		}

		DrawScaledJagobj_15bppBlendWithOrigin(
			scene->sonic, scene->sonicPosX >> FRACBITS, scene->sonicPosY >> FRACBITS, 64, 126, scene->sonicScale, scene->sonicScale, 0, true, I_FrameBuffer());
	}

	DrawText(&scene->scene);
}

void Scene_9_Stop(scene_9_t *scene)
{
	Z_Free(scene->scene.background);
	Z_Free(scene->sonic);
	Z_Free(scene->aura);

	for (int i = 1; i <= 14; i++)
		Z_Free(scene->zoom[i - 1]);
}

void Scene_10_Init(scene_10_t *scene)
{

}

void Scene_10_Tick(scene_10_t *scene)
{

}

void Scene_10_Draw(scene_10_t *scene)
{

}

void Scene_10_Stop(scene_10_t *scene)
{

}

void Scene_11_Init(scene_10_t *scene)
{

}

void Scene_11_Tick(scene_10_t *scene)
{

}

void Scene_11_Draw(scene_10_t *scene)
{

}

void Scene_11_Stop(scene_10_t *scene)
{
	
}

void Scene_12_Init(scene_10_t *scene)
{

}

void Scene_12_Tick(scene_10_t *scene)
{

}

void Scene_12_Draw(scene_10_t *scene)
{

}

void Scene_12_Stop(scene_10_t *scene)
{
	
}

void Scene_13_Init(scene_10_t *scene)
{

}

void Scene_13_Tick(scene_10_t *scene)
{

}

void Scene_13_Draw(scene_10_t *scene)
{

}

void Scene_13_Stop(scene_10_t *scene)
{
	
}

void Scene_14_Init(scene_10_t *scene)
{

}

void Scene_14_Tick(scene_10_t *scene)
{

}

void Scene_14_Draw(scene_10_t *scene)
{

}

void Scene_14_Stop(scene_10_t *scene)
{
	
}

void Scene_15_Init(scene_10_t *scene)
{

}

void Scene_15_Tick(scene_10_t *scene)
{

}

void Scene_15_Draw(scene_10_t *scene)
{

}

void Scene_15_Stop(scene_10_t *scene)
{
	
}

void Scene_16_Init(scene_10_t *scene)
{

}

void Scene_16_Tick(scene_10_t *scene)
{

}

void Scene_16_Draw(scene_10_t *scene)
{

}

void Scene_16_Stop(scene_10_t *scene)
{
	
}

const char *intro1text =
"Two months had passed since Dr. Eggman\n"
"tried to take over the world using his\n"
"Ring Satellite.";

const char *intro2text =
"As it was about to drain the rings\n"
"away from the planet, Sonic burst into\n"
"the control room and for what he thought\n"
"would be the last time,\xB4 defeated\n"
"Dr. Eggman.";

const char *intro3text =
"What Sonic, Tails, and Knuckles had\n"
"not anticipated was that Eggman would\n"
"return,\xB8 bringing an all new threat.";

const char *intro4text =
"\xA8""About every five years, a strange asteroid\n"
"hovers around the planet.\xBF It suddenly\n"
"appears from nowhere, circles around, and\n"
"\xB6- just as mysteriously as it arrives -\xB6\n"
"vanishes after only one week.\xBF\n"
"No one knows why it appears, or how.";

const char *intro5text = 
"\"Curses!\" Eggman yelled. \"That hedgehog\n"
"and his ridiculous friends will pay\n"
"dearly for this!\"\xC8 Just then his scanner\n"
"blipped as the Black Rock made its\n"
"appearance from nowhere. Eggman looked at\n"
"the screen, and just shrugged it off.";

const char *intro6text =
"It was hours later\n"
"that he had an\n"
"idea. \xA7\"The Black\n"
"Rock has a large\n"
"amount of energy\n"
"within it\xA7...\xA7\n"
"If I can somehow\n"
"harness this\xA7, \xA7I\n"
"can turn it into\n"
"the ultimate\n"
"battle station\xA7...\xA7\n"
"And every last\n"
"person will be\n"
"begging for mercy,\n"
"including Sonic!\"\xA7";

const char *intro7text =
"\xA8\nBefore beginning his scheme,\n"
"Eggman decided to give Sonic\n"
"a reunion party...";

const char *intro8text =
"\xA5\"PRE-""\xB6""PARING-""\xB6""TO-""\xB4""FIRE-\xB6IN-""\xB6""15-""\xB6""SECONDS!\"\xA8\xB8\n"
"his targeting system crackled\n"
"robotically down the com-link. \xBF\xA7\"Good!\"\xA8\xB8\n"
"Eggman sat back in his eggmobile and\n"
"began to count down as he saw the\n"
"Greenflower mountain on the monitor.";

const char *intro9text =
"\xA5\"10...\xD2""9...\xD2""8...\"\xA8\xD2\n"
"Meanwhile, Sonic was tearing across the\n"
"zones. Everything became a blur as he\n"
"ran up slopes, skimmed over water,\n"
"and catapulted himself off rocks with\n"
"his phenomenal speed.";

const char *intro10text =
"\xA5\"6...\xD2""5...\xD2""4...\"\xA8\xD2\n"
"Sonic knew he was getting closer to the\n"
"zone, and pushed himself harder.\xB4 Finally,\n"
"the mountain appeared on the horizon.\xD2\xD2\n"
"\xA5\"3...\xD2""2...\xD2""1...\xD2""Zero.\"";

const char *intro11text =
"Greenflower Mountain was no more.\xC4\n"
"Sonic was now staring down a massive\n"
"visage of Eggman looking back at him.\n"
"The natural beauty of the zone\n"
"had been obliterated.";

const char *intro12text =
"\xA7\"You're not\n"
"quite as gone\n"
"as we thought,\n"
"huh?\xBF Are you\n"
"going to tell\n"
"us your plan as\n"
"usual or will I\n"
"\xA8\xB4'have to work\n"
"it out'\xA7 or\n"
"something?\"\xD2\xD2";

const char *intro13text =
"\"We'll see\xAA...\xA7\xBF let's give you a quick warm\n"
"up, Sonic!\xA6\xC4 JETTYSYNS!\xA7\xBD Open fire!\"";

const char *intro14text =
"Eggman took this\n"
"as his cue and\n"
"blasted off,\n"
"leaving Sonic\n"
"and Tails behind.\xB6\n"
"Tails looked at\n"
"the once-perfect\n"
"mountainside\n"
"with a grim face\n"
"and sighed.\xC6\n"
"\xA7\"Now\xB6 what do we\n"
"do?\",\xA9 he asked.";

const char *intro15text =
"\xA7\"Easy!\xBF We go\n"
"find Eggman\n"
"and stop his\n"
"latest\n"
"insane plan.\xBF\n"
"Just like\n"
"we've always\n"
"done,\xBA right?\xD2\n\n"
"\xAE...\xA9\xD2\n\n"
"\"Tails, what\n"
"\xAA*ARE*\xA9 you\n"
"doing?\"";

const char *intro16text =
"\xA8\"I'm just finding what mission obje\xAC\xB1...\xBF\n"
"\xA6""a-\xB8""ha!\xBF Here it is!\xA8\xBF This will only give us\n"
"the robot's primary objective.\xBF It says\xAC\xB1...\"\n"
"\xD2\xA3\x83"
"* LOCATE  AND  RETRIEVE:  CHAOS  EMERALDS *"
"\xBF\n"
"*  CLOSEST  LOCATION:  GREENFLOWER  ZONE  *"
"\x80\n\xA9\xD2\xD2"
"\"All right, then\xAF... \xD2\xD2\xA7let's go!\"";

void BuildScenes()
{
	int i = 0;

	scene_1_t *scene1 = Z_Calloc(sizeof(*scene1), PU_STATIC);
	scene1->scene.transitionOutHeight = 204;
	scene1->scene.picLump = W_GetNumForName("PLANET");
	scene1->picSatellite = W_GetNumForName("SATELLIT");
	scene1->scene.text = intro1text;
	scene1->scene.textCharDelayTics = scene1->scene.textCharDelayCounter = 4;
	scene1->scene.preTextDelay = TICRATE;
	scene1->scene.postTextDelay = 2*TICRATE;
	scene1->scene.textBox.x = 16;
	scene1->scene.textBox.y = 128 + 16;
	scene1->scene.textBox.width = 320 - 32 - 32;
	scene1->scene.textBox.height = 224 - 16 - scene1->scene.textBox.y;
	scene1->scene.init = (void(*)(storyscene_t *))Scene_1_Init;
	scene1->scene.tic = (void(*)(storyscene_t *))Scene_1_Tick;
	scene1->scene.draw = (void(*)(storyscene_t *))Scene_1_Draw;
	scene1->scene.stop = (void(*)(storyscene_t *))Scene_1_Stop;
	introScenes[i++] = (storyscene_t*)scene1;

	scene_2_t *scene2 = Z_Calloc(sizeof(*scene2), PU_STATIC);
	scene2->scene.transitionOutHeight = 204;
	scene2->scene.picLump = W_GetNumForName("RSBG");
	scene2->scene.text = intro2text;
	scene2->scene.textCharDelayTics = scene2->scene.textCharDelayCounter = 4;
	scene2->scene.preTextDelay = TICRATE;
	scene2->scene.postTextDelay = 2*TICRATE;
	scene2->scene.textBox.x = 16;
	scene2->scene.textBox.y = 128 + 16;
	scene2->scene.textBox.width = 320 - 32 - 32;
	scene2->scene.textBox.height = 224 - 16 - scene2->scene.textBox.y;
	scene2->scene.init = (void(*)(storyscene_t *))Scene_2_Init;
	scene2->scene.tic = (void(*)(storyscene_t *))Scene_2_Tick;
	scene2->scene.draw = (void(*)(storyscene_t *))Scene_2_Draw;
	scene2->scene.stop = (void(*)(storyscene_t *))Scene_2_Stop;
	introScenes[i++] = (storyscene_t*)scene2;

	scene_3_t *scene3 = Z_Calloc(sizeof(*scene3), PU_STATIC);
	scene3->scene.transitionOutHeight = 204;
	scene3->scene.picLump = W_GetNumForName("PLANET2");
	scene3->scene.text = intro3text;
	scene3->scene.textCharDelayTics = scene3->scene.textCharDelayCounter = 4;
	scene3->scene.preTextDelay = TICRATE;
	scene3->scene.postTextDelay = 2*TICRATE;
	scene3->scene.textBox.x = 16;
	scene3->scene.textBox.y = 128 + 16;
	scene3->scene.textBox.width = 320 - 32 - 32;
	scene3->scene.textBox.height = 224 - 16 - scene3->scene.textBox.y;
	scene3->scene.init = (void(*)(storyscene_t *))Scene_3_Init;
	scene3->scene.tic = (void(*)(storyscene_t *))Scene_3_Tick;
	scene3->scene.draw = (void(*)(storyscene_t *))Scene_3_Draw;
	scene3->scene.stop = (void(*)(storyscene_t *))Scene_3_Stop;
	introScenes[i++] = (storyscene_t*)scene3;

	scene_4_t *scene4 = Z_Calloc(sizeof(*scene4), PU_STATIC);
	scene4->scene.transitionOutHeight = 204;
	scene4->scene.picLump = W_GetNumForName("ROCKBG");
	scene4->scene.text = intro4text;
	scene4->scene.textCharDelayTics = scene4->scene.textCharDelayCounter = 4;
	scene4->scene.preTextDelay = TICRATE;
	scene4->scene.postTextDelay = 2*TICRATE;
	scene4->scene.textBox.x = 16;
	scene4->scene.textBox.y = 128 + 16;
	scene4->scene.textBox.width = 320 - 32 - 32;
	scene4->scene.textBox.height = 224 - 16 - scene4->scene.textBox.y;
	scene4->scene.init = (void(*)(storyscene_t *))Scene_4_Init;
	scene4->scene.tic = (void(*)(storyscene_t *))Scene_4_Tick;
	scene4->scene.draw = (void(*)(storyscene_t *))Scene_4_Draw;
	scene4->scene.stop = (void(*)(storyscene_t *))Scene_4_Stop;
	introScenes[i++] = (storyscene_t*)scene4;

	scene_5_t *scene5 = Z_Calloc(sizeof(*scene5), PU_STATIC);
	scene5->scene.transitionOutHeight = 204;
	scene5->scene.picLump = W_GetNumForName("EGGMAD");
	scene5->picRadar = W_GetNumForName("RADARBG");
	scene5->picSweep = W_GetNumForName("RSWEEP");
	scene5->scene.text = intro5text;
	scene5->scene.textCharDelayTics = scene5->scene.textCharDelayCounter = 4;
	scene5->scene.preTextDelay = TICRATE;
	scene5->scene.postTextDelay = 2*TICRATE;
	scene5->scene.textBox.x = 16;
	scene5->scene.textBox.y = 128 + 16;
	scene5->scene.textBox.width = 320 - 32 - 32;
	scene5->scene.textBox.height = 224 - 16 - scene5->scene.textBox.y;
	scene5->scene.init = (void(*)(storyscene_t *))Scene_5_Init;
	scene5->scene.tic = (void(*)(storyscene_t *))Scene_5_Tick;
	scene5->scene.draw = (void(*)(storyscene_t *))Scene_5_Draw;
	scene5->scene.stop = (void(*)(storyscene_t *))Scene_5_Stop;
	introScenes[i++] = (storyscene_t*)scene5;

	scene_6_t *scene6 = Z_Calloc(sizeof(*scene6), PU_STATIC);
	scene6->scene.transitionOutHeight = 204;
	scene6->scene.picLump = W_GetNumForName("THINKBG");
	scene6->scene.text = intro6text;
	scene6->scene.textCharDelayTics = scene6->scene.textCharDelayCounter = 4;
	scene6->scene.preTextDelay = TICRATE;
	scene6->scene.postTextDelay = 2*TICRATE;
	scene6->scene.textBox.x = 184;
	scene6->scene.textBox.y = 32;
	scene6->scene.textBox.width = 320 - 184 - 32;
	scene6->scene.textBox.height = 200 - 64;
	scene6->metalX = 27 - 10;
	scene6->metalY = 64 - 25;
	scene6->scene.init = (void(*)(storyscene_t *))Scene_6_Init;
	scene6->scene.tic = (void(*)(storyscene_t *))Scene_6_Tick;
	scene6->scene.draw = (void(*)(storyscene_t *))Scene_6_Draw;
	scene6->scene.stop = (void(*)(storyscene_t *))Scene_6_Stop;
	introScenes[i++] = (storyscene_t*)scene6;

	scene_7_t *scene7 = Z_Calloc(sizeof(*scene7), PU_STATIC);
	scene7->scene.transitionOutHeight = 204;
	scene7->scene.picLump = W_GetNumForName("SLEEP1");
	scene7->scene.text = intro7text;
	scene7->scene.textCharDelayTics = scene7->scene.textCharDelayCounter = 4;
	scene7->scene.preTextDelay = TICRATE;
	scene7->scene.postTextDelay = 4*TICRATE;
	scene7->scene.textBox.x = 16;
	scene7->scene.textBox.y = 128 + 16;
	scene7->scene.textBox.width = 320 - 32 - 32;
	scene7->scene.textBox.height = 224 - 16 - scene7->scene.textBox.y;
	scene7->scene.init = (void(*)(storyscene_t *))Scene_7_Init;
	scene7->scene.tic = (void(*)(storyscene_t *))Scene_7_Tick;
	scene7->scene.draw = (void(*)(storyscene_t *))Scene_7_Draw;
	scene7->scene.stop = (void(*)(storyscene_t *))Scene_7_Stop;
	introScenes[i++] = (storyscene_t*)scene7;

	scene_8_t *scene8 = Z_Calloc(sizeof(*scene8), PU_STATIC);
	scene8->scene.transitionOutHeight = 204;
	scene8->scene.picLump = W_GetNumForName("TARGETBG");
	scene8->scene.text = intro8text;
	scene8->scene.textCharDelayTics = scene8->scene.textCharDelayCounter = 4;
	scene8->scene.preTextDelay = TICRATE;
	scene8->scene.postTextDelay = 2*TICRATE;
	scene8->scene.textBox.x = 16;
	scene8->scene.textBox.y = 128 + 16;
	scene8->scene.textBox.width = 320 - 32 - 32;
	scene8->scene.textBox.height = 224 - 16 - scene8->scene.textBox.y;
	scene8->scene.init = (void(*)(storyscene_t *))Scene_8_Init;
	scene8->scene.tic = (void(*)(storyscene_t *))Scene_8_Tick;
	scene8->scene.draw = (void(*)(storyscene_t *))Scene_8_Draw;
	scene8->scene.stop = (void(*)(storyscene_t *))Scene_8_Stop;
	introScenes[i++] = (storyscene_t*)scene8;

	scene_9_t *scene9 = Z_Calloc(sizeof(*scene9), PU_STATIC);
	scene9->scene.transitionOutHeight = 204;
	scene9->scene.picLump = W_GetNumForName("SPRINGBG");
	scene9->scene.text = intro9text;
	scene9->scene.textCharDelayTics = scene9->scene.textCharDelayCounter = 4;
	scene9->scene.preTextDelay = TICRATE;
	scene9->scene.postTextDelay = 2*TICRATE;
	scene9->scene.textBox.x = 16;
	scene9->scene.textBox.y = 128 + 16;
	scene9->scene.textBox.width = 320 - 32 - 32;
	scene9->scene.textBox.height = 224 - 16 - scene9->scene.textBox.y;
	scene9->scene.init = (void(*)(storyscene_t *))Scene_9_Init;
	scene9->scene.tic = (void(*)(storyscene_t *))Scene_9_Tick;
	scene9->scene.draw = (void(*)(storyscene_t *))Scene_9_Draw;
	scene9->scene.stop = (void(*)(storyscene_t *))Scene_9_Stop;
	scene9->zoomCoords[0].x = 128;
	scene9->zoomCoords[0].y = 19;
	scene9->zoomCoords[1].x = 118;
	scene9->zoomCoords[1].y = 11;
	scene9->zoomCoords[2].x = 118;
	scene9->zoomCoords[2].y = 11;
	scene9->zoomCoords[3].x = 65;
	scene9->zoomCoords[3].y = 3;
	scene9->zoomCoords[4].x = 59;
	scene9->zoomCoords[4].y = 2;
	scene9->zoomCoords[5].x = 11;
	scene9->zoomCoords[5].y = 0;
	scene9->zoomCoords[6].x = 3;
	scene9->zoomCoords[6].y = 0;
	scene9->zoomCoords[7].x = 0;
	scene9->zoomCoords[7].y = 1;
	scene9->zoomCoords[8].x = 0;
	scene9->zoomCoords[8].y = 12;
	scene9->zoomCoords[9].x = 9;
	scene9->zoomCoords[9].y = 16;
	scene9->zoomCoords[10].x = 24;
	scene9->zoomCoords[10].y = 16;
	scene9->zoomCoords[11].x = 48;
	scene9->zoomCoords[11].y = 17;
	scene9->zoomCoords[12].x = 50;
	scene9->zoomCoords[12].y = 24;
	scene9->zoomCoords[13].x = 50;
	scene9->zoomCoords[13].y = 40;
	introScenes[i++] = (storyscene_t*)scene9;

	scene_10_t *scene10 = Z_Calloc(sizeof(*scene10), PU_STATIC);
	scene10->scene.transitionOutHeight = 204;
	scene10->scene.picLump = W_GetNumForName("RUNBG");
	scene10->scene.text = intro10text;
	scene10->scene.textCharDelayTics = scene10->scene.textCharDelayCounter = 4;
	scene10->scene.preTextDelay = TICRATE;
	scene10->scene.postTextDelay = 2*TICRATE;
	scene10->scene.textBox.x = 16;
	scene10->scene.textBox.y = 128 + 16;
	scene10->scene.textBox.width = 320 - 32 - 32;
	scene10->scene.textBox.height = 224 - 16 - scene10->scene.textBox.y;
	scene10->scene.init = (void(*)(storyscene_t *))Scene_10_Init;
	scene10->scene.tic = (void(*)(storyscene_t *))Scene_10_Tick;
	scene10->scene.draw = (void(*)(storyscene_t *))Scene_10_Draw;
	scene10->scene.stop = (void(*)(storyscene_t *))Scene_10_Stop;
	introScenes[i++] = (storyscene_t*)scene10;

	scene_11_t *scene11 = Z_Calloc(sizeof(*scene11), PU_STATIC);
	scene11->scene.transitionOutHeight = 204;
	scene11->scene.picLump = W_GetNumForName("EGGMOUNT");
	scene11->scene.text = intro11text;
	scene11->scene.textCharDelayTics = scene11->scene.textCharDelayCounter = 4;
	scene11->scene.preTextDelay = TICRATE;
	scene11->scene.postTextDelay = 2*TICRATE;
	scene11->scene.textBox.x = 16;
	scene11->scene.textBox.y = 128 + 16;
	scene11->scene.textBox.width = 320 - 32 - 32;
	scene11->scene.textBox.height = 224 - 16 - scene11->scene.textBox.y;
	scene11->scene.init = (void(*)(storyscene_t *))Scene_11_Init;
	scene11->scene.tic = (void(*)(storyscene_t *))Scene_11_Tick;
	scene11->scene.draw = (void(*)(storyscene_t *))Scene_11_Draw;
	scene11->scene.stop = (void(*)(storyscene_t *))Scene_11_Stop;
	introScenes[i++] = (storyscene_t*)scene11;

	scene_12_t *scene12 = Z_Calloc(sizeof(*scene12), PU_STATIC);
	scene12->scene.transitionOutHeight = 204;
	scene12->scene.picLump = W_GetNumForName("SPOINT");
	scene12->scene.text = intro12text;
	scene12->scene.textCharDelayTics = scene12->scene.textCharDelayCounter = 4;
	scene12->scene.preTextDelay = TICRATE;
	scene12->scene.postTextDelay = 2*TICRATE;
	scene12->scene.textBox.x = 16;
	scene12->scene.textBox.y = 128 + 16;
	scene12->scene.textBox.width = 320 - 32 - 32;
	scene12->scene.textBox.height = 224 - 16 - scene12->scene.textBox.y;
	scene12->scene.init = (void(*)(storyscene_t *))Scene_12_Init;
	scene12->scene.tic = (void(*)(storyscene_t *))Scene_12_Tick;
	scene12->scene.draw = (void(*)(storyscene_t *))Scene_12_Draw;
	scene12->scene.stop = (void(*)(storyscene_t *))Scene_12_Stop;
	introScenes[i++] = (storyscene_t*)scene12;

	scene_13_t *scene13 = Z_Calloc(sizeof(*scene13), PU_STATIC);
	scene13->scene.transitionOutHeight = 204;
	scene13->scene.picLump = W_GetNumForName("EJBG");
	scene13->scene.text = intro13text;
	scene13->scene.textCharDelayTics = scene13->scene.textCharDelayCounter = 4;
	scene13->scene.preTextDelay = TICRATE;
	scene13->scene.postTextDelay = 2*TICRATE;
	scene13->scene.textBox.x = 16;
	scene13->scene.textBox.y = 128 + 16;
	scene13->scene.textBox.width = 320 - 32 - 32;
	scene13->scene.textBox.height = 224 - 16 - scene13->scene.textBox.y;
	scene13->scene.init = (void(*)(storyscene_t *))Scene_13_Init;
	scene13->scene.tic = (void(*)(storyscene_t *))Scene_13_Tick;
	scene13->scene.draw = (void(*)(storyscene_t *))Scene_13_Draw;
	scene13->scene.stop = (void(*)(storyscene_t *))Scene_13_Stop;
	introScenes[i++] = (storyscene_t*)scene13;

	scene_14_t *scene14 = Z_Calloc(sizeof(*scene14), PU_STATIC);
	scene14->scene.transitionOutHeight = 204;
	scene14->scene.picLump = W_GetNumForName("SADTAILS");
	scene14->scene.text = intro14text;
	scene14->scene.textCharDelayTics = scene14->scene.textCharDelayCounter = 4;
	scene14->scene.preTextDelay = TICRATE;
	scene14->scene.postTextDelay = 2*TICRATE;
	scene14->scene.textBox.x = 16;
	scene14->scene.textBox.y = 128 + 16;
	scene14->scene.textBox.width = 320 - 32 - 32;
	scene14->scene.textBox.height = 224 - 16 - scene14->scene.textBox.y;
	scene14->scene.init = (void(*)(storyscene_t *))Scene_14_Init;
	scene14->scene.tic = (void(*)(storyscene_t *))Scene_14_Tick;
	scene14->scene.draw = (void(*)(storyscene_t *))Scene_14_Draw;
	scene14->scene.stop = (void(*)(storyscene_t *))Scene_14_Stop;
	introScenes[i++] = (storyscene_t*)scene14;

	scene_15_t *scene15 = Z_Calloc(sizeof(*scene15), PU_STATIC);
	scene15->scene.transitionOutHeight = 204;
	scene15->scene.picLump = W_GetNumForName("STALK1");
	scene15->scene.text = intro15text;
	scene15->scene.textCharDelayTics = scene15->scene.textCharDelayCounter = 4;
	scene15->scene.preTextDelay = TICRATE;
	scene15->scene.postTextDelay = 2*TICRATE;
	scene15->scene.textBox.x = 16;
	scene15->scene.textBox.y = 128 + 16;
	scene15->scene.textBox.width = 320 - 32 - 32;
	scene15->scene.textBox.height = 224 - 16 - scene15->scene.textBox.y;
	scene15->scene.init = (void(*)(storyscene_t *))Scene_15_Init;
	scene15->scene.tic = (void(*)(storyscene_t *))Scene_15_Tick;
	scene15->scene.draw = (void(*)(storyscene_t *))Scene_15_Draw;
	scene15->scene.stop = (void(*)(storyscene_t *))Scene_15_Stop;
	introScenes[i++] = (storyscene_t*)scene15;

	scene_16_t *scene16 = Z_Calloc(sizeof(*scene16), PU_STATIC);
	scene16->scene.transitionOutHeight = 204;
	scene16->scene.picLump = W_GetNumForName("MOBG");
	scene16->scene.text = intro16text;
	scene16->scene.textCharDelayTics = scene16->scene.textCharDelayCounter = 4;
	scene16->scene.preTextDelay = TICRATE;
	scene16->scene.postTextDelay = 2*TICRATE;
	scene16->scene.textBox.x = 16;
	scene16->scene.textBox.y = 128 + 16;
	scene16->scene.textBox.width = 320 - 32 - 32;
	scene16->scene.textBox.height = 224 - 16 - scene16->scene.textBox.y;
	scene16->scene.init = (void(*)(storyscene_t *))Scene_16_Init;
	scene16->scene.tic = (void(*)(storyscene_t *))Scene_16_Tick;
	scene16->scene.draw = (void(*)(storyscene_t *))Scene_16_Draw;
	scene16->scene.stop = (void(*)(storyscene_t *))Scene_16_Stop;
	introScenes[i++] = (storyscene_t*)scene16;
}

void START_Story (void)
{
	DoubleBufferSetup();	// Clear frame buffers to black.

	screenCount = 0;

	fadetime = 0;

	startmap = 1;

	sceneFrameCount = 0;
	phaseFrameCount = 0;

	I_SetPalette(dc_playpals);

	Z_FreeTags(mainzone);

	R_InitColormap();

	clearscreen = 2;

	for (int i = 0; i < 2; i++)
	{
		I_FillFrameBuffer(COLOR_THRU);
		UpdateBuffer();
	}

	effects_flags = EFFECTS_COPPER_ENABLED;

	I_SetBankPageExplicit(7, 12, true);

	BuildScenes();
	currentScene = 0;
	introScenes[currentScene]->init(introScenes[currentScene]);

	StartTransition();

	S_StartSong(W_CheckNumForName("VGM_STOR"), false, cdtrack_story);
}

int TIC_Story (void)
{
	int exit = ga_nothing;

	screenCount++;
	sceneFrameCount++;
	phaseFrameCount++;

	//DebugControls();

	// Command format:
	// 0000000x sssssppp
	// x - 1 for new command; 0 otherwise
	// s - Scene number (0-31)
	// p - Phase number (0-7)

	if (bgm_sync_command != 0) {
		//currentPhase = (bgm_sync_command & 7);
		//currentScene = ((bgm_sync_command >> 3) & 0x1F);

		// If phase is 0, we're starting a new scene; otherwise, we're starting a new phase.
		if (bgm_sync_command & 7) {
			NextPhase();
		}
		else {
			NextScene();
		}

		bgm_sync_command = 0;
	}

	if ((ticrealbuttons & BT_ACTION_START && !(oldticrealbuttons & BT_ACTION_START)) || currentScene >= NUMSCENES) {
		exit = ga_startnew;
	}
	else {
		introScenes[currentScene]->tic(introScenes[currentScene]);
	}

	return exit;
}

void DRAW_Story (void)
{
	if (currentScene >= NUMSCENES) {
		return;
	}

	// Sync frames.
	while (frame_sync == mars_vblank_count);
	frame_sync = mars_vblank_count;

	// Initialize framebuffers if necessary.
	if (clearscreen > 0) {
		h32_adjust = false;
		Mars_SetVideoMode(MARS_VDP_MODE_32K, 10);
		clearscreen--;
	}

	if (transitionInProgress) {
		RunTransition();
	}
	else {
		introScenes[currentScene]->draw(introScenes[currentScene]);
	}
}

void STOP_Story (void)
{
	S_StopSong();
	
	// Sync frames.
	while (frame_sync == mars_vblank_count);
	frame_sync = mars_vblank_count;

	// Initialize framebuffers if necessary.
	clearscreen = 2;
	if (clearscreen > 0) {
		h32_adjust = true;
		Mars_SetVideoMode(MARS_VDP_MODE_256, 0);
		clearscreen--;
	}

	//introScenes[currentScene]->stop(introScenes[currentScene]);

	for (int i = 0; i < NUMSCENES; i++) {
		Z_Free(introScenes[i]);
	}

	I_SetBankPageExplicit(6, 6, false);
	I_SetBankPageExplicit(7, 7, true);

	DoubleBufferSetup();	// Clear frame buffers to black.
}
