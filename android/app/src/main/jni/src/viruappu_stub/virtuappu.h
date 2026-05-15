#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Minimal GBA PPU renderer for Android port. */

#define MODE1_GBA_WIDTH  240
#define MODE1_GBA_HEIGHT 160
typedef uint16_t u16;
typedef uint32_t u32;

typedef struct {
    uint8_t* io;
    uint8_t* vram;
    uint16_t* bg_palette;
    uint16_t* obj_palette;
    uint16_t* oam;
} VirtuaPPUMode1GbaMemory;

typedef struct {
    int frame_width;
    int mode;
} VirtuaPPURegisters;

extern VirtuaPPURegisters virtuappu_registers;
extern uint32_t virtuappu_frame_buffer[MODE1_GBA_WIDTH * MODE1_GBA_HEIGHT];

extern void (*virtuappu_mode1_pre_line_callback)(int line);

void virtuappu_mode1_bind_gba_memory(const VirtuaPPUMode1GbaMemory* mem);
void virtuappu_render_frame(void);

#ifdef __cplusplus
}
#endif
