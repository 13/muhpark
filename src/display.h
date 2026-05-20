#pragma once
#include "park.h"
#include <cstdint>

namespace Display {
    void init(uint8_t brightness);
    void update(const ParkState& s);
    void setBrightness(uint8_t b);
}
