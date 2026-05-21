#include "sensor.h"
#include <Arduino.h>
#include <Wire.h>
#include <VL53L1X.h>

static constexpr uint8_t PIN_SDA    = 33;
static constexpr uint8_t PIN_SCL    = 35;

static constexpr int   LEVEL_DEBOUNCE  = 5;
static int   pendingLevel = -1;
static int   pendingCount = 0;

static const char* stateStr(SysState s) {
    switch (s) {
        case SysState::ACTIVE:  return "ACTIVE";
        case SysState::IDLE:    return "IDLE";
        default:                return "SLEEPING";
    }
}

static void updateStateMachine(ParkState& s, const Config& cfg) {
    bool inRange = (s.parkLevel < 6);
    if (inRange) { s.lastSeenDist = s.distFiltered; s.lastSeenLevel = s.parkLevel; }

    switch (s.sysState) {
        case SysState::SLEEPING:
            if (inRange) { s.sysState = SysState::ACTIVE; s.lastActiveMs = millis(); s.lastLevelChangeMs = millis(); }
            break;
        case SysState::ACTIVE:
            if (inRange)  s.lastActiveMs = millis();
            else          s.sysState = SysState::IDLE;
            break;
        case SysState::IDLE:
            if (inRange)                                                        { s.sysState = SysState::ACTIVE; s.lastActiveMs = millis(); }
            else if (millis() - s.lastActiveMs >= cfg.sleep_timeout * 1000UL)  { s.sysState = SysState::SLEEPING; }
            break;
    }

    s.displaySleep = (s.sysState == SysState::SLEEPING) ||
                     (s.sysState == SysState::ACTIVE &&
                      millis() - s.lastLevelChangeMs >= cfg.sleep_timeout * 1000UL);
}

// ── Variant: dual ceiling-mounted sensors ─────────────────────────────────────
#ifdef VARIANT_DUAL_CEILING

static constexpr uint8_t PIN_XSHUT_B = 34;
static VL53L1X sensorA, sensorB;

void Sensor::init() {
    Wire.begin(PIN_SDA, PIN_SCL);
    pinMode(PIN_XSHUT_B, OUTPUT);

    // Disable B so only A is on the bus at its default address 0x29
    digitalWrite(PIN_XSHUT_B, LOW);
    delay(10);

    sensorA.setTimeout(500);
    if (!sensorA.init()) { Serial.println("[sensor] A init failed — check wiring"); return; }
    sensorA.setAddress(0x30);

    // Enable B; it powers up at 0x29 (A is now at 0x30)
    digitalWrite(PIN_XSHUT_B, HIGH);
    delay(10);

    sensorB.setTimeout(500);
    if (!sensorB.init()) { Serial.println("[sensor] B init failed — check wiring"); return; }

    sensorA.setDistanceMode(VL53L1X::Long); sensorA.setMeasurementTimingBudget(50000); sensorA.startContinuous(100);
    sensorB.setDistanceMode(VL53L1X::Long); sensorB.setMeasurementTimingBudget(50000); sensorB.startContinuous(100);
    Serial.printf("[sensor] dual-ceiling  SDA=%d SCL=%d  XSHUT_B=%d  A=0x30 B=0x29\n", PIN_SDA, PIN_SCL, PIN_XSHUT_B);
}

bool Sensor::ready() {
    return sensorA.dataReady() && sensorB.dataReady();
}

void Sensor::update(ParkState& s, const Config& cfg) {
    int      prevLevel = s.parkLevel;
    SysState prevState = s.sysState;

    auto readOne = [](VL53L1X& sen) -> float {
        uint16_t mm = sen.read(false);
        if (sen.timeoutOccurred()) return 9999.f;
        float cm = mm * 0.1f;
        return (cm >= 2.f && cm <= 400.f) ? cm : 9999.f;
    };

    s.distRaw      = readOne(sensorA);
    s.distFiltered = readOne(sensorB);  // reused field: sensor B raw reading

    float a = (s.distRaw      < 9000.f) ? constrain(s.distRaw      + cfg.offset, 0.1f, 9998.f) : 9999.f;
    float b = (s.distFiltered < 9000.f) ? constrain(s.distFiltered + cfg.offset, 0.1f, 9998.f) : 9999.f;
    int cand = (a < cfg.activate_dist && b < cfg.activate_dist) ? 0 : 6;

    if (cand == s.parkLevel) {
        pendingLevel = -1;
        pendingCount = 0;
    } else if (cand == pendingLevel) {
        if (++pendingCount >= LEVEL_DEBOUNCE) {
            s.parkLevel         = cand;
            s.lastLevelChangeMs = millis();
            pendingLevel = -1;
            pendingCount = 0;
        }
    } else {
        pendingLevel = cand;
        pendingCount = 1;
    }

    updateStateMachine(s, cfg);

    if (s.parkLevel != prevLevel)
        Serial.printf("[sensor] %s  A=%.0fcm B=%.0fcm thr=%.0fcm\n",
            s.parkLevel == 0 ? "CAR IN PLACE" : "empty",
            s.distRaw, s.distFiltered, cfg.activate_dist);
    if (s.sysState != prevState)
        Serial.printf("[state]  %s→%s\n", stateStr(prevState), stateStr(s.sysState));
}

