#include "port_touch_controls.h"
#include "port_runtime_config.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

#ifdef __ANDROID__

extern "C" {
const unsigned char kEmbeddedGbaSpShellPng[] = {
#include "gba-sp-zelda.png.h"
};
const std::size_t kEmbeddedGbaSpShellPngSize = sizeof(kEmbeddedGbaSpShellPng);
const unsigned char kEmbeddedGbaSpAButtonPng[] = {
#include "gba-sp-a.png.h"
};
const std::size_t kEmbeddedGbaSpAButtonPngSize = sizeof(kEmbeddedGbaSpAButtonPng);
const unsigned char kEmbeddedGbaSpBButtonPng[] = {
#include "gba-sp-b.png.h"
};
const std::size_t kEmbeddedGbaSpBButtonPngSize = sizeof(kEmbeddedGbaSpBButtonPng);
const unsigned char kEmbeddedGbaSpLeftPng[] = {
#include "gba-sp-left.png.h"
};
const std::size_t kEmbeddedGbaSpLeftPngSize = sizeof(kEmbeddedGbaSpLeftPng);
const unsigned char kEmbeddedGbaSpRightPng[] = {
#include "gba-sp-right.png.h"
};
const std::size_t kEmbeddedGbaSpRightPngSize = sizeof(kEmbeddedGbaSpRightPng);
const unsigned char kEmbeddedGbaSpUpPng[] = {
#include "gba-sp-up.png.h"
};
const std::size_t kEmbeddedGbaSpUpPngSize = sizeof(kEmbeddedGbaSpUpPng);
const unsigned char kEmbeddedGbaSpDownPng[] = {
#include "gba-sp-down.png.h"
};
const std::size_t kEmbeddedGbaSpDownPngSize = sizeof(kEmbeddedGbaSpDownPng);
const unsigned char kEmbeddedGbaSpStartSelectPng[] = {
#include "gba-sp-start-select.png.h"
};
const std::size_t kEmbeddedGbaSpStartSelectPngSize = sizeof(kEmbeddedGbaSpStartSelectPng);
const unsigned char kEmbeddedGbaSpMenuPng[] = {
#include "gba-sp-menu.png.h"
};
const std::size_t kEmbeddedGbaSpMenuPngSize = sizeof(kEmbeddedGbaSpMenuPng);
}

