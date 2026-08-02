#include "f_story.h"
#include "v_font.h"
#include "marshw.h"
#include "r_local.h"

static jagobj_t *tf = NULL;
boolean test_always_zoom = false;
fixed_t test_x_zoom = 0;
fixed_t test_y_zoom = 0;
fixed_t test_x_pos = 0;
fixed_t test_y_pos = 0;

typedef struct
{
    int16_t x, y;
    int16_t width, height;
} rect_t;

typedef struct
{
    const char *text;
    const int16_t textLength;
    const int16_t textPos; // Position in the string of how far along the text has been printed
    const int16_t textCharDelayTics; // Number of tics to wait before incrementing textPos
    int16_t textCharDelayCounter; // Decrement this. When it hits zero, textPos++
    const rect_t textBox; // Bounding box to print text inside

    int16_t postTextDelay; // Number of tics to wait after printing all text before transitioning to the next scene

    //void (*tic)(storyscene_t *self);
    //void (*draw)(storyscene_t *self);

} storyscene_t;

void DrawText(storyscene_t *scene)
{
    // Handles drawing the text, including how much of it to draw
}


void START_Story (void)
{
	DoubleBufferSetup();	// Clear frame buffers to black.

	screenCount = 0;

	fadetime = 0;

	startmap = 1;

	I_SetPalette(dc_playpals);

	R_InitColormap();

	tf = W_CacheLumpName("TF128", PU_STATIC);

	clearscreen = 2;

	for (int i = 0; i < 2; i++)
	{
		I_FillFrameBuffer(COLOR_THRU);
		UpdateBuffer();
	}

	effects_flags = EFFECTS_COPPER_ENABLED;
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
		DrawJagobj3_15bpp(
			tf,
			((320-128)/2) + (test_x_pos >> 16),
			((204-128)/2) + (test_y_pos >> 16),
			0,
			0,
			tf->width,
			tf->height,
			320,
			I_FrameBuffer()
		);
	}
	else {
		DrawScaledJagobj_15bpp(
			tf,
			((320-128)/2) + (test_x_pos >> 16),
			((204-128)/2) + (test_y_pos >> 16),
			FRACUNIT + test_x_zoom,
			FRACUNIT + test_y_zoom,
			I_FrameBuffer()
		);
	}
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

	DoubleBufferSetup();	// Clear frame buffers to black.

	Z_Free(tf);
}
