#include "display.h"
#include <MD_MAX72xx.h>
#include <Arduino.h>

static constexpr uint8_t PIN_DIN = 7;
static constexpr uint8_t PIN_CLK = 3;
static constexpr uint8_t PIN_CS  = 5;

static MD_MAX72XX matrix(MD_MAX72XX::GENERIC_HW, PIN_DIN, PIN_CLK, PIN_CS, 1);

static const uint8_t PAT_SMILEY[8] = { 0x3C,0x42,0x99,0xA5,0x81,0xA5,0x42,0x3C };
static const uint8_t PAT_1[8]      = { 0x00,0x3C,0x18,0x18,0x18,0x18,0x1C,0x18 };
static const uint8_t PAT_2[8]      = { 0x00,0x7E,0x0C,0x18,0x30,0x60,0x66,0x3C };
static const uint8_t PAT_3[8]      = { 0x00,0x3C,0x66,0x60,0x3C,0x60,0x66,0x3C };
static const uint8_t PAT_4[8]      = { 0x00,0x30,0x30,0x7F,0x33,0x36,0x3C,0x38 };
static const uint8_t PAT_5[8]      = { 0x00,0x3E,0x63,0x60,0x60,0x3F,0x03,0x7F };

static const uint8_t* PATTERNS[7] = {
    PAT_SMILEY, PAT_1, PAT_2, PAT_3, PAT_4, PAT_5, nullptr
};

static void on(const uint8_t* pat) {
    matrix.control(MD_MAX72XX::SHUTDOWN, MD_MAX72XX::OFF);
    matrix.clear();
    for (int r = 0; r < 8; r++) matrix.setRow(0, r, pat[r]);
}

static void off() {
    matrix.control(MD_MAX72XX::SHUTDOWN, MD_MAX72XX::ON);
}

void Display::init(uint8_t brightness) {
    matrix.begin();
    matrix.clear();
    setBrightness(brightness);
    matrix.control(MD_MAX72XX::SHUTDOWN, MD_MAX72XX::OFF);

    // Boot animation: wipe in top→bottom, wipe out bottom→top
    for (int r = 0; r < 8; r++) { matrix.setRow(0, r, 0xFF); delay(40); }
    delay(150);
    for (int r = 7; r >= 0; r--) { matrix.setRow(0, r, 0x00); delay(40); }

    matrix.clear();
    off();
    Serial.printf("[display] DIN=%d CLK=%d CS=%d  brightness=%d\n", PIN_DIN, PIN_CLK, PIN_CS, brightness);
}

void Display::setBrightness(uint8_t b) {
    matrix.control(MD_MAX72XX::INTENSITY, b);
}

void Display::update(const ParkState& s) {
    if (s.displaySleep) { off(); return; }
    int lvl = (s.sysState == SysState::ACTIVE) ? s.parkLevel : s.lastSeenLevel;
    if (lvl >= 0 && lvl < 7 && PATTERNS[lvl])
        on(PATTERNS[lvl]);
    else
        off();
}
