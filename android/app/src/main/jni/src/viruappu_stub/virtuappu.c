#include "virtuappu.h"
#include <string.h>
#include <stdlib.h>

VirtuaPPURegisters virtuappu_registers = { MODE1_GBA_WIDTH, 1 };
uint32_t virtuappu_frame_buffer[MODE1_GBA_WIDTH * MODE1_GBA_HEIGHT];
void (*virtuappu_mode1_pre_line_callback)(int line) = NULL;

static uint8_t* s_io = NULL;
static uint8_t* s_vram = NULL;
static uint16_t* s_bg_pal = NULL;
static uint16_t* s_obj_pal = NULL;
static uint16_t* s_oam = NULL;

void virtuappu_mode1_bind_gba_memory(const VirtuaPPUMode1GbaMemory* mem) {
    s_io = mem->io;
    s_vram = mem->vram;
    s_bg_pal = mem->bg_palette;
    s_obj_pal = mem->obj_palette;
    s_oam = mem->oam;
}

void virtuappu_render_frame(void) {
    (void)s_io;
    (void)s_vram;
    (void)s_bg_pal;
    (void)s_obj_pal;
    (void)s_oam;

    /* Stub: fill with dark green so we can see something */
    uint32_t color = 0xFF002000;
    for (int i = 0; i < MODE1_GBA_WIDTH * MODE1_GBA_HEIGHT; i++) {
        virtuappu_frame_buffer[i] = color;
    }
}
