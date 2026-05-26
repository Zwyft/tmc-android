#include "cpu/mode1.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "virtuappu.h"

virtuappu_mode1_pre_line_fn virtuappu_mode1_pre_line_callback = NULL;

typedef struct Mode1TilemapEntry {
    uint16_t raw;
} Mode1TilemapEntry;

typedef struct Mode1OAMAttr {
    uint16_t attr0;
    uint16_t attr1;
    uint16_t attr2;
} Mode1OAMAttr;

typedef enum Mode1BlendEffect {
    MODE1_BLEND_NONE = 0,
    MODE1_BLEND_ALPHA = 1,
    MODE1_BLEND_BRIGHTEN = 2,
    MODE1_BLEND_DARKEN = 3
} Mode1BlendEffect;

static uint8_t mode1_default_io_mem[MODE1_IO_MEM_SIZE];
static uint8_t mode1_default_vram[MODE1_VRAM_SIZE];
static uint16_t mode1_default_bg_palette[MODE1_PALETTE_COLORS];
static uint16_t mode1_default_obj_palette[MODE1_PALETTE_COLORS];
static uint16_t mode1_default_oam_mem[MODE1_OAM_HALFWORDS];

static VirtuaPPUMode1GbaMemory mode1_memory = {
    mode1_default_io_mem,
    mode1_default_vram,
    mode1_default_bg_palette,
    mode1_default_obj_palette,
    mode1_default_oam_mem
};

static const uint8_t mode1_obj_widths[3][4] = {
    {8, 16, 32, 64},
    {16, 32, 32, 64},
    {8, 8, 16, 32}
};

static const uint8_t mode1_obj_heights[3][4] = {
    {8, 16, 32, 64},
    {8, 8, 16, 32},
    {16, 32, 32, 64}
};

static uint16_t mode1_tile_index(Mode1TilemapEntry entry)
{
    return entry.raw & 0x03FFu;
}

static bool mode1_tile_hflip(Mode1TilemapEntry entry)
{
    return ((entry.raw >> 10u) & 1u) != 0u;
}

static bool mode1_tile_vflip(Mode1TilemapEntry entry)
{
    return ((entry.raw >> 11u) & 1u) != 0u;
}

static uint8_t mode1_tile_palette(Mode1TilemapEntry entry)
{
    return (uint8_t)((entry.raw >> 12u) & 0x0Fu);
}

static bool mode1_oam_affine(Mode1OAMAttr attr)
{
    return ((attr.attr0 >> 8u) & 1u) != 0u;
}

static bool mode1_oam_double_size(Mode1OAMAttr attr)
{
    return mode1_oam_affine(attr) && (((attr.attr0 >> 9u) & 1u) != 0u);
}

static bool mode1_oam_hidden(Mode1OAMAttr attr)
{
    return !mode1_oam_affine(attr) && (((attr.attr0 >> 9u) & 1u) != 0u);
}

static bool mode1_oam_bpp8(Mode1OAMAttr attr)
{
    return ((attr.attr0 >> 13u) & 1u) != 0u;
}

static int mode1_oam_y(Mode1OAMAttr attr)
{
    return attr.attr0 & 0xFF;
}

static uint8_t mode1_oam_shape(Mode1OAMAttr attr)
{
    return (uint8_t)((attr.attr0 >> 14u) & 3u);
}

static int mode1_oam_x(Mode1OAMAttr attr)
{
    return attr.attr1 & 0x1FF;
}

static bool mode1_oam_hflip(Mode1OAMAttr attr)
{
    return !mode1_oam_affine(attr) && (((attr.attr1 >> 12u) & 1u) != 0u);
}

static bool mode1_oam_vflip(Mode1OAMAttr attr)
{
    return !mode1_oam_affine(attr) && (((attr.attr1 >> 13u) & 1u) != 0u);
}

static uint8_t mode1_oam_affine_index(Mode1OAMAttr attr)
{
    return (uint8_t)((attr.attr1 >> 9u) & 0x1Fu);
}

