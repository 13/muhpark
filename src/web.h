#pragma once
#include "park.h"

namespace Web {
    void begin(ParkState& s);
    void loop();
    void notify(const ParkState& s);
}
