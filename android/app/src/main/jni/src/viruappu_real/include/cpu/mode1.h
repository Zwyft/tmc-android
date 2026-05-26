#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../ppu_memory.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*virtuappu_mode1_pre_line_fn)(int line);
extern virtuappu_mode1_pre_line_fn virtuappu_mode1_pre_line_callback;

enum {
    MODE1_GBA_WIDTH = 240,
    MODE1_GBA_HEIGHT = 160,
    MODE1_GBA_BG_COUNT = 4,
    MODE1_GBA_OAM_COUNT = 128,
    MODE1_IO_MEM_SIZE = 0x400,
    MODE1_VRAM_SIZE = 0x18000,
    MODE1_PALETTE_COLORS = 256,
    MODE1_OAM_HALFWORDS = 512
};

enum {
    MODE1_IO_DISPCNT = 0x00,
    MODE1_IO_BG0CNT = 0x08,
    MODE1_IO_BG1CNT = 0x0A,
    MODE1_IO_BG2CNT = 0x0C,
    MODE1_IO_BG3CNT = 0x0E,
    MODE1_IO_BG0HOFS = 0x10,
    MODE1_IO_BG0VOFS = 0x12,
    MODE1_IO_BG1HOFS = 0x14,
    MODE1_IO_BG1VOFS = 0x16,
    MODE1_IO_BG2HOFS = 0x18,
    MODE1_IO_BG2VOFS = 0x1A,
    MODE1_IO_BG3HOFS = 0x1C,
    MODE1_IO_BG3VOFS = 0x1E,
    MODE1_IO_WIN0H = 0x40,
    MODE1_IO_WIN1H = 0x42,
    MODE1_IO_WIN0V = 0x44,
    MODE1_IO_WIN1V = 0x46,
    MODE1_IO_WININ = 0x48,
    MODE1_IO_WINOUT = 0x4A,
    MODE1_IO_MOSAIC = 0x4C,
    MODE1_IO_BLDCNT = 0x50,
    MODE1_IO_BLDALPHA = 0x52,
    MODE1_IO_BLDY = 0x54
};

enum {
    MODE1_DISP_OBJ_1D = 0x0040,
    MODE1_DISP_FORCED_BLANK = 0x0080,
    MODE1_DISP_BG0_ON = 0x0100,
    MODE1_DISP_BG1_ON = 0x0200,
    MODE1_DISP_BG2_ON = 0x0400,
    MODE1_DISP_BG3_ON = 0x0800,
    MODE1_DISP_OBJ_ON = 0x1000,
    MODE1_DISP_WIN0_ON = 0x2000,
    MODE1_DISP_WIN1_ON = 0x4000,
    MODE1_DISP_OBJWIN_ON = 0x8000
};

typedef struct VirtuaPPUMode1GbaMemory {
    uint8_t *io_mem;
    uint8_t *vram;
    uint16_t *bg_palette;
    uint16_t *obj_palette;
    uint16_t *oam_mem;
} VirtuaPPUMode1GbaMemory;

void virtuappu_mode1_bind_gba_memory(const VirtuaPPUMode1GbaMemory *memory);
void virtuappu_mode1_get_bound_gba_memory(VirtuaPPUMode1GbaMemory *memory);
uint16_t virtuappu_mode1_io_read16(uint16_t offset);
uint32_t virtuappu_mode1_io_read32(uint16_t offset);
uint32_t virtuappu_mode1_rgb555_to_abgr8888(uint16_t color);
void virtuappu_mode1_render_text_bg_line(int bg_index, int line, uint32_t *line_buffer, uint8_t *priority_buffer);
void virtuappu_mode1_render_obj_line(int line, bool obj_1d, uint32_t *line_buffer, uint8_t *priority_buffer);
void virtuappu_mode1_composite_line(
    int line,
    uint32_t bg_layers[MODE1_GBA_BG_COUNT][MODE1_GBA_WIDTH],
    uint8_t bg_priority[MODE1_GBA_BG_COUNT][MODE1_GBA_WIDTH],
    uint32_t obj_layer[MODE1_GBA_WIDTH],
    uint8_t obj_priority[MODE1_GBA_WIDTH],
    uint16_t dispcnt);
void virtuappu_mode1_render_frame(const PPUMemory *ppu);

#ifdef __cplusplus
}
#endif