static uint8_t mode1_oam_size(Mode1OAMAttr attr)
{
    return (uint8_t)((attr.attr1 >> 14u) & 3u);
}

static uint16_t mode1_oam_tile_index(Mode1OAMAttr attr)
{
    return attr.attr2 & 0x03FFu;
}

static uint8_t mode1_oam_priority(Mode1OAMAttr attr)
{
    return (uint8_t)((attr.attr2 >> 10u) & 3u);
}

static uint8_t mode1_oam_palette(Mode1OAMAttr attr)
{
    return (uint8_t)((attr.attr2 >> 12u) & 0x0Fu);
}

static bool mode1_is_first_target(uint16_t bldcnt, int layer_id)
{
    return ((bldcnt >> layer_id) & 1u) != 0u;
}

static bool mode1_is_second_target(uint16_t bldcnt, int layer_id)
{
    return ((bldcnt >> (layer_id + 8)) & 1u) != 0u;
}

static uint32_t mode1_alpha_blend(uint32_t top_abgr, uint32_t bottom_abgr, int eva, int evb)
{
    int top_r = (int)((top_abgr >> 0u) & 0xFFu);
    int top_g = (int)((top_abgr >> 8u) & 0xFFu);
    int top_b = (int)((top_abgr >> 16u) & 0xFFu);
    int bottom_r = (int)((bottom_abgr >> 0u) & 0xFFu);
    int bottom_g = (int)((bottom_abgr >> 8u) & 0xFFu);
    int bottom_b = (int)((bottom_abgr >> 16u) & 0xFFu);
    int out_r = (top_r * eva + bottom_r * evb) / 16;
    int out_g = (top_g * eva + bottom_g * evb) / 16;
    int out_b = (top_b * eva + bottom_b * evb) / 16;

    if (out_r > 255) {
        out_r = 255;
    }
    if (out_g > 255) {
        out_g = 255;
    }
    if (out_b > 255) {
        out_b = 255;
    }

    return 0xFF000000u | ((uint32_t)out_b << 16u) | ((uint32_t)out_g << 8u) | (uint32_t)out_r;
}

static uint32_t mode1_brighten(uint32_t abgr, int evy)
{
    int r = (int)((abgr >> 0u) & 0xFFu);
    int g = (int)((abgr >> 8u) & 0xFFu);
    int b = (int)((abgr >> 16u) & 0xFFu);

    r = r + ((255 - r) * evy) / 16;
    g = g + ((255 - g) * evy) / 16;
    b = b + ((255 - b) * evy) / 16;

    if (r > 255) {
        r = 255;
    }
    if (g > 255) {
        g = 255;
    }
    if (b > 255) {
        b = 255;
    }

    return 0xFF000000u | ((uint32_t)b << 16u) | ((uint32_t)g << 8u) | (uint32_t)r;
}

static uint32_t mode1_darken(uint32_t abgr, int evy)
{
    int r = (int)((abgr >> 0u) & 0xFFu);
    int g = (int)((abgr >> 8u) & 0xFFu);
    int b = (int)((abgr >> 16u) & 0xFFu);

    r -= (r * evy) / 16;
    g -= (g * evy) / 16;
    b -= (b * evy) / 16;

    if (r < 0) {
        r = 0;
    }
    if (g < 0) {
        g = 0;
    }
    if (b < 0) {
        b = 0;
    }

    return 0xFF000000u | ((uint32_t)b << 16u) | ((uint32_t)g << 8u) | (uint32_t)r;
}

