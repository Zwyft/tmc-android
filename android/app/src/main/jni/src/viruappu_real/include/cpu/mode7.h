#pragma once

#include <stdint.h>

#include "../ppu_memory.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
    MODE7_GB_SCREEN_WIDTH = 160,
    MODE7_GB_SCREEN_HEIGHT = 144,
    MODE7_VRAM_SIZE_BYTES = 0x2000,
    MODE7_OAM_SIZE_BYTES = 0x00A0,
    MODE7_LCDC_ENABLE = 1u << 7,
    MODE7_LCDC_WINDOW_TILE_MAP = 1u << 6,
    MODE7_LCDC_WINDOW_ENABLE = 1u << 5,
    MODE7_LCDC_BG_WINDOW_TILE_DATA = 1u << 4,
    MODE7_LCDC_BG_TILE_MAP = 1u << 3,
    MODE7_LCDC_OBJ_SIZE = 1u << 2,
    MODE7_LCDC_OBJ_ENABLE = 1u << 1,
    MODE7_LCDC_BG_ENABLE = 1u << 0
};

typedef struct Mode7GBRegs {
    uint8_t lcdc;
    uint8_t scy;
    uint8_t scx;
    uint8_t bgp;
    uint8_t obp0;
    uint8_t obp1;
    uint8_t wy;
    uint8_t wx;
} Mode7GBRegs;

typedef struct Mode7Layout {
    uint8_t vram[MODE7_VRAM_SIZE_BYTES];
    uint8_t oam[MODE7_OAM_SIZE_BYTES];
    Mode7GBRegs regs;
} Mode7Layout;

void virtuappu_mode7_render_frame(const PPUMemory *ppu);

#ifdef __cplusplus
}
#endif
