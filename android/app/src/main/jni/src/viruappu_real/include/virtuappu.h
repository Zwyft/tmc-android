#pragma once

#include <stdint.h>

#include "ppu_memory.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
    VIRTUAPPU_MAX_FRAME_WIDTH = 1280,
    VIRTUAPPU_MAX_FRAME_HEIGHT = 360,
    VIRTUAPPU_FRAME_BUFFER_SIZE = VIRTUAPPU_MAX_FRAME_WIDTH * VIRTUAPPU_MAX_FRAME_HEIGHT,
    VIRTUAPPU_VRAM_SIZE = 4 * 1024 * 1024
};

extern uint32_t virtuappu_frame_buffer[VIRTUAPPU_FRAME_BUFFER_SIZE];
extern uint8_t virtuappu_vram[VIRTUAPPU_VRAM_SIZE];
extern PPUMemory virtuappu_registers;

void virtuappu_reset(void);
void virtuappu_render_frame(void);
uint32_t *virtuappu_get_frame_buffer(void);
uint8_t *virtuappu_get_vram(void);
PPUMemory *virtuappu_get_registers(void);

#ifdef __cplusplus
}
#endif
