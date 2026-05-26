#pragma once

#include <stdint.h>

typedef struct PPUMemory {
    uint16_t frame_width;
    uint8_t mode;
    uint8_t reserved;
} PPUMemory;