namespace {

constexpr float kCanvasWidth = 1080.0f;
constexpr float kCanvasHeight = 2400.0f;
constexpr float kScreenX = 60.0f;
/* The supplied Zelda shell PNG places its 960x640 display opening near the
 * top of the portrait canvas. The upstream RetroArch preset's y=970 refers
 * to a different overlay composition and would land over the controls here. */
constexpr float kScreenY = 179.0f;
constexpr float kScreenWidth = 960.0f;
constexpr float kScreenHeight = 640.0f;
constexpr float kMenuX = 170.0f;
constexpr float kMenuY = 1032.0f;
constexpr float kMenuRadius = 46.0f;

struct CanvasPoint {
    float x;
    float y;
};

struct HitRegion {
    float x;
    float y;
    float halfWidth;
    float halfHeight;
    uint32_t inputMask;
};

struct TouchPoint {
    int64_t id;
    float x;
    float y;
};

SDL_Renderer* sTextureRenderer = nullptr;
SDL_Texture* sShellTexture = nullptr;
SDL_Texture* sButtonATexture = nullptr;
SDL_Texture* sButtonBTexture = nullptr;
SDL_Texture* sLeftTexture = nullptr;
SDL_Texture* sRightTexture = nullptr;
SDL_Texture* sUpTexture = nullptr;
SDL_Texture* sDownTexture = nullptr;
SDL_Texture* sStartSelectTexture = nullptr;
SDL_Texture* sMenuTexture = nullptr;

bool sVisible = true;
bool sMenuOpen = false;
bool sSettingsRequested = false;
int sWindowWidth = 0;
int sWindowHeight = 0;
std::vector<TouchPoint> sTouches;
std::array<bool, PORT_INPUT_COUNT> sHeld{};

uint32_t InputMask(PortInput input) {
    return 1u << static_cast<unsigned>(input);
}

const std::array<HitRegion, 14>& HitRegions() {
    static const std::array<HitRegion, 14> regions = {{
        {162, 1715, 60, 64, InputMask(PORT_INPUT_LEFT)},
        {378, 1715, 60, 64, InputMask(PORT_INPUT_RIGHT)},
        {270, 1607, 64, 60, InputMask(PORT_INPUT_UP)},
        {270, 1823, 64, 60, InputMask(PORT_INPUT_DOWN)},
        {162, 1607, 38, 38, InputMask(PORT_INPUT_LEFT) | InputMask(PORT_INPUT_UP)},
        {378, 1607, 38, 38, InputMask(PORT_INPUT_RIGHT) | InputMask(PORT_INPUT_UP)},
        {162, 1823, 38, 38, InputMask(PORT_INPUT_LEFT) | InputMask(PORT_INPUT_DOWN)},
        {378, 1823, 38, 38, InputMask(PORT_INPUT_RIGHT) | InputMask(PORT_INPUT_DOWN)},
        {905, 1684, 66, 66, InputMask(PORT_INPUT_A)},
        {703, 1741, 66, 66, InputMask(PORT_INPUT_B)},
        {128, 1210, 128, 80, InputMask(PORT_INPUT_L)},
        {952, 1210, 128, 80, InputMask(PORT_INPUT_R)},
        {435, 2282, 35, 35, InputMask(PORT_INPUT_SELECT)},
        {641, 2282, 35, 35, InputMask(PORT_INPUT_START)},
    }};
    return regions;
}

SDL_FRect CanvasRect(int width, int height) {
    const float scale = std::min(static_cast<float>(width) / kCanvasWidth,
                                 static_cast<float>(height) / kCanvasHeight);
    const float canvasWidth = kCanvasWidth * scale;
    const float canvasHeight = kCanvasHeight * scale;
    return SDL_FRect{(static_cast<float>(width) - canvasWidth) * 0.5f,
                     (static_cast<float>(height) - canvasHeight) * 0.5f,
                     canvasWidth, canvasHeight};
}

SDL_FRect CanvasImageRect(float centerX, float centerY, float imageWidth, float imageHeight,
                          int width, int height) {
    const SDL_FRect canvas = CanvasRect(width, height);
    const float scale = canvas.w / kCanvasWidth;
    return SDL_FRect{canvas.x + (centerX - imageWidth * 0.5f) * scale,
                     canvas.y + (centerY - imageHeight * 0.5f) * scale,
                     imageWidth * scale, imageHeight * scale};
}

CanvasPoint ToCanvas(float x, float y, int width, int height) {
    const SDL_FRect canvas = CanvasRect(width, height);
    const float scale = canvas.w / kCanvasWidth;
    return CanvasPoint{(x - canvas.x) / scale, (y - canvas.y) / scale};
}

bool InEllipse(float x, float y, float cx, float cy, float rx, float ry) {
    if (rx <= 0.0f || ry <= 0.0f) {
        return false;
    }
    const float dx = (x - cx) / rx;
    const float dy = (y - cy) / ry;
    return dx * dx + dy * dy <= 1.0f;
}

void UpdateHeldState() {
    sHeld.fill(false);
    if (!sVisible || sMenuOpen || sWindowWidth <= 0 || sWindowHeight <= 0) {
        return;
    }

    for (const TouchPoint& touch : sTouches) {
        const CanvasPoint point = ToCanvas(touch.x, touch.y, sWindowWidth, sWindowHeight);
        for (const HitRegion& region : HitRegions()) {
            if (InEllipse(point.x, point.y, region.x, region.y,
                          region.halfWidth, region.halfHeight)) {
                for (int input = 0; input < PORT_INPUT_COUNT; ++input) {
                    if ((region.inputMask & (1u << static_cast<unsigned>(input))) != 0) {
                        sHeld[input] = true;
                    }
                }
            }
        }
    }
}

void AddOrUpdateTouch(int64_t id, float x, float y) {
    sVisible = true;
    for (TouchPoint& touch : sTouches) {
        if (touch.id == id) {
            touch.x = x;
            touch.y = y;
            UpdateHeldState();
            return;
        }
    }
    sTouches.push_back(TouchPoint{id, x, y});
    UpdateHeldState();
}

void RemoveTouch(int64_t id) {
    sTouches.erase(std::remove_if(sTouches.begin(), sTouches.end(),
                                  [id](const TouchPoint& touch) { return touch.id == id; }),
                   sTouches.end());
    UpdateHeldState();
}

void TryTriggerSettings(float x, float y) {
    if (!sVisible || sWindowWidth <= 0 || sWindowHeight <= 0) {
        return;
    }
    const CanvasPoint point = ToCanvas(x, y, sWindowWidth, sWindowHeight);
    if (InEllipse(point.x, point.y, kMenuX, kMenuY, kMenuRadius, kMenuRadius)) {
        sSettingsRequested = true;
    }
}

SDL_Texture* LoadEmbeddedTexture(SDL_Renderer* renderer, const unsigned char* bytes, std::size_t size) {
    if (!renderer || !bytes || size == 0) {
        return nullptr;
    }
    SDL_IOStream* io = SDL_IOFromConstMem(bytes, size);
    if (!io) {
        return nullptr;
    }
    SDL_Surface* surface = SDL_LoadPNG_IO(io, true);
    if (!surface) {
        std::fprintf(stderr, "[touch-controls] PNG decode failed: %s\n", SDL_GetError());
        return nullptr;
    }
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_DestroySurface(surface);
    if (!texture) {
        std::fprintf(stderr, "[touch-controls] texture creation failed: %s\n", SDL_GetError());
        return nullptr;
    }
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_LINEAR);
    return texture;
}

