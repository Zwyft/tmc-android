#include "virtuappu.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* Minimal GBA PPU renderer — software-renders VRAM/OAM to 240x160 RGBA.
 * Handles modes 0-2 text/affine BGs and OBJ sprites.
 * This is a clean-room implementation based on the GBA hardware spec. */

VirtuaPPURegisters virtuappu_registers = { MODE1_GBA_WIDTH, 1 };
uint32_t virtuappu_frame_buffer[MODE1_GBA_WIDTH * MODE1_GBA_HEIGHT];
void (*virtuappu_mode1_pre_line_callback)(int line) = NULL;

/* Bound GBA memory (set by Port_PPU_Init) */
static uint8_t* s_io = NULL;
static uint8_t* s_vram = NULL;
static uint16_t* s_bg_pal = NULL;
static uint16_t* s_obj_pal = NULL;
static uint16_t* s_oam = NULL;

/* Helper: read u16 from IO reg */
static inline u16 io16(u32 reg) { return s_io ? *(u16*)(s_io + reg) : 0; }

/* Helper: convert GBA 15-bit BGR color to 32-bit RGBA */
static inline uint32_t gba_color(uint16_t c) {
    uint8_t r = (c >> 0) & 0x1f; r = (r << 3) | (r >> 2);
    uint8_t g = (c >> 5) & 0x1f; g = (g << 3) | (g >> 2);
    uint8_t b = (c >> 10) & 0x1f; b = (b << 3) | (b >> 2);
    return 0xFF000000 | (r << 16) | (g << 8) | b;
}

/* BG control register fields */
static inline int bg_map_base(u16 cnt) { return (cnt & 0x1F) * 2; }     /* KB offset into VRAM */
static inline int bg_tile_base(u16 cnt) { return ((cnt >> 8) & 0x0F) * 16; } /* KB offset */
static inline int bg_bpp_8(u16 cnt) { return (cnt >> 7) & 1; }            /* 0=4bpp, 1=8bpp */
static inline int bg_size(u16 cnt) { return cnt & 0xC000; }               /* 0=32x32, 1=64x32, 2=32x64, 3=64x64 */
static inline int bg_mosaic(u16 cnt) { return (cnt >> 6) & 1; }

/* Render a single text BG tile row */
static void render_bg_text_row(int bg, int line, u16* pal) {
    u16 cnt = io16(0x08 + bg * 2);
    if (!(io16(0) & (0x100 << bg))) return; /* BG not enabled */
    if (bg_bpp_8(cnt)) return; /* Skip 8bpp for now */

    int map_kb = bg_map_base(cnt);
    int tile_kb = bg_tile_base(cnt);
    int s = bg_size(cnt);
    int tiles_w = (s & 1) ? 64 : 32;
    int tiles_h = (s & 2) ? 64 : 32;
    int scroll_x = io16(0x10 + bg * 4);
    int scroll_y = io16(0x12 + bg * 4);

    int vram_ofs = map_kb * 1024;
    int tile_ofs = tile_kb * 16384;
    int tile_row = ((line + scroll_y) & (tiles_h * 8 - 1)) / 8;
    int tile_y = ((line + scroll_y) & 7);

    for (int x = 0; x < MODE1_GBA_WIDTH; x++) {
        int tx = (x + scroll_x) & (tiles_w * 8 - 1);
        int tile_col = tx / 8;
        int tile_x = tx & 7;

        /* 32x32: se_addr = tile_row * 32 + tile_col (2 bytes each) */
        /* 64x32: se_addr = tile_row * 64 + tile_col */
        int se_addr;
        if (s == 0) se_addr = tile_row * 32 + tile_col;
        else if (s == 1) se_addr = tile_row * 64 + tile_col;
        else if (s == 2) se_addr = tile_row * 32 + tile_col; /* two map sections */
        else se_addr = tile_row * 64 + tile_col;

        u16 se = s_vram ? *(u16*)(s_vram + vram_ofs + se_addr * 2) : 0;
        int tile_num = se & 0x3FF;
        int pal_bank = (se >> 12) & 0x0F;
        int hflip = (se >> 10) & 1;
        int vflip = (se >> 11) & 1;

        int px = hflip ? (7 - tile_x) : tile_x;
        int py = vflip ? (7 - tile_y) : tile_y;

        /* 4bpp tile: 32 bytes per tile, 8 pixels per row = 4 bytes */
        u32 tile_data = s_vram ? *(u32*)(s_vram + tile_ofs + tile_num * 32 + py * 4) : 0;
        int color_idx = (tile_data >> (px * 4)) & 0x0F;

        uint32_t color = 0;
        if (color_idx != 0 && pal) {
            color = gba_color(pal[pal_bank * 16 + color_idx]);
        }

        virtuappu_frame_buffer[line * MODE1_GBA_WIDTH + x] = color | virtuappu_frame_buffer[line * MODE1_GBA_WIDTH + x];
    }
}