void virtuappu_mode1_bind_gba_memory(const VirtuaPPUMode1GbaMemory *memory)
{
    mode1_memory.io_mem = (memory != NULL && memory->io_mem != NULL) ? memory->io_mem : mode1_default_io_mem;
    mode1_memory.vram = (memory != NULL && memory->vram != NULL) ? memory->vram : mode1_default_vram;
    mode1_memory.bg_palette = (memory != NULL && memory->bg_palette != NULL) ? memory->bg_palette : mode1_default_bg_palette;
    mode1_memory.obj_palette = (memory != NULL && memory->obj_palette != NULL) ? memory->obj_palette : mode1_default_obj_palette;
    mode1_memory.oam_mem = (memory != NULL && memory->oam_mem != NULL) ? memory->oam_mem : mode1_default_oam_mem;
}

void virtuappu_mode1_get_bound_gba_memory(VirtuaPPUMode1GbaMemory *memory)
{
    if (memory == NULL) {
        return;
    }

    *memory = mode1_memory;
}

uint16_t virtuappu_mode1_io_read16(uint16_t offset)
{
    return (uint16_t)mode1_memory.io_mem[offset] | ((uint16_t)mode1_memory.io_mem[offset + 1u] << 8u);
}

uint32_t virtuappu_mode1_io_read32(uint16_t offset)
{
    return (uint32_t)virtuappu_mode1_io_read16(offset) |
           ((uint32_t)virtuappu_mode1_io_read16((uint16_t)(offset + 2u)) << 16u);
}

uint32_t virtuappu_mode1_rgb555_to_abgr8888(uint16_t color)
{
    uint8_t r = (uint8_t)((color & 0x1Fu) << 3u);
    uint8_t g = (uint8_t)(((color >> 5u) & 0x1Fu) << 3u);
    uint8_t b = (uint8_t)(((color >> 10u) & 0x1Fu) << 3u);

    return 0xFF000000u | ((uint32_t)b << 16u) | ((uint32_t)g << 8u) | (uint32_t)r;
}

