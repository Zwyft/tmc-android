#include "port_android_input.h"
#include <android/log.h>
#include <string.h>

#define LOG_TAG "TMC-Input"

#define GBA_A      0x0001
#define GBA_B      0x0002
#define GBA_SELECT 0x0004
#define GBA_START  0x0008
#define GBA_RIGHT  0x0010
#define GBA_LEFT   0x0020
#define GBA_UP     0x0040
#define GBA_DOWN   0x0080
#define GBA_R      0x0100
#define GBA_L      0x0200

static uint16_t sKeyState = 0x03FF; /* 1 = released */
static int sScreenW = 240, sScreenH = 160;

/* Android key codes mapping */
enum {
    AKEYCODE_DPAD_UP    = 19,
    AKEYCODE_DPAD_DOWN  = 20,
    AKEYCODE_DPAD_LEFT  = 21,
    AKEYCODE_DPAD_RIGHT = 22,
    AKEYCODE_DPAD_CENTER = 23,
    AKEYCODE_A = 29,
    AKEYCODE_S = 30,
    AKEYCODE_B = 30,
    AKEYCODE_X = 31,
    AKEYCODE_Y = 32,
    AKEYCODE_Z = 54,
    AKEYCODE_SHIFT_LEFT = 59,
    AKEYCODE_ENTER = 66,
    AKEYCODE_SPACE = 62,
    AKEYCODE_BACK = 4,
    AKEYCODE_Q = 45,
    AKEYCODE_W = 51,
    AKEYCODE_E = 33,
    AKEYCODE_R = 46,
};

/* Touch overlay button layout */
typedef struct {
    int x, y, w, h;
    uint16_t mask;
} TouchButton;

#define BTN_SIZE 48
#define BTN_MARGIN 8
#define SCREEN_Y_OFFSET 16

static TouchButton sButtons[] = {
    { 240 - BTN_SIZE - BTN_MARGIN, 160 - BTN_SIZE - BTN_MARGIN, BTN_SIZE, BTN_SIZE, GBA_A },
    { 240 - BTN_SIZE * 2 - BTN_MARGIN * 2 - 4, 160 - BTN_SIZE - BTN_MARGIN, BTN_SIZE, BTN_SIZE, GBA_B },
    { 240 - BTN_SIZE - BTN_MARGIN, 160 - BTN_SIZE * 2 - BTN_MARGIN * 2 - 4, BTN_SIZE, BTN_SIZE, GBA_B },
    { BTN_MARGIN, 160 - BTN_SIZE - BTN_MARGIN, BTN_SIZE, BTN_SIZE, GBA_LEFT },
    { BTN_SIZE + BTN_MARGIN + 4, 160 - BTN_SIZE - BTN_MARGIN, BTN_SIZE, BTN_SIZE, GBA_DOWN },
    { BTN_SIZE * 2 + BTN_MARGIN * 2 + 8, 160 - BTN_SIZE - BTN_MARGIN, BTN_SIZE, BTN_SIZE, GBA_RIGHT },
    { BTN_SIZE + BTN_MARGIN + 4, 160 - BTN_SIZE * 2 - BTN_MARGIN * 2 - 4, BTN_SIZE, BTN_SIZE, GBA_UP },
    { BTN_MARGIN + BTN_SIZE / 2, BTN_MARGIN, BTN_SIZE, BTN_SIZE, GBA_START },
    { BTN_MARGIN + BTN_SIZE / 2 + BTN_SIZE + 8, BTN_MARGIN, BTN_SIZE, BTN_SIZE, GBA_SELECT },
    { 240 - BTN_SIZE - BTN_MARGIN, SCREEN_Y_OFFSET, BTN_SIZE, BTN_SIZE, GBA_L },
    { 240 - BTN_MARGIN, 160 - BTN_SIZE - BTN_MARGIN + BTN_SIZE / 2, BTN_SIZE / 2, BTN_SIZE / 2, GBA_R },
};

#define NUM_BUTTONS (sizeof(sButtons) / sizeof(sButtons[0]))

#define MAX_TOUCH_POINTERS 16
static struct { int active, x, y; } sPointers[MAX_TOUCH_POINTERS];