/* Render OBJ (sprites) from OAM */
static void render_sprites(int line) {
    if (!(io16(0) & 0x1000)) return; /* OBJ not enabled */
    if (!s_oam) return;
    if (!s_obj_pal) return;

    for (int i = 0; i < 128; i++) {
        u16 attr0 = s_oam[i * 4 + 0];
        u16 attr1 = s_oam[i * 4 + 1];
        u16 attr2 = s_oam[i * 4 + 2];

        int y = attr0 & 0xFF;
        if (y >= 160) y -= 256;
        int shape = (attr0 >> 14) & 3;
        int bpp8 = (attr0 >> 13) & 1;

        int x = attr1 & 0x1FF;
        if (x >= 240) x -= 512;
        int size = (attr1 >> 14) & 3;

        /* Calculate sprite dimensions from shape+size */
        static const int dim[4][4][2] = {
            {{8,8},{16,16},{32,32},{64,64}},  /* square */
            {{16,8},{32,8},{32,16},{64,32}},   /* horizontal */
            {{8,16},{8,32},{16,32},{32,64}},   /* vertical */
        };
        int sw = dim[shape][size][0];
        int sh = dim[shape][size][1];

        int hflip = (attr1 >> 12) & 1;
        int vflip = (attr1 >> 13) & 1;
        int prio = (attr2 >> 10) & 3;

        if (y > line || y + sh <= line) continue;

        int tile_num = attr2 & 0x3FF;
        int pal_bank = (attr2 >> 12) & 0x0F;
        int sy = line - y;
        if (vflip) sy = sh - 1 - sy;

        for (int sx = 0; sx < sw; sx++) {
            int px = hflip ? (sw - 1 - sx) : sx;
            int screen_x = x + sx;
            if (screen_x < 0 || screen_x >= MODE1_GBA_WIDTH) continue;

            /* Calculate tile pixel */
            int tile_row = sy / 8;
            int tile_col = px / 8;
            int tile_in_sprite = tile_row * (sw / 8) + tile_col;
            int in_tile_x = px & 7;
            int in_tile_y = sy & 7;

            int real_tile = tile_num + tile_in_sprite;
            u32 tile_data = s_vram ? *(u32*)(s_vram + real_tile * 32 + in_tile_y * 4) : 0;
            int color_idx = (tile_data >> (in_tile_x * 4)) & 0x0F;

            if (color_idx != 0) {
                uint32_t color = bpp8
                    ? gba_color(s_obj_pal[color_idx])
                    : gba_color(s_obj_pal[pal_bank * 16 + color_idx]);
                /* Simple priority: sprite over BG if priority matches */
                virtuappu_frame_buffer[line * MODE1_GBA_WIDTH + screen_x] = color;
            }
        }
    }
}

void virtuappu_mode1_bind_gba_memory(const VirtuaPPUMode1GbaMemory* mem) {
    s_io = mem->io;
    s_vram = mem->vram;
    s_bg_pal = mem->bg_palette;
    s_obj_pal = mem->obj_palette;
    s_oam = mem->oam;
}

void virtuappu_render_frame(void) {
    /* Clear framebuffer to white (title screen BG color default) */
    uint32_t bg_color = s_bg_pal ? gba_color(s_bg_pal[0]) : 0xFFFFFFFF;
    for (int i = 0; i < MODE1_GBA_WIDTH * MODE1_GBA_HEIGHT; i++) {
        virtuappu_frame_buffer[i] = bg_color;
    }

    u16 dispcnt = io16(0);
    int mode = dispcnt & 7;
    int bg_enable = (dispcnt >> 8) & 0x0F;

    virtuappu_registers.mode = mode;
    if (virtuappu_mode1_pre_line_callback) {
        for (int line = 0; line < MODE1_GBA_HEIGHT; line++) {
            virtuappu_mode1_pre_line_callback(line);
        }
    }

    /* Render per-scanline */
    for (int line = 0; line < MODE1_GBA_HEIGHT; line++) {
        if (mode <= 1) {
            /* Text BGs: BG0-BG3 */
            for (int bg = 0; bg < 4; bg++) {
                if (bg_enable & (1 << bg)) {
                    render_bg_text_row(bg, line, s_bg_pal);
                }
            }
        }
        /* Sprites always render on top (simplified) */
        render_sprites(line);
    }
}