void virtuappu_mode1_render_text_bg_line(int bg_index, int line, uint32_t *line_buffer, uint8_t *priority_buffer)
{
    uint16_t bgcnt = virtuappu_mode1_io_read16((uint16_t)(MODE1_IO_BG0CNT + bg_index * 2));
    uint8_t priority = (uint8_t)(bgcnt & 3u);
    uint32_t char_base = (uint32_t)((bgcnt >> 2u) & 3u) * 0x4000u;
    bool mosaic_on = ((bgcnt >> 6u) & 1u) != 0u;
    bool bpp8 = ((bgcnt >> 7u) & 1u) != 0u;
    uint32_t screen_base = (uint32_t)((bgcnt >> 8u) & 0x1Fu) * 0x800u;
    uint16_t size_flag = (uint16_t)((bgcnt >> 14u) & 3u);
    int map_width_tiles = (size_flag & 1u) ? 64 : 32;
    int map_height_tiles = (size_flag & 2u) ? 64 : 32;
    int scroll_x = virtuappu_mode1_io_read16((uint16_t)(MODE1_IO_BG0HOFS + bg_index * 4)) & 0x1FF;
    int scroll_y = virtuappu_mode1_io_read16((uint16_t)(MODE1_IO_BG0VOFS + bg_index * 4)) & 0x1FF;
    uint16_t mosaic_reg = virtuappu_mode1_io_read16(MODE1_IO_MOSAIC);
    mosaic_on = mosaic_on && getenv("TMC_ENABLE_MOSAIC") != NULL;
    int mosaic_h = mosaic_on ? (int)((mosaic_reg & 0x0Fu) + 1u) : 1;
    int mosaic_v = mosaic_on ? (int)(((mosaic_reg >> 4u) & 0x0Fu) + 1u) : 1;
    int eff_line = (line / mosaic_v) * mosaic_v;
    int src_y = (eff_line + scroll_y) % (map_height_tiles * 8);
    int tile_row = src_y / 8;
    int pixel_y = src_y % 8;
    int x;

    for (x = 0; x < MODE1_GBA_WIDTH; ++x) {
        int eff_x = (x / mosaic_h) * mosaic_h;
        int src_x = (eff_x + scroll_x) % (map_width_tiles * 8);
        int tile_col = src_x / 8;
        int pixel_x = src_x % 8;
        int screen_block_x = tile_col / 32;
        int screen_block_y = tile_row / 32;
        int screen_block_index = screen_block_x + screen_block_y * (map_width_tiles / 32);
        int local_col = tile_col % 32;
        int local_row = tile_row % 32;
        uint32_t map_addr = screen_base + (uint32_t)screen_block_index * 0x800u + (uint32_t)(local_row * 32 + local_col) * 2u;
        Mode1TilemapEntry tile_entry;
        int tile_pixel_x;
        int tile_pixel_y;
        uint8_t color_index;
        uint16_t rgb555;

        tile_entry.raw = (uint16_t)mode1_memory.vram[map_addr] | ((uint16_t)mode1_memory.vram[map_addr + 1u] << 8u);
        tile_pixel_x = mode1_tile_hflip(tile_entry) ? (7 - pixel_x) : pixel_x;
        tile_pixel_y = mode1_tile_vflip(tile_entry) ? (7 - pixel_y) : pixel_y;

        if (bpp8) {
            uint32_t addr = char_base + (uint32_t)mode1_tile_index(tile_entry) * 64u +
                            (uint32_t)tile_pixel_y * 8u + (uint32_t)tile_pixel_x;
            color_index = (addr < MODE1_VRAM_SIZE) ? mode1_memory.vram[addr] : 0u;
        } else {
            uint32_t addr = char_base + (uint32_t)mode1_tile_index(tile_entry) * 32u +
                            (uint32_t)tile_pixel_y * 4u + (uint32_t)(tile_pixel_x / 2);
            uint8_t packed = (addr < MODE1_VRAM_SIZE) ? mode1_memory.vram[addr] : 0u;
            color_index = (tile_pixel_x & 1) ? (packed >> 4u) : (packed & 0x0Fu);
        }

        if (color_index == 0u) {
            continue;
        }

        if (bpp8) {
            rgb555 = mode1_memory.bg_palette[color_index];
        } else {
            rgb555 = mode1_memory.bg_palette[(size_t)mode1_tile_palette(tile_entry) * 16u + color_index];
        }

        line_buffer[x] = virtuappu_mode1_rgb555_to_abgr8888(rgb555);
        priority_buffer[x] = priority;
    }
}

