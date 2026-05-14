#include "port_ppu.h"
#include "port_gba_mem.h"
#include "port_hdma.h"
#include "port_android_render.h"
#include <cpu/mode1.h>
#include <virtuappu.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Android PPU implementation.
 * ViruaPPU renders to virtuappu_frame_buffer (240x160 RGBA).
 * We upload it as an OpenGL ES texture and blit to screen. */

static int sInitialized = 0;

void Port_PPU_Init(SDL_Window* window) {
    (void)window;

    VirtuaPPUMode1GbaMemory memory = {
        gIoMem, gVram, gBgPltt, gObjPltt, gOamMem,
    };
    virtuappu_mode1_bind_gba_memory(&memory);
    virtuappu_mode1_pre_line_callback = port_hdma_step_line;
    virtuappu_registers.frame_width = 240;
    virtuappu_registers.mode = 1;

    sInitialized = 1;
    printf("Android PPU initialized\n");
}

void Port_PPU_PresentFrame(void) {
    if (!sInitialized) return;

    uint16_t dispcnt = (uint16_t)(gIoMem[0x00] | (gIoMem[0x01] << 8));
    uint8_t gbaMode = (uint8_t)(dispcnt & 0x07);

    switch (gbaMode) {
        case 0: virtuappu_registers.mode = 1; break;
        case 1:
        case 2: virtuappu_registers.mode = 2; break;
        default: virtuappu_registers.mode = 1; break;
    }

    virtuappu_render_frame();
    Port_PresentFrameGL();
}

void Port_PPU_SetWindowTitle(const char* title) {
    (void)title;
}

void Port_PPU_ToggleFullscreen(void) {}
bool Port_PPU_IsFullscreen(void) { return false; }
void Port_PPU_CycleWindowScale(int direction) { (void)direction; }
unsigned char Port_PPU_WindowScale(void) { return 1; }
void Port_PPU_ToggleSmoothing(void) {}
void Port_PPU_CyclePresentationMode(int direction) { (void)direction; }
const char* Port_PPU_PresentationModeName(void) { return "nearest"; }
void Port_PPU_CycleFilter(int direction) { (void)direction; }
const char* Port_PPU_FilterName(void) { return "none"; }
void Port_PPU_SetVSync(bool enabled) { (void)enabled; }

void Port_PPU_Shutdown(void) {
    sInitialized = 0;
}
