#pragma once
#include <cstdint>

enum class SysState { SLEEPING, IDLE, ACTIVE };

struct ParkState {
    float    distRaw      = 9999.f;
    float    distFiltered = 9999.f;
    float    lastSeenDist  = -1.f;
    int      lastSeenLevel = -1;
    int      parkLevel     = 6;
    SysState sysState          = SysState::SLEEPING;
    uint32_t lastActiveMs      = 0;
    uint32_t lastLevelChangeMs = 0;
    bool     displaySleep      = true;
};
