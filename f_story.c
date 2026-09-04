#include "f_story.h"
#include "v_font.h"
#include "marshw.h"
#include "r_local.h"

boolean test_always_zoom = false;
fixed_t test_x_zoom = 0;
fixed_t test_y_zoom = 0;
fixed_t test_x_pos = 0;
fixed_t test_y_pos = 0;
int currentScene = 0;

typedef struct
{
    int16_t x, y;
    int16_t width, height;
} rect_t;

typedef struct storyscene_s
{
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
	jagobj_t *background;
	VINT picLump;
	VINT picSatellite;
	VINT satX, satY;
} scene_1_t;

typedef struct
{
	storyscene_t scene;
	VINT picLump;
} scene_2_t;

//#define NUMSCENES 12
#define NUMSCENES	2
storyscene_t *introScenes[NUMSCENES];

void NextScene()
{
	currentScene++;
	if (currentScene >= NUMSCENES) // We're done. How to signal?
		currentScene = currentScene-1;

	// A transition or something?
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
	else // We're at the end
	{
		if (scene->postTextDelay > 0)
			scene->postTextDelay--;

		if (scene->postTextDelay <= 0)
			NextScene();
	}
}

void DrawText(storyscene_t *scene)
{
    // Common function to handle drawing the text, including how much of it to draw
}

void Scene_1_Init(scene_1_t *scene)
{
	// Cache any graphics, etc.
	scene->background = W_CacheLumpNum(scene->picLump, PU_STATIC);
}

void Scene_1_Tick(scene_1_t *scene)
{
	TIC_Text(&scene->scene);

	if (screenCount & 1)
		scene->satX += 1;
}

void Scene_1_Draw(scene_1_t *scene)
{
	// Draw background
	DrawJagobj3_15bpp(
		scene->background,
		((320-128)/2) + (test_x_pos >> 16),
		((204-128)/2) + (test_y_pos >> 16),
		0,
		0,
		scene->background->width,
		scene->background->height,
		320,
		I_FrameBuffer()
	);

	// Draw satellite drifting overtop

	DrawText(&scene->scene);
}

void Scene_1_Stop(scene_1_t *scene)
{
	// Free any resources
	Z_Free(scene->background);
}

void Scene_2_Init(scene_1_t *scene)
{
	// Cache any graphics, etc.
}

void Scene_2_Tick(scene_1_t *scene)
{
	TIC_Text(&scene->scene);
}

void Scene_2_Draw(scene_1_t *scene)
{
	// Draw background?

	DrawText(&scene->scene);
}

void Scene_2_Stop(scene_1_t *scene)
{
	// Free any resources
}

void BuildScenes()
{
	int i = 0;

	scene_1_t *scene1 = Z_Calloc(sizeof(*scene1), PU_STATIC);
	scene1->picLump = W_GetNumForName("PLANET");
	scene1->picSatellite = W_GetNumForName("SATELLIT");
	scene1->scene.text = "Two months had passed since Dr. Eggman\ntried to take over the world with his\nRing Satellite.";
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
	scene2->picLump = W_GetNumForName("RSBG");
	scene2->scene.text = "As it was about to drain the rings\naway from the planet, Sonic burst into\nthe control room and for what he thought\nwould be the last time, defeated\nDr.Eggman.";
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
}

int TIC_Story (void)
{
	int exit = ga_nothing;

	screenCount++;

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

	introScenes[currentScene]->tic(introScenes[currentScene]);

	if (screenCount > 120) {
		exit = ga_startnew;
	}

	return exit;
}

void DRAW_Story (void)
{
	// Sync frames.
	while (frame_sync == mars_vblank_count);
	frame_sync = mars_vblank_count;

	// Initialize framebuffers if necessary.
	if (clearscreen > 0) {
		h32_adjust = false;
		Mars_SetVideoMode(MARS_VDP_MODE_32K, 10);
		clearscreen--;
	}

	// Implement drawing code here.
	if (!test_always_zoom && (test_x_zoom == 0 && test_y_zoom == 0)) {
		// Use the faster function for drawing 15bpp when using 1:1 scaling.
/*		DrawJagobj3_15bpp(
			tf,
			((320-128)/2) + (test_x_pos >> 16),
			((204-128)/2) + (test_y_pos >> 16),
			0,
			0,
			tf->width,
			tf->height,
			320,
			I_FrameBuffer()
		);*/
	}
	else {
/*		DrawScaledJagobj_15bpp(
			tf,
			((320-128)/2) + (test_x_pos >> 16),
			((204-128)/2) + (test_y_pos >> 16),
			FRACUNIT + test_x_zoom,
			FRACUNIT + test_y_zoom,
			I_FrameBuffer()
		);*/
	}

	introScenes[currentScene]->draw(introScenes[currentScene]);
}

void STOP_Story (void)
{
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

	for (int i = 0; i < NUMSCENES; i++) {
		Z_Free(introScenes[i]);
	}

	DoubleBufferSetup();	// Clear frame buffers to black.
}