void virtuappu_mode1_render_obj_line(int line, bool obj_1d, uint32_t *line_buffer, uint8_t *priority_buffer)
{
    const uint32_t obj_tile_base = 0x10000u;
    int i;

    for (i = MODE1_GBA_OAM_COUNT - 1; i >= 0; --i) {
        Mode1OAMAttr attr;
        uint8_t shape;
        uint8_t size;
        int obj_width;
        int obj_height;
        bool is_affine;
        int bounds_width;
        int bounds_height;
        int obj_y;
        int obj_x;
        bool bpp8;
        uint8_t priority;
        uint16_t base_tile;
        int tiles_w;
        int16_t pa = 0x100;
        int16_t pb = 0;
        int16_t pc = 0;
        int16_t pd = 0x100;
        int half_width;
        int half_height;
        int sprite_half_width;
        int sprite_half_height;
        int input_rel_y;
        int sx;

        attr.attr0 = mode1_memory.oam_mem[i * 4];
        attr.attr1 = mode1_memory.oam_mem[i * 4 + 1];
        attr.attr2 = mode1_memory.oam_mem[i * 4 + 2];

        if (mode1_oam_hidden(attr)) {
            continue;
        }

        shape = mode1_oam_shape(attr);
        size = mode1_oam_size(attr);
        obj_width = mode1_obj_widths[shape][size];
        obj_height = mode1_obj_heights[shape][size];
        is_affine = mode1_oam_affine(attr);
        bounds_width = obj_width;
        bounds_height = obj_height;

        if (is_affine && mode1_oam_double_size(attr)) {
            bounds_width *= 2;
            bounds_height *= 2;
        }

        obj_y = mode1_oam_y(attr);
        if (obj_y >= MODE1_GBA_HEIGHT) {
            obj_y -= 256;
        }
        if (line < obj_y || line >= obj_y + bounds_height) {
            continue;
        }

        obj_x = mode1_oam_x(attr);
        if (obj_x >= MODE1_GBA_WIDTH) {
            obj_x -= 512;
        }

        bpp8 = mode1_oam_bpp8(attr);
        priority = mode1_oam_priority(attr);
        base_tile = mode1_oam_tile_index(attr);
        tiles_w = obj_width / 8;

        if (is_affine) {
            int affine_group = mode1_oam_affine_index(attr);
            pa = (int16_t)mode1_memory.oam_mem[affine_group * 16 + 3];
            pb = (int16_t)mode1_memory.oam_mem[affine_group * 16 + 7];
            pc = (int16_t)mode1_memory.oam_mem[affine_group * 16 + 11];
            pd = (int16_t)mode1_memory.oam_mem[affine_group * 16 + 15];
        }

        half_width = bounds_width / 2;
        half_height = bounds_height / 2;
        sprite_half_width = obj_width / 2;
        sprite_half_height = obj_height / 2;
        input_rel_y = line - obj_y - half_height;

        for (sx = 0; sx < bounds_width; ++sx) {
            int screen_x = obj_x + sx;
            int tex_x;
            int tex_y;
            int tile_row;
            int pixel_y;
            int tile_col;
            int pixel_x;
            uint16_t tile_index;
            uint8_t color_index;
            uint16_t rgb555;

            if (screen_x < 0 || screen_x >= MODE1_GBA_WIDTH) {
                continue;
            }

            if (is_affine) {
                int input_rel_x = sx - half_width;
                tex_x = ((pa * input_rel_x + pb * input_rel_y) >> 8) + sprite_half_width;
                tex_y = ((pc * input_rel_x + pd * input_rel_y) >> 8) + sprite_half_height;
                if (tex_x < 0 || tex_x >= obj_width || tex_y < 0 || tex_y >= obj_height) {
                    continue;
                }
            } else {
                int draw_x = mode1_oam_hflip(attr) ? (obj_width - 1 - sx) : sx;
                int draw_y = line - obj_y;
                if (mode1_oam_vflip(attr)) {
                    draw_y = obj_height - 1 - draw_y;
                }
                tex_x = draw_x;
                tex_y = draw_y;
            }

            tile_row = tex_y / 8;
            pixel_y = tex_y % 8;
            tile_col = tex_x / 8;
            pixel_x = tex_x % 8;

            if (obj_1d) {
                tile_index = (uint16_t)(base_tile + tile_row * tiles_w + tile_col);
                if (bpp8) {
                    tile_index = (uint16_t)(base_tile + (tile_row * tiles_w + tile_col) * 2);
                }
            } else {
                tile_index = (uint16_t)(base_tile + tile_row * 32 + tile_col);
                if (bpp8) {
                    tile_index = (uint16_t)(base_tile + tile_row * 32 + tile_col * 2);
                }
            }

            if (bpp8) {
                uint32_t addr = obj_tile_base + (uint32_t)tile_index * 32u + (uint32_t)pixel_y * 8u + (uint32_t)pixel_x;
                color_index = (addr < MODE1_VRAM_SIZE) ? mode1_memory.vram[addr] : 0u;
            } else {
                uint32_t addr = obj_tile_base + (uint32_t)tile_index * 32u + (uint32_t)pixel_y * 4u + (uint32_t)(pixel_x / 2);
                uint8_t packed = (addr < MODE1_VRAM_SIZE) ? mode1_memory.vram[addr] : 0u;
                color_index = (pixel_x & 1) ? (packed >> 4u) : (packed & 0x0Fu);
            }

            if (color_index == 0u) {
                continue;
            }

            if (line_buffer[screen_x] != 0u && priority_buffer[screen_x] < priority) {
                continue;
            }

            if (bpp8) {
                rgb555 = mode1_memory.obj_palette[color_index];
            } else {
                rgb555 = mode1_memory.obj_palette[(size_t)mode1_oam_palette(attr) * 16u + color_index];
            }

            line_buffer[screen_x] = virtuappu_mode1_rgb555_to_abgr8888(rgb555);
            priority_buffer[screen_x] = priority;
        }
    }
}

