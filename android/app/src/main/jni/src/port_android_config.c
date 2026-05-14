#include "port_runtime_config.h"
#include "port_android_input.h"
#include <android/log.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LOG_TAG "TMC-Cfg"

/* Android config: bridges touch input to GBA button states.
 * Port_Config_InputPressed is called from port_bios.c each frame. */

static int sWindowScale = 1;
static int sTargetFps = 60;
static char sUpscaleMethod[32] = "nearest";
static int sPortSettingsMenu = 1;
static int sInternalScale = 1;

static const struct {
    PortInput input;
    uint16_t gbaMask;
} sInputMap[] = {
    { PORT_INPUT_A,      0x0001 },
    { PORT_INPUT_B,      0x0002 },
    { PORT_INPUT_SELECT, 0x0004 },
    { PORT_INPUT_START,  0x0008 },
    { PORT_INPUT_RIGHT,  0x0010 },
    { PORT_INPUT_LEFT,   0x0020 },
    { PORT_INPUT_UP,     0x0040 },
    { PORT_INPUT_DOWN,   0x0080 },
    { PORT_INPUT_R,      0x0100 },
    { PORT_INPUT_L,      0x0200 },
};

/* Derive GBA bitmask from GBA_KEYINPUT register (active-low).
 * sKeyState from port_android_input.c: 0 = pressed, 1 = released. */
extern uint16_t Port_Android_GetKeyInput(void);

void Port_Config_Load(const char* path) {
    (void)path;
}

u8 Port_Config_WindowScale(void) {
    return sWindowScale;
}

const char* Port_Config_UpscaleMethod(void) {
    return sUpscaleMethod;
}

u64 Port_Config_FrameTimeNs(void) {
    return sTargetFps > 0 ? 1000000000ULL / (u64)sTargetFps : 0;
}

u32 Port_Config_TargetFps(void) {
    return (u32)sTargetFps;
}

bool Port_Config_PortSettingsMenuEnabled(void) {
    return (bool)sPortSettingsMenu;
}

void Port_Config_SetWindowScale(u8 scale) {
    sWindowScale = scale < 1 ? 1 : (scale > 10 ? 10 : (int)scale);
}

void Port_Config_SetUpscaleMethod(const char* method) {
    strncpy(sUpscaleMethod, method ? method : "nearest", sizeof(sUpscaleMethod) - 1);
}

void Port_Config_SetTargetFps(u32 fps) {
    sTargetFps = (int)fps;
}

void Port_Config_CycleTargetFps(int direction) {
    static const u32 presets[] = {0, 30, 60, 90, 120, 144};
    u32 cur = (u32)sTargetFps;
    int idx = 0;
    u32 best = 999999;
    for (int i = 0; i < 6; i++) {
        u32 d = cur > presets[i] ? cur - presets[i] : presets[i] - cur;
        if (d < best) { best = d; idx = i; }
    }
    idx = (idx + (direction < 0 ? -1 : 1) + 6) % 6;
    sTargetFps = (int)presets[idx];
}

u8 Port_Config_InternalScale(void) {
    return (u8)sInternalScale;
}

void Port_Config_SetInternalScale(u8 scale) {
    sInternalScale = scale < 1 ? 1 : (scale > 4 ? 4 : (int)scale);
}

void Port_Config_CycleInternalScale(int direction) {
    sInternalScale += direction < 0 ? -1 : 1;
    if (sInternalScale < 1) sInternalScale = 4;
    if (sInternalScale > 4) sInternalScale = 1;
}

void Port_Config_OpenGamepads(void) {}
void Port_Config_CloseGamepads(void) {}
void Port_Config_ClearInputEdges(void) {}

bool Port_Config_InputPressed(PortInput input) {
    uint16_t keys = Port_Android_GetKeyInput();
    for (size_t i = 0; i < sizeof(sInputMap) / sizeof(sInputMap[0]); i++) {
        if (sInputMap[i].input == input) {
            return (keys & sInputMap[i].gbaMask) == 0;
        }
    }
    return false;
}

bool Port_Config_SoftSlotPressed(int slot) {
    (void)slot;
    return false;
}

void Port_Config_HandleEvent(const SDL_Event* e) {
    (void)e;
}
