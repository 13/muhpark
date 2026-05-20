#pragma once
#include <ArduinoJson.h>
#include <cstdint>

struct Config {
    float    activate_dist = 300.f;
    float    offset        = 0.f;
    uint8_t  brightness    = 8;
    uint32_t sleep_timeout = 60;
    char     description[64]  = {};
    char     wifi_ssid[64]    = {};
    char     wifi_pass[64]    = {};
    char     mqtt_server[64]  = {};
    uint16_t mqtt_port        = 1883;
    char     mqtt_user[32]    = {};
    char     mqtt_pass[64]    = {};
    char     mqtt_topic[64]   = {};
    char     mqtt_base[64]    = {};
};

namespace Cfg {
    void          load();
    void          save();
    const Config& get();
    bool          apply(JsonObject& obj);
}