void EnsureTextures(SDL_Renderer* renderer) {
    if (!renderer || renderer == sTextureRenderer) {
        return;
    }
    // The renderer lifecycle calls ReleaseRenderer before destroying old textures.
    sShellTexture = nullptr;
    sButtonATexture = nullptr;
    sButtonBTexture = nullptr;
    sLeftTexture = nullptr;
    sRightTexture = nullptr;
    sUpTexture = nullptr;
    sDownTexture = nullptr;
    sStartSelectTexture = nullptr;
    sMenuTexture = nullptr;
    sTextureRenderer = renderer;
    sShellTexture = LoadEmbeddedTexture(renderer, kEmbeddedGbaSpShellPng, kEmbeddedGbaSpShellPngSize);
    sButtonATexture = LoadEmbeddedTexture(renderer, kEmbeddedGbaSpAButtonPng, kEmbeddedGbaSpAButtonPngSize);
    sButtonBTexture = LoadEmbeddedTexture(renderer, kEmbeddedGbaSpBButtonPng, kEmbeddedGbaSpBButtonPngSize);
    sLeftTexture = LoadEmbeddedTexture(renderer, kEmbeddedGbaSpLeftPng, kEmbeddedGbaSpLeftPngSize);
    sRightTexture = LoadEmbeddedTexture(renderer, kEmbeddedGbaSpRightPng, kEmbeddedGbaSpRightPngSize);
    sUpTexture = LoadEmbeddedTexture(renderer, kEmbeddedGbaSpUpPng, kEmbeddedGbaSpUpPngSize);
    sDownTexture = LoadEmbeddedTexture(renderer, kEmbeddedGbaSpDownPng, kEmbeddedGbaSpDownPngSize);
    sStartSelectTexture = LoadEmbeddedTexture(renderer, kEmbeddedGbaSpStartSelectPng,
                                               kEmbeddedGbaSpStartSelectPngSize);
    sMenuTexture = LoadEmbeddedTexture(renderer, kEmbeddedGbaSpMenuPng, kEmbeddedGbaSpMenuPngSize);
}

void DrawTexture(SDL_Renderer* renderer, SDL_Texture* texture, const SDL_FRect& destination,
                 bool applyTouchOpacity = true) {
    if (!texture) {
        return;
    }
    const Uint8 alpha = applyTouchOpacity
                            ? static_cast<Uint8>(255.0f * Port_Config_TouchOpacity())
                            : 255;
    SDL_SetTextureAlphaMod(texture, alpha);
    SDL_RenderTexture(renderer, texture, nullptr, &destination);
    SDL_SetTextureAlphaMod(texture, 255);
}

void DrawPressedTexture(SDL_Renderer* renderer, SDL_Texture* texture, bool pressed,
                        float centerX, float centerY, float imageWidth, float imageHeight,
                        int width, int height) {
    if (pressed) {
        DrawTexture(renderer, texture,
                    CanvasImageRect(centerX, centerY, imageWidth, imageHeight, width, height));
    }
}

