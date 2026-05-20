#pragma once
#include "park.h"
#include "cfg.h"

namespace Sensor {
    void init();
    bool ready();
    void update(ParkState& s, const Config& cfg);
}