static int in_rect(int px, int py, int rx, int ry, int rw, int rh) {
    return px >= rx && px < rx + rw && py >= ry && py < ry + rh;
}

static float scale_x(int x) {
    return (float)x / (float)sScreenW * 240.0f;
}

static float scale_y(int y) {
    return (float)y / (float)sScreenH * 160.0f;
}

static void update_touch_state(void) {
    /* Start with all buttons released */
    uint16_t newState = 0x03FF;

    for (int p = 0; p < MAX_TOUCH_POINTERS; p++) {
        if (!sPointers[p].active) continue;
        int tx = (int)scale_x(sPointers[p].x);
        int ty = (int)scale_y(sPointers[p].y);

        for (size_t i = 0; i < NUM_BUTTONS; i++) {
            if (in_rect(tx, ty, sButtons[i].x, sButtons[i].y,
                        sButtons[i].w, sButtons[i].h)) {
                newState &= ~sButtons[i].mask;
            }
        }
    }

    sKeyState = newState;
}

void Port_Android_TouchDown(int id, int x, int y) {
    if (id >= 0 && id < MAX_TOUCH_POINTERS) {
        sPointers[id].active = 1;
        sPointers[id].x = x;
        sPointers[id].y = y;
    }
    update_touch_state();
}

void Port_Android_TouchMove(int id, int x, int y) {
    if (id >= 0 && id < MAX_TOUCH_POINTERS) {
        sPointers[id].x = x;
        sPointers[id].y = y;
    }
    update_touch_state();
}

void Port_Android_TouchUp(int id, int x, int y) {
    if (id >= 0 && id < MAX_TOUCH_POINTERS) {
        sPointers[id].active = 0;
    }
    update_touch_state();
}

static void set_key(uint16_t mask, int pressed) {
    if (pressed)
        sKeyState &= ~mask;
    else
        sKeyState |= mask;
}

void Port_Android_KeyDown(int keyCode) {
    switch (keyCode) {
        case AKEYCODE_DPAD_UP:    set_key(GBA_UP, 1); break;
        case AKEYCODE_DPAD_DOWN:  set_key(GBA_DOWN, 1); break;
        case AKEYCODE_DPAD_LEFT:  set_key(GBA_LEFT, 1); break;
        case AKEYCODE_DPAD_RIGHT: set_key(GBA_RIGHT, 1); break;
        case AKEYCODE_Z:
        case AKEYCODE_A:          set_key(GBA_A, 1); break;
        case AKEYCODE_X:
        case AKEYCODE_S:          set_key(GBA_B, 1); break;
        case AKEYCODE_ENTER:
        case AKEYCODE_SPACE:      set_key(GBA_START, 1); break;
        case AKEYCODE_SHIFT_LEFT:
        case AKEYCODE_BACK:       set_key(GBA_SELECT, 1); break;
        case AKEYCODE_Q:          set_key(GBA_L, 1); break;
        case AKEYCODE_W:          set_key(GBA_R, 1); break;
    }
}

void Port_Android_KeyUp(int keyCode) {
    switch (keyCode) {
        case AKEYCODE_DPAD_UP:    set_key(GBA_UP, 0); break;
        case AKEYCODE_DPAD_DOWN:  set_key(GBA_DOWN, 0); break;
        case AKEYCODE_DPAD_LEFT:  set_key(GBA_LEFT, 0); break;
        case AKEYCODE_DPAD_RIGHT: set_key(GBA_RIGHT, 0); break;
        case AKEYCODE_Z:
        case AKEYCODE_A:          set_key(GBA_A, 0); break;
        case AKEYCODE_X:
        case AKEYCODE_S:          set_key(GBA_B, 0); break;
        case AKEYCODE_ENTER:
        case AKEYCODE_SPACE:      set_key(GBA_START, 0); break;
        case AKEYCODE_SHIFT_LEFT:
        case AKEYCODE_BACK:       set_key(GBA_SELECT, 0); break;
        case AKEYCODE_Q:          set_key(GBA_L, 0); break;
        case AKEYCODE_W:          set_key(GBA_R, 0); break;
    }
}

uint16_t Port_Android_GetKeyInput(void) {
    return sKeyState;
}

void Port_Android_SetScreenSize(int w, int h) {
    sScreenW = w;
    sScreenH = h;
}