void StampHeldEdges() {
    for (int i = 0; i < PORT_INPUT_COUNT; ++i) {
        if (sHeld[i]) {
            Port_Config_StampInputEdge(static_cast<PortInput>(i));
        }
    }
}

void ClearTextures() {
    if (sShellTexture) SDL_DestroyTexture(sShellTexture);
    if (sButtonATexture) SDL_DestroyTexture(sButtonATexture);
    if (sButtonBTexture) SDL_DestroyTexture(sButtonBTexture);
    if (sLeftTexture) SDL_DestroyTexture(sLeftTexture);
    if (sRightTexture) SDL_DestroyTexture(sRightTexture);
    if (sUpTexture) SDL_DestroyTexture(sUpTexture);
    if (sDownTexture) SDL_DestroyTexture(sDownTexture);
    if (sStartSelectTexture) SDL_DestroyTexture(sStartSelectTexture);
    if (sMenuTexture) SDL_DestroyTexture(sMenuTexture);
    sShellTexture = nullptr;
    sButtonATexture = nullptr;
    sButtonBTexture = nullptr;
    sLeftTexture = nullptr;
    sRightTexture = nullptr;
    sUpTexture = nullptr;
    sDownTexture = nullptr;
    sStartSelectTexture = nullptr;
    sMenuTexture = nullptr;
    sTextureRenderer = nullptr;
}

} // namespace

extern "C" bool Port_TouchControls_GameViewport(int width, int height, SDL_FRect* viewport) {
    if (!viewport || width <= 0 || height <= 0 || height <= width) {
        return false;
    }
    const SDL_FRect canvas = CanvasRect(width, height);
    const float scale = canvas.w / kCanvasWidth;
    *viewport = SDL_FRect{canvas.x + kScreenX * scale, canvas.y + kScreenY * scale,
                          kScreenWidth * scale, kScreenHeight * scale};
    return true;
}

extern "C" void Port_TouchControls_RenderShell(SDL_Renderer* renderer, int width, int height) {
    if (!renderer || width <= 0 || height <= width) {
        return;
    }
    EnsureTextures(renderer);
    DrawTexture(renderer, sShellTexture, CanvasRect(width, height), false);
}

extern "C" void Port_TouchControls_ReleaseRenderer(SDL_Renderer* renderer) {
    if (renderer && renderer == sTextureRenderer) {
        ClearTextures();
    }
}

extern "C" void Port_TouchControls_NotifyRenderSize(int width, int height) {
    if (width > 0 && height > 0) {
        sWindowWidth = width;
        sWindowHeight = height;
    }
}

extern "C" void Port_TouchControls_HandleEvent(const SDL_Event* event) {
    if (!event) {
        return;
    }
    switch (event->type) {
        case SDL_EVENT_FINGER_DOWN:
        case SDL_EVENT_FINGER_MOTION:
        case SDL_EVENT_FINGER_UP:
        case SDL_EVENT_FINGER_CANCELED:
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_MOTION:
        case SDL_EVENT_MOUSE_BUTTON_UP:
            break;
        default:
            return;
    }
    if (sMenuOpen || sWindowWidth <= 0 || sWindowHeight <= 0) {
        return;
    }

    if (event->type == SDL_EVENT_FINGER_DOWN) {
        const int64_t id = static_cast<int64_t>(event->tfinger.fingerID);
        const float x = event->tfinger.x * static_cast<float>(sWindowWidth);
        const float y = event->tfinger.y * static_cast<float>(sWindowHeight);
        TryTriggerSettings(x, y);
        AddOrUpdateTouch(id, x, y);
        StampHeldEdges();
    } else if (event->type == SDL_EVENT_FINGER_MOTION) {
        AddOrUpdateTouch(static_cast<int64_t>(event->tfinger.fingerID),
                         event->tfinger.x * static_cast<float>(sWindowWidth),
                         event->tfinger.y * static_cast<float>(sWindowHeight));
    } else if (event->type == SDL_EVENT_FINGER_UP || event->type == SDL_EVENT_FINGER_CANCELED) {
        RemoveTouch(static_cast<int64_t>(event->tfinger.fingerID));
    } else if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN && event->button.button == SDL_BUTTON_LEFT) {
        TryTriggerSettings(event->button.x, event->button.y);
        AddOrUpdateTouch(-1, event->button.x, event->button.y);
        StampHeldEdges();
    } else if (event->type == SDL_EVENT_MOUSE_MOTION && (event->motion.state & SDL_BUTTON_LMASK) != 0) {
        AddOrUpdateTouch(-1, event->motion.x, event->motion.y);
    } else if (event->type == SDL_EVENT_MOUSE_BUTTON_UP && event->button.button == SDL_BUTTON_LEFT) {
        RemoveTouch(-1);
    }
}

