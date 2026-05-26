#pragma once

#include "../ppu_memory.h"

#ifdef __cplusplus
extern "C" {
#endif

void virtuappu_mode2_render_frame(const PPUMemory *ppu);

#ifdef __cplusplus
}
#endif