void virtuappu_mode1_composite_line(
    int line,
    uint32_t bg_layers[MODE1_GBA_BG_COUNT][MODE1_GBA_WIDTH],
    uint8_t bg_priority[MODE1_GBA_BG_COUNT][MODE1_GBA_WIDTH],
    uint32_t obj_layer[MODE1_GBA_WIDTH],
    uint8_t obj_priority[MODE1_GBA_WIDTH],
    uint16_t dispcnt)
{
    uint32_t backdrop_color = virtuappu_mode1_rgb555_to_abgr8888(mode1_memory.bg_palette[0]);
    bool bg_enabled[MODE1_GBA_BG_COUNT] = {
        (dispcnt & MODE1_DISP_BG0_ON) != 0u,
        (dispcnt & MODE1_DISP_BG1_ON) != 0u,
        (dispcnt & MODE1_DISP_BG2_ON) != 0u,
        (dispcnt & MODE1_DISP_BG3_ON) != 0u
    };
    bool obj_enabled = (dispcnt & MODE1_DISP_OBJ_ON) != 0u;
    uint16_t bldcnt = virtuappu_mode1_io_read16(MODE1_IO_BLDCNT);
    uint16_t bldalpha = virtuappu_mode1_io_read16(MODE1_IO_BLDALPHA);
    uint16_t bldy = virtuappu_mode1_io_read16(MODE1_IO_BLDY);
    Mode1BlendEffect effect = (Mode1BlendEffect)((bldcnt >> 6u) & 3u);
    int eva = bldalpha & 0x1Fu;
    int evb = (bldalpha >> 8u) & 0x1Fu;
    int evy = bldy & 0x1Fu;
    uint8_t bg_order[MODE1_GBA_BG_COUNT] = {0, 1, 2, 3};
    uint8_t bg_order_priority[MODE1_GBA_BG_COUNT];
    bool win0_on = (dispcnt & MODE1_DISP_WIN0_ON) != 0u;
    bool win1_on = (dispcnt & MODE1_DISP_WIN1_ON) != 0u;
    bool any_window = win0_on || win1_on;
    uint16_t winin = virtuappu_mode1_io_read16(MODE1_IO_WININ);
    uint16_t winout = virtuappu_mode1_io_read16(MODE1_IO_WINOUT);
    uint16_t win0h = virtuappu_mode1_io_read16(MODE1_IO_WIN0H);
    uint16_t win0v = virtuappu_mode1_io_read16(MODE1_IO_WIN0V);
    int win0_left = win0h >> 8u;
    int win0_right = win0h & 0xFFu;
    int win0_top = win0v >> 8u;
    int win0_bottom = win0v & 0xFFu;
    bool win0_h_wrap;
    bool win0_v_active;
    uint16_t win1h = virtuappu_mode1_io_read16(MODE1_IO_WIN1H);
    uint16_t win1v = virtuappu_mode1_io_read16(MODE1_IO_WIN1V);
    int win1_left = win1h >> 8u;
    int win1_right = win1h & 0xFFu;
    int win1_top = win1v >> 8u;
    int win1_bottom = win1v & 0xFFu;
    bool win1_h_wrap;
    bool win1_v_active;
    uint8_t win0_ctrl = (uint8_t)(winin & 0x3Fu);
    uint8_t win1_ctrl = (uint8_t)((winin >> 8u) & 0x3Fu);
    uint8_t outside_ctrl = (uint8_t)(winout & 0x3Fu);
    int i;
    int x;

    (void)bg_priority;

    if (eva > 16) {
        eva = 16;
    }
    if (evb > 16) {
        evb = 16;
    }
    if (evy > 16) {
        evy = 16;
    }

    if (win0_right > MODE1_GBA_WIDTH) {
        win0_right = MODE1_GBA_WIDTH;
    }
    if (win0_bottom > MODE1_GBA_HEIGHT) {
        win0_bottom = MODE1_GBA_HEIGHT;
    }
    if (win1_right > MODE1_GBA_WIDTH) {
        win1_right = MODE1_GBA_WIDTH;
    }
    if (win1_bottom > MODE1_GBA_HEIGHT) {
        win1_bottom = MODE1_GBA_HEIGHT;
    }

    win0_h_wrap = win0_left > win0_right;
    win0_v_active = win0_on && win0_top <= win0_bottom && line >= win0_top && line < win0_bottom;
    win1_h_wrap = win1_left > win1_right;
    win1_v_active = win1_on && win1_top <= win1_bottom && line >= win1_top && line < win1_bottom;

    for (i = 0; i < MODE1_GBA_BG_COUNT; ++i) {
        bg_order_priority[i] = (uint8_t)(virtuappu_mode1_io_read16((uint16_t)(MODE1_IO_BG0CNT + i * 2)) & 3u);
    }

    for (i = 0; i < MODE1_GBA_BG_COUNT - 1; ++i) {
        int j;
        for (j = i + 1; j < MODE1_GBA_BG_COUNT; ++j) {
            if (bg_order_priority[bg_order[j]] < bg_order_priority[bg_order[i]]) {
                uint8_t tmp = bg_order[i];
                bg_order[i] = bg_order[j];
                bg_order[j] = tmp;
            }
        }
    }

    for (x = 0; x < MODE1_GBA_WIDTH; ++x) {
        uint8_t win_ctrl = 0x3Fu;
        bool visible_bg[MODE1_GBA_BG_COUNT];
        bool visible_obj;
        bool allow_sfx;
        uint32_t top_color = backdrop_color;
        int top_layer = 5;
        uint32_t bottom_color = backdrop_color;
        int bottom_layer = 5;
        bool found_top = false;
        bool found_bottom = false;
        int priority;

        if (any_window) {
            win_ctrl = outside_ctrl;
            if (win1_v_active) {
                bool in_h = win1_h_wrap ? (x >= win1_left || x < win1_right) : (x >= win1_left && x < win1_right);
                if (in_h) {
                    win_ctrl = win1_ctrl;
                }
            }
            if (win0_v_active) {
                bool in_h = win0_h_wrap ? (x >= win0_left || x < win0_right) : (x >= win0_left && x < win0_right);
                if (in_h) {
                    win_ctrl = win0_ctrl;
                }
            }
        }

        visible_bg[0] = (win_ctrl & 0x01u) != 0u;
        visible_bg[1] = (win_ctrl & 0x02u) != 0u;
        visible_bg[2] = (win_ctrl & 0x04u) != 0u;
        visible_bg[3] = (win_ctrl & 0x08u) != 0u;
        visible_obj = (win_ctrl & 0x10u) != 0u;
        allow_sfx = (win_ctrl & 0x20u) != 0u;

        for (priority = 0; priority <= 3 && !found_bottom; ++priority) {
            int order_index;

            if (obj_enabled && visible_obj && obj_layer[x] != 0u && obj_priority[x] == priority) {
                if (!found_top) {
                    top_color = obj_layer[x];
                    top_layer = 4;
                    found_top = true;
                } else if (!found_bottom) {
                    bottom_color = obj_layer[x];
                    bottom_layer = 4;
                    found_bottom = true;
                }
            }

            for (order_index = 0; order_index < MODE1_GBA_BG_COUNT; ++order_index) {
                int bg = bg_order[order_index];
                if (!bg_enabled[bg] || !visible_bg[bg]) {
                    continue;
                }
                if (bg_order_priority[bg] != priority) {
                    continue;
                }
                if (bg_layers[bg][x] == 0u) {
                    continue;
                }

                if (!found_top) {
                    top_color = bg_layers[bg][x];
                    top_layer = bg;
                    found_top = true;
                } else if (!found_bottom) {
                    bottom_color = bg_layers[bg][x];
                    bottom_layer = bg;
                    found_bottom = true;
                    break;
                }
            }
        }

        if (allow_sfx) {
            switch (effect) {
            case MODE1_BLEND_ALPHA:
                if (mode1_is_first_target(bldcnt, top_layer) && mode1_is_second_target(bldcnt, bottom_layer)) {
                    top_color = mode1_alpha_blend(top_color, bottom_color, eva, evb);
                }
                break;
            case MODE1_BLEND_BRIGHTEN:
                if (mode1_is_first_target(bldcnt, top_layer)) {
                    top_color = mode1_brighten(top_color, evy);
                }
                break;
            case MODE1_BLEND_DARKEN:
                if (mode1_is_first_target(bldcnt, top_layer)) {
                    top_color = mode1_darken(top_color, evy);
                }
                break;
            default:
                break;
            }
        }

        virtuappu_frame_buffer[(size_t)line * MODE1_GBA_WIDTH + (size_t)x] = top_color;
    }
}