extern "C" void Port_TouchControls_SetMenuOpen(bool open) {
    sMenuOpen = open;
    if (open) {
        sTouches.clear();
        sHeld.fill(false);
    }
}

extern "C" void Port_TouchControls_NotifyGamepadUsed(void) {
    sVisible = false;
    sTouches.clear();
    sHeld.fill(false);
}

extern "C" void Port_TouchControls_SetGamepadAvailable(bool available) {
#ifdef __ANDROID__
    if (!available) {
        sVisible = true;
    }
#else
    if (available) {
        Port_TouchControls_NotifyGamepadUsed();
    } else {
        sVisible = true;
    }
#endif
}

extern "C" bool Port_TouchControls_InputPressed(PortInput input) {
    return input >= 0 && input < PORT_INPUT_COUNT && sHeld[input];
}

extern "C" void Port_TouchControls_Render(SDL_Renderer* renderer, int width, int height) {
    Port_TouchControls_NotifyRenderSize(width, height);
    if (!renderer || width <= 0 || height <= width) {
        return;
    }
    EnsureTextures(renderer);
    UpdateHeldState();
    if (!sVisible || sMenuOpen) {
        return;
    }

    DrawPressedTexture(renderer, sLeftTexture, sHeld[PORT_INPUT_LEFT], 162, 1715, 120, 128, width, height);
    DrawPressedTexture(renderer, sRightTexture, sHeld[PORT_INPUT_RIGHT], 378, 1715, 120, 128, width, height);
    DrawPressedTexture(renderer, sUpTexture, sHeld[PORT_INPUT_UP], 270, 1607, 128, 120, width, height);
    DrawPressedTexture(renderer, sDownTexture, sHeld[PORT_INPUT_DOWN], 270, 1823, 128, 120, width, height);
    DrawPressedTexture(renderer, sButtonATexture, sHeld[PORT_INPUT_A], 905, 1684, 132, 132, width, height);
    DrawPressedTexture(renderer, sButtonBTexture, sHeld[PORT_INPUT_B], 703, 1741, 132, 132, width, height);
    DrawPressedTexture(renderer, sStartSelectTexture, sHeld[PORT_INPUT_SELECT],
                       435, 2282, 70, 70, width, height);
    DrawPressedTexture(renderer, sStartSelectTexture, sHeld[PORT_INPUT_START],
                       641, 2282, 70, 70, width, height);
    DrawTexture(renderer, sMenuTexture,
                CanvasImageRect(kMenuX, kMenuY, 50, 50, width, height));
}

extern "C" bool Port_TouchControls_ConsumeSettingsRequest(void) {
    if (!sSettingsRequested) {
        return false;
    }
    sSettingsRequested = false;
    sTouches.clear();
    sHeld.fill(false);
    return true;
}

#else

extern "C" bool Port_TouchControls_GameViewport(int, int, SDL_FRect*) {
    return false;
}
extern "C" void Port_TouchControls_RenderShell(SDL_Renderer*, int, int) {
}
extern "C" void Port_TouchControls_ReleaseRenderer(SDL_Renderer*) {
}
extern "C" void Port_TouchControls_NotifyRenderSize(int, int) {
}
extern "C" void Port_TouchControls_HandleEvent(const SDL_Event*) {
}
extern "C" void Port_TouchControls_SetMenuOpen(bool) {
}
extern "C" void Port_TouchControls_NotifyGamepadUsed(void) {
}
extern "C" void Port_TouchControls_SetGamepadAvailable(bool) {
}
extern "C" bool Port_TouchControls_InputPressed(PortInput) {
    return false;
}
extern "C" void Port_TouchControls_Render(SDL_Renderer*, int, int) {
}
extern "C" bool Port_TouchControls_ConsumeSettingsRequest(void) {
    return false;
}

#endif
