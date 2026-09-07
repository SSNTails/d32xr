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
	jagobj_t *satellite;
	VINT picLump;
	VINT picSatellite;
	VINT satX, satY;
	VINT satCounter;
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
	scene->background = W_CacheLumpNum(scene->picLump, PU_LEVEL);
	scene->satellite = W_CacheLumpNum(scene->picSatellite, PU_LEVEL);
	scene->satCounter = 4;
	scene->satX = 144;
	scene->satY = 24;
}

void Scene_1_Tick(scene_1_t *scene)
{
	TIC_Text(&scene->scene);

	if (--scene->satCounter <= 0)
	{
		scene->satX++;
		scene->satCounter = 4;
	}

	if (scene->satX > 288)
		scene->satX = 288;
}

void Scene_1_Draw(scene_1_t *scene)
{
	// Draw background
	DrawJagobj3_15bpp(
		scene->background,
		0,
		0,
		0,
		0,
		scene->background->width,
		scene->background->height,
		320,
		I_FrameBuffer()
	);

	// Draw satellite drifting overtop
	DrawJagobj3_15bpp(
		scene->satellite,
		scene->satX,
		scene->satY,
		0,
		0,
		scene->satellite->width,
		scene->satellite->height,
		320,
		I_FrameBuffer()
	);

	DrawText(&scene->scene);
}

void Scene_1_Stop(scene_1_t *scene)
{
	// Free any resources
	Z_Free(scene->background);
	Z_Free(scene->satellite);
}

void Scene_2_Init(scene_1_t *scene)
{
	// Cache any graphics, etc.
	scene->background = W_CacheLumpNum(scene->picLump, PU_LEVEL);
}

void Scene_2_Tick(scene_1_t *scene)
{
	TIC_Text(&scene->scene);
}

void Scene_2_Draw(scene_1_t *scene)
{
	// Draw background
	DrawJagobj3_15bpp(
		scene->background,
		0,
		0,
		0,
		0,
		scene->background->width,
		scene->background->height,
		320,
		I_FrameBuffer()
	);

	DrawText(&scene->scene);
}

void Scene_2_Stop(scene_1_t *scene)
{
	// Free any resources
	Z_Free(scene->background);
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
	scene1->picLump = W_GetNumForName("PLANET");
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
	scene2->picLump = W_GetNumForName("RSBG");
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

	S_StartSong(W_CheckNumForName("VGM_STOR"), false, cdtrack_story);
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

	introScenes[currentScene]->stop(introScenes[currentScene]);

	for (int i = 0; i < NUMSCENES; i++) {
		Z_Free(introScenes[i]);
	}

	DoubleBufferSetup();	// Clear frame buffers to black.
}
