#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SDL_Window SDL_Window;
typedef struct SDL_Renderer SDL_Renderer;
typedef struct SDL_Texture SDL_Texture;
typedef struct SDL_Surface SDL_Surface;
typedef struct SDL_AudioStream SDL_AudioStream;
typedef struct SDL_Gamepad SDL_Gamepad;
typedef struct SDL_Joystick SDL_Joystick;

typedef int32_t SDL_Keycode;
typedef int32_t SDL_Scancode;

#define SDLK_UNKNOWN 0
#define SDLK_BACKSLASH 92
#define SDL_SCANCODE_UNKNOWN 0

enum {
    SDL_GAMEPAD_BUTTON_INVALID = -1,
    SDL_GAMEPAD_BUTTON_COUNT = 16,
};

typedef int SDL_GamepadButton;
typedef int SDL_GamepadAxis;
typedef int SDL_JoystickID;

#define SDL_GAMEPAD_AXIS_INVALID -1
#define SDL_GAMEPAD_AXIS_COUNT 6
#define SDL_GAMEPAD_AXIS_LEFT_TRIGGER 4
#define SDL_GAMEPAD_AXIS_RIGHT_TRIGGER 5

#define SDL_INIT_VIDEO 0x00000020u
#define SDL_INIT_AUDIO 0x00000010u
#define SDL_INIT_JOYSTICK 0x00000200u
#define SDL_INIT_GAMEPAD 0x00002000u

#define SDL_WINDOW_RESIZABLE 0x00000020u
#define SDL_WINDOW_FULLSCREEN 0x00000001u
#define SDL_PIXELFORMAT_ABGR8888 0
#define SDL_TEXTUREACCESS_STREAMING 1
#define SDL_SCALEMODE_NEAREST 0
#define SDL_SCALEMODE_LINEAR 1
#define SDL_MESSAGEBOX_ERROR 0x00000010u
#define SDL_AUDIO_S16LE 0
#define SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK 0
#define SDL_HINT_VIDEO_DRIVER "SDL_VIDEO_DRIVER"
#define SDL_HINT_RENDER_DRIVER "SDL_RENDER_DRIVER"
#define SDL_HINT_AUDIO_DRIVER "SDL_AUDIO_DRIVER"
#define SDL_HINT_JOYSTICK_HIDAPI "SDL_JOYSTICK_HIDAPI"
#define SDL_HINT_GAMECONTROLLER_USE_BUTTON_LABELS "SDL_GAMECONTROLLER_USE_BUTTON_LABELS"

enum {
    SDL_EVENT_QUIT = 0x100,
    SDL_EVENT_KEY_DOWN,
    SDL_EVENT_GAMEPAD_ADDED,
    SDL_EVENT_GAMEPAD_REMOVED,
    SDL_EVENT_GAMEPAD_BUTTON_DOWN,
    SDL_EVENT_GAMEPAD_AXIS_MOTION,
    SDL_EVENT_JOYSTICK_ADDED,
    SDL_EVENT_JOYSTICK_REMOVED,
};

typedef union SDL_Event {
    uint32_t type;
    struct { uint32_t type; } common;
    struct { uint32_t type; SDL_Keycode key; struct { int repeat; } key; } key;
    struct { uint32_t type; SDL_JoystickID which; } gdevice;
    struct { uint32_t type; SDL_JoystickID which; uint8_t button; } gbutton;
    struct { uint32_t type; SDL_JoystickID which; uint8_t axis; int16_t value; } gaxis;
} SDL_Event;

typedef struct SDL_AudioSpec {
    int freq;
    int format;
    int channels;
} SDL_AudioSpec;

typedef uint32_t SDL_WindowFlags;
typedef void* SDL_GLContext;

/* ── Real timing via clock_gettime ──────────────────────────────────── */
static inline uint64_t SDL_GetTicksNS(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static inline uint32_t SDL_GetTicks(void) {
    return (uint32_t)(SDL_GetTicksNS() / 1000000ULL);
}

static inline void SDL_Delay(uint32_t ms) {
    struct timespec ts = { ms / 1000, (ms % 1000) * 1000000 };
    nanosleep(&ts, NULL);
}

/* ── Stub macro stubs ───────────────────────────────────────────────── */
#define SDL_ShowSimpleMessageBox(...) 0
#define SDL_Init(...) 1
#define SDL_InitSubSystem(...) 1
#define SDL_Quit()
#define SDL_QuitSubSystem(...)
#define SDL_SetHint(...) 1
#define SDL_ResetHint(...)
#define SDL_GetCurrentVideoDriver() "android"
#define SDL_GetVideoDriver(i) "android"
#define SDL_GetNumVideoDrivers() 1
#define SDL_CreateWindowAndRenderer(t,w,f,wp,rp) 0
#define SDL_CreateWindow(...) NULL
#define SDL_CreateRenderer(...) NULL
#define SDL_GetRenderer(...) NULL
#define SDL_ShowWindow(w)
#define SDL_RaiseWindow(w)
#define SDL_SyncWindow(w)
#define SDL_DestroyWindow(w)
#define SDL_DestroyRenderer(r)
#define SDL_GetWindowSurface(w) NULL
#define SDL_CreateSurfaceFrom(...) NULL
#define SDL_FillSurfaceRect(...)
#define SDL_BlitSurfaceScaled(...)
#define SDL_UpdateWindowSurface(...)
#define SDL_DestroySurface(...)
#define SDL_DestroyTexture(...)
#define SDL_SetRenderVSync(...) 1
#define SDL_CreateTexture(...) NULL
#define SDL_UpdateTexture(...)
#define SDL_SetTextureScaleMode(...)
#define SDL_SetRenderDrawColor(...)
#define SDL_RenderClear(...)
#define SDL_RenderTexture(...)
#define SDL_RenderPresent(...)
#define SDL_GetCurrentRenderOutputSize(...) 0
#define SDL_GetWindowFlags(...) 0
#define SDL_SetWindowFullscreen(...)
#define SDL_SetWindowSize(...)
#define SDL_SetWindowTitle(...)
#define SDL_GetWindowTitle(...) ""
#define SDL_SetWindowSurfaceVSync(...) 1
#define SDL_PollEvent(...) 0
#define SDL_GetKeyboardState(...) NULL
#define SDL_GetScancodeFromKey(...) 0
#define SDL_UpdateGamepads()
#define SDL_GetGamepads(...) NULL
#define SDL_IsGamepad(...) 0
#define SDL_OpenGamepad(...) NULL
#define SDL_CloseGamepad(...)
#define SDL_GetGamepadID(...) 0
#define SDL_GetGamepadName(...) ""
#define SDL_GetGamepadNameForID(...) ""
#define SDL_GetGamepadButton(...) 0
#define SDL_GetGamepadAxis(...) 0
#define SDL_OpenAudioDeviceStream(...) NULL
#define SDL_ResumeAudioStreamDevice(...) 1
#define SDL_PutAudioStreamData(...)
#define SDL_DestroyAudioStream(...)
#define SDL_GetError(...) ""
#define SDL_Log(...)
#define SDL_zero(x)
#define SDL_free(p) free(p)
#define SDL_RenderDebugText(...)
#define SDL_snprintf snprintf
#define SDL_GetAudioDriver(i) ""
#define SDL_GetCurrentAudioDriver() ""

#ifdef __cplusplus
}
#endif
