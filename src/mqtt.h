#pragma once
#include "park.h"

namespace Mqtt {
    void begin();
    void loop();
    void publish(const ParkState& s);
}
