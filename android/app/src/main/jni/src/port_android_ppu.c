#include "port_ppu.h"
#include "port_gba_mem.h"
#include "port_hdma.h"
#include "port_android_render.h"
#include <cpu/mode1.h>
#include <virtuappu.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DBG(...) do { \
    FILE* _f = fopen("/storage/emulated/0/Android/data/org.tmc/files/debug.log", "a"); \
    if (_f) { fprintf(_f, "[PPU] " __VA_ARGS__); fprintf(_f, "\n"); fclose(_f); } \
} while(0)

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
}

void Port_PPU_PresentFrame(void) {
    DBG("PresentFrame start, sInitialized=%d", sInitialized);
    if (!sInitialized) return;

    uint16_t dispcnt = (uint16_t)(gIoMem[0x00] | (gIoMem[0x01] << 8));
    DBG("dispcnt=0x%04X", dispcnt);
    uint8_t gbaMode = (uint8_t)(dispcnt & 0x07);
    DBG("gbaMode=%d", gbaMode);

    switch (gbaMode) {
        case 0: virtuappu_registers.mode = 1; break;
        case 1:
        case 2: virtuappu_registers.mode = 2; break;
        default: virtuappu_registers.mode = 1; break;
    }

    DBG("calling virtuappu_render_frame...");
    virtuappu_render_frame();
    DBG("calling Port_PresentFrameGL...");
    Port_PresentFrameGL();
    DBG("PresentFrame done");
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
