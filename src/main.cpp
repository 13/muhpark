/*
 * muhpark — Smart Parking Sensor v1.1
 * Hardware: ESP32-S2 Mini + VL53L1X + MAX7219 8×8 LED matrix
 *
 * Pin map (ESP32-S2 Mini):
 *   VL53L1X  SDA  → GPIO33
 *   VL53L1X  SCL  → GPIO35
 *   MAX7219  DIN  → GPIO7
 *   MAX7219  CLK  → GPIO3
 *   MAX7219  CS   → GPIO5
 */

#include <Arduino.h>
#include <LittleFS.h>
#include "park.h"
#include "cfg.h"
#include "sensor.h"
#include "display.h"
#include "net.h"
#include "web.h"
#include "mqtt.h"

static ParkState state;

void setup() {
#ifndef VERSION
#define VERSION "dev"
#endif
    Serial.begin(115200);
    while (!Serial && millis() < 5000) delay(10);
    Serial.printf("\n── muhpark %s ──\n", VERSION);

    if (!LittleFS.begin()) Serial.println("[fs] mount failed");
    else                   Serial.println("[fs] mounted");

    Cfg::load();
    Sensor::init();
    Display::init(Cfg::get().brightness);
    Net::begin();
    Web::begin(state);
    Mqtt::begin();

    Serial.println("[muhpark] ready");
}

void loop() {
    Net::loop();
    Web::loop();
    Mqtt::loop();

    if (Sensor::ready()) {
        Sensor::update(state, Cfg::get());
        Display::update(state);
        Web::notify(state);
        Mqtt::publish(state);
    }
}
