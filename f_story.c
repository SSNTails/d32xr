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
    int16_t width, height;
} rect_t;

typedef struct storyscene_s
{
	VINT transitionOutHeight;
	VINT picLump;
	jagobj_t *background;
    const char *text;
    int16_t textPos; // Position in the string of how far along the text has been printed
    int16_t textCharDelayTics; // Number of tics to wait before incrementing textPos
    int16_t textCharDelayCounter; // Decrement this. When it hits zero, textPos++
    int16_t postTextDelay; // Number of tics to wait after printing all text before transitioning to the next scene
    rect_t textBox; // Bounding box to print text inside

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
} scene_2_t, scene_3_t, scene_4_t, scene_5_t;

//#define NUMSCENES 12
#define NUMSCENES	5
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
	// If not at the end of the string, run the char counter
	if (scene->text[scene->textPos] != '\0')
	{
		scene->textCharDelayCounter--;
		if (scene->textCharDelayCounter <= 0)
		{
			scene->textCharDelayCounter = scene->textCharDelayTics;
			scene->textPos++;
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
}

void StartTransition() {
	transitionCount = 8;
	transitionDirection = 0;
	transitionInProgress = true;
}

void RunTransition() {
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

void TransitionOut() {
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

void TransitionIn() {
	transitionCount++;

	jagobj_t *background = introScenes[currentScene]->background;

	for (int section = transitionCount; section < background->height; section += 8) {
		DrawJagobj3_15bpp(
			background,
			0,
			section,
			0,
			section,
			background->width,
			1,
			320,
			I_FrameBuffer()
		);
	}

	if (transitionCount > 0) {
		for (int section = transitionCount-1; section < background->height; section += 8) {
			DrawJagobj3_15bpp(
				background,
				0,
				section,
				0,
				section,
				background->width,
				1,
				320,
				I_FrameBuffer()
			);
		}
	}
}

void Scene_1_Init(scene_1_t *scene)
{
	// Cache any graphics, etc.
	scene->scene.background = W_CacheLumpNum(scene->scene.picLump, PU_LEVEL);
	scene->satellite = W_CacheLumpNum(scene->picSatellite, PU_LEVEL);
	scene->satX = (24<<16);
	scene->satY = (32<<16);
	scene->satZ = (1<<16) + (1<<15);

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
			0,
			0,
			0,
			0,
			scene->scene.background->width,
			scene->scene.background->height,
			320,
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
			0,
			0,
			0,
			0,
			scene->scene.background->width,
			scene->scene.background->height,
			320,
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
			0,
			0,
			0,
			0,
			scene->scene.background->width,
			scene->scene.background->height,
			320,
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
}

void Scene_4_Tick(scene_4_t *scene)
{
	TIC_Text(&scene->scene);
}

void Scene_4_Draw(scene_4_t *scene)
{
	if (sceneFrameCount <= 2) {
		// Draw background
		DrawJagobj3_15bpp(
			scene->scene.background,
			0,
			0,
			0,
			0,
			scene->scene.background->width,
			scene->scene.background->height,
			320,
			I_FrameBuffer()
		);
	}

	DrawText(&scene->scene);
}

void Scene_4_Stop(scene_4_t *scene)
{
	// Free any resources
	Z_Free(scene->scene.background);
}

void Scene_5_Init(scene_5_t *scene)
{
	// Cache any graphics, etc.
	scene->scene.background = W_CacheLumpNum(scene->scene.picLump, PU_LEVEL);
}

void Scene_5_Tick(scene_5_t *scene)
{
	TIC_Text(&scene->scene);
}

void Scene_5_Draw(scene_5_t *scene)
{
	if (sceneFrameCount <= 2) {
		// Draw background
		DrawJagobj3_15bpp(
			scene->scene.background,
			0,
			0,
			0,
			0,
			scene->scene.background->width,
			scene->scene.background->height,
			320,
			I_FrameBuffer()
		);
	}

	DrawText(&scene->scene);
}

void Scene_5_Stop(scene_5_t *scene)
{
	// Free any resources
	Z_Free(scene->scene.background);
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
"\xA7\"Curses!\"\xA9\xBA Eggman yelled. \xA7\"That hedgehog\n"
"and his ridiculous friends will pay\n"
"dearly for this!\"\xA9\xC8 Just then his scanner\n"
"blipped as the Black Rock made its\n"
"appearance from nowhere.\xBF Eggman looked at\n"
"the screen, and just shrugged it off.";

const char *intro6text =
"It was hours later\n"
"that he had an\n"
"idea. \xBF\xA7\"The Black\n"
"Rock has a large\n"
"amount of energy\n"
"within it\xAC...\xA7\xBF\n"
"If I can somehow\n"
"harness this,\xB8 I\n"
"can turn it into\n"
"the ultimate\n"
"battle station\xAC...\xA7\xBF\n"
"And every last\n"
"person will be\n"
"begging for mercy,\xB8\xA8\n"
"including Sonic!\"";

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
	scene1->scene.textCharDelayTics = scene1->scene.textCharDelayCounter = 2;
	scene1->scene.postTextDelay = 2*TICRATE;
	scene1->scene.textBox.x = 32;
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
	scene2->scene.textCharDelayTics = scene2->scene.textCharDelayCounter = 2;
	scene2->scene.postTextDelay = 2*TICRATE;
	scene2->scene.textBox.x = 32;
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
	scene3->scene.textCharDelayTics = scene3->scene.textCharDelayCounter = 2;
	scene3->scene.postTextDelay = 2*TICRATE;
	scene3->scene.textBox.x = 32;
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
	scene4->scene.picLump = W_GetNumForName("PLANET");	//TODO: Change me!
	scene4->scene.text = intro4text;
	scene4->scene.textCharDelayTics = scene4->scene.textCharDelayCounter = 2;
	scene4->scene.postTextDelay = 2*TICRATE;
	scene4->scene.textBox.x = 32;
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
	scene5->scene.text = intro5text;
	scene5->scene.textCharDelayTics = scene5->scene.textCharDelayCounter = 2;
	scene5->scene.postTextDelay = 2*TICRATE;
	scene5->scene.textBox.x = 32;
	scene5->scene.textBox.y = 128 + 16;
	scene5->scene.textBox.width = 320 - 32 - 32;
	scene5->scene.textBox.height = 224 - 16 - scene5->scene.textBox.y;
	scene5->scene.init = (void(*)(storyscene_t *))Scene_5_Init;
	scene5->scene.tic = (void(*)(storyscene_t *))Scene_5_Tick;
	scene5->scene.draw = (void(*)(storyscene_t *))Scene_5_Draw;
	scene5->scene.stop = (void(*)(storyscene_t *))Scene_5_Stop;
	introScenes[i++] = (storyscene_t*)scene5;
}

void START_Story (void)
{
	DoubleBufferSetup();	// Clear frame buffers to black.

	screenCount = 0;

	fadetime = 0;

	startmap = 1;

	I_SetPalette(dc_playpals);

	R_InitColormap();

	clearscreen = 2;

	for (int i = 0; i < 2; i++)
	{
		I_FillFrameBuffer(COLOR_THRU);
		UpdateBuffer();
	}

	effects_flags = EFFECTS_COPPER_ENABLED;

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

	//TODO: Why don't these button press checks work??
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

	DoubleBufferSetup();	// Clear frame buffers to black.
}