// ── Variant: single side-mounted sensor (default) ────────────────────────────
#else

static constexpr int   FILTER_SIZE = 7;
static constexpr float INNER_CLOSER[5]  = { 25.f,  50.f, 100.f, 150.f, 200.f };
static constexpr float INNER_FARTHER[5] = { 28.f,  55.f, 110.f, 165.f, 220.f };

static VL53L1X sensor;

static float filterBuf[FILTER_SIZE] = {};
static int   filterIdx    = 0;
static bool  filterFull   = false;

void Sensor::init() {
    Wire.begin(PIN_SDA, PIN_SCL);
    sensor.setTimeout(500);
    if (!sensor.init()) {
        Serial.println("[sensor] VL53L1X init failed — check wiring");
        return;
    }
    sensor.setDistanceMode(VL53L1X::Long);
    sensor.setMeasurementTimingBudget(50000);
    sensor.startContinuous(100);
    Serial.printf("[sensor] VL53L1X SDA=%d SCL=%d\n", PIN_SDA, PIN_SCL);
}

bool Sensor::ready() {
    return sensor.dataReady();
}

static float readRaw() {
    uint16_t mm = sensor.read(false);
    if (sensor.timeoutOccurred()) return 9999.f;
    float cm = mm * 0.1f;
    return (cm >= 2.f && cm <= 400.f) ? cm : 9999.f;
}

static float medianFilter(float v) {
    filterBuf[filterIdx] = v;
    filterIdx = (filterIdx + 1) % FILTER_SIZE;
    if (filterIdx == 0) filterFull = true;
    int n = filterFull ? FILTER_SIZE : filterIdx;
    float sorted[FILTER_SIZE];
    memcpy(sorted, filterBuf, n * sizeof(float));
    for (int i = 1; i < n; i++) {
        float k = sorted[i]; int j = i - 1;
        while (j >= 0 && sorted[j] > k) { sorted[j + 1] = sorted[j]; j--; }
        sorted[j + 1] = k;
    }
    return sorted[n / 2];
}

static int nextLevel(float dist, int cur, const Config& cfg) {
    if (dist >= 9000.f) return 6;
    if (cur == 6 && dist <= cfg.activate_dist)         return 5;
    if (cur == 5 && dist >  cfg.activate_dist * 1.10f) return 6;
    if (cur > 0 && cur <= 5 && dist <= INNER_CLOSER[cur - 1]) return cur - 1;
    if (cur >= 0 && cur <  5 && dist >  INNER_FARTHER[cur])   return cur + 1;
    return cur;
}

void Sensor::update(ParkState& s, const Config& cfg) {
    int      prevLevel = s.parkLevel;
    SysState prevState = s.sysState;

    s.distRaw = readRaw();
    float adjusted = (s.distRaw < 9000.f)
        ? constrain(s.distRaw + cfg.offset, 0.1f, 9998.f)
        : 9999.f;
    s.distFiltered = medianFilter(adjusted);
    int cand       = nextLevel(s.distFiltered, s.parkLevel, cfg);

    if (cand == s.parkLevel) {
        pendingLevel = -1;
        pendingCount = 0;
    } else if (cand == pendingLevel) {
        if (++pendingCount >= LEVEL_DEBOUNCE) {
            s.parkLevel         = cand;
            s.lastLevelChangeMs = millis();
            pendingLevel = -1;
            pendingCount = 0;
        }
    } else {
        pendingLevel = cand;
        pendingCount = 1;
    }

    updateStateMachine(s, cfg);

#ifdef SENSOR_DEBUG
    Serial.printf("[sensor:raw] %.0fmm → %.1fcm\n", s.distRaw * 10.f, s.distRaw);
#endif

    if (s.parkLevel != prevLevel)
        Serial.printf("[sensor] level %d→%d  dist=%.0fcm raw=%.0fcm\n",
            prevLevel, s.parkLevel, s.distFiltered, s.distRaw);
    if (s.sysState != prevState)
        Serial.printf("[state]  %s→%s\n", stateStr(prevState), stateStr(s.sysState));
}

#endif // VARIANT_DUAL_CEILING
