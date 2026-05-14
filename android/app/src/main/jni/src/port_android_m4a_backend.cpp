#include "port_m4a_backend.h"
#include <cstring>
#include <cstdio>

/* Minimal m4a audio backend for Android.
 * Routes through agbplay's MP2K renderer. */

extern "C" {
#include "agbplay_core.h"
}

static bool sInitialized = false;

bool Port_M4A_Backend_Init(uint32_t sampleRate) {
    if (sInitialized) return true;
    agbplay_engine_init(sampleRate, 0); // 0 = no logging
    sInitialized = true;
    return true;
}

void Port_M4A_Backend_Render(int16_t* buffer, uint32_t numFrames, bool mute) {
    if (mute) {
        std::memset(buffer, 0, numFrames * 2 * sizeof(int16_t));
        return;
    }
    agbplay_engine_render(buffer, numFrames);
}

void Port_M4A_Backend_Reset(void) {
    agbplay_engine_reset();
}

void Port_M4A_Backend_Shutdown(void) {
    if (sInitialized) {
        agbplay_engine_shutdown();
        sInitialized = false;
    }
}
