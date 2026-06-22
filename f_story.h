#include <stdint.h>

// Not the best place for these two
extern uint16_t screenCount;
extern uint32_t frame_sync;

void START_Story (void);
int TIC_Story (void);
void DRAW_Story (void);
void STOP_Story (void);
