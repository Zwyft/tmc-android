#include "virtuappu.h"

#include <string.h>

#include "modes_impl.h"

uint32_t virtuappu_frame_buffer[VIRTUAPPU_FRAME_BUFFER_SIZE];
uint8_t virtuappu_vram[VIRTUAPPU_VRAM_SIZE];
PPUMemory virtuappu_registers;

void virtuappu_reset(void)
{
    memset(virtuappu_frame_buffer, 0, sizeof(virtuappu_frame_buffer));
    memset(virtuappu_vram, 0, sizeof(virtuappu_vram));
    memset(&virtuappu_registers, 0, sizeof(virtuappu_registers));
}

void virtuappu_render_frame(void)
{
    switch (virtuappu_registers.mode) {
    case 0:
        virtuappu_mode0_render_frame(&virtuappu_registers);
        break;
    case 1:
        virtuappu_mode1_render_frame(&virtuappu_registers);
        break;
    case 2:
        virtuappu_mode2_render_frame(&virtuappu_registers);
        break;
    case 7:
        virtuappu_mode7_render_frame(&virtuappu_registers);
        break;
    default:
        break;
    }
}

uint32_t *virtuappu_get_frame_buffer(void)
{
    return virtuappu_frame_buffer;
}

uint8_t *virtuappu_get_vram(void)
{
    return virtuappu_vram;
}

PPUMemory *virtuappu_get_registers(void)
{
    return &virtuappu_registers;
}