void virtuappu_mode1_render_frame(const PPUMemory *ppu)
{
    uint16_t dispcnt;
    int line;

    (void)ppu;

    dispcnt = virtuappu_mode1_io_read16(MODE1_IO_DISPCNT);
    if ((dispcnt & MODE1_DISP_FORCED_BLANK) != 0u) {
        memset(virtuappu_frame_buffer, 0xFF, MODE1_GBA_WIDTH * MODE1_GBA_HEIGHT * sizeof(uint32_t));
        return;
    }

    for (line = 0; line < MODE1_GBA_HEIGHT; ++line) {
        if (virtuappu_mode1_pre_line_callback != NULL) {
            virtuappu_mode1_pre_line_callback(line);
        }
        uint32_t bg_layers[MODE1_GBA_BG_COUNT][MODE1_GBA_WIDTH];
        uint8_t bg_priority[MODE1_GBA_BG_COUNT][MODE1_GBA_WIDTH];
        uint32_t obj_layer[MODE1_GBA_WIDTH];
        uint8_t obj_priority[MODE1_GBA_WIDTH];
        bool obj_1d = (dispcnt & MODE1_DISP_OBJ_1D) != 0u;

        memset(bg_layers, 0, sizeof(bg_layers));
        memset(bg_priority, 0, sizeof(bg_priority));
        memset(obj_layer, 0, sizeof(obj_layer));
        memset(obj_priority, 0xFF, sizeof(obj_priority));

        if ((dispcnt & MODE1_DISP_BG0_ON) != 0u) {
            virtuappu_mode1_render_text_bg_line(0, line, bg_layers[0], bg_priority[0]);
        }
        if ((dispcnt & MODE1_DISP_BG1_ON) != 0u) {
            virtuappu_mode1_render_text_bg_line(1, line, bg_layers[1], bg_priority[1]);
        }
        if ((dispcnt & MODE1_DISP_BG2_ON) != 0u) {
            virtuappu_mode1_render_text_bg_line(2, line, bg_layers[2], bg_priority[2]);
        }
        if ((dispcnt & MODE1_DISP_BG3_ON) != 0u) {
            virtuappu_mode1_render_text_bg_line(3, line, bg_layers[3], bg_priority[3]);
        }
        if ((dispcnt & MODE1_DISP_OBJ_ON) != 0u) {
            virtuappu_mode1_render_obj_line(line, obj_1d, obj_layer, obj_priority);
        }

        virtuappu_mode1_composite_line(line, bg_layers, bg_priority, obj_layer, obj_priority, dispcnt);
    }
}
