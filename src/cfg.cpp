#include "cfg.h"
#include <LittleFS.h>
#include <Arduino.h>
#include <cstring>

static Config cfg;

static void applyDefaults() {
#ifdef WIFI_SSID
    strlcpy(cfg.wifi_ssid, WIFI_SSID, sizeof(cfg.wifi_ssid));
#endif
#ifdef WIFI_PASS
    strlcpy(cfg.wifi_pass, WIFI_PASS, sizeof(cfg.wifi_pass));
#endif
#ifdef MQTT_HOST
    strlcpy(cfg.mqtt_server, MQTT_HOST, sizeof(cfg.mqtt_server));
#endif
#ifdef MQTT_PORT
    cfg.mqtt_port = MQTT_PORT;
#endif
#ifdef MQTT_USER
    strlcpy(cfg.mqtt_user, MQTT_USER, sizeof(cfg.mqtt_user));
#endif
#ifdef MQTT_PASS
    strlcpy(cfg.mqtt_pass, MQTT_PASS, sizeof(cfg.mqtt_pass));
#endif
#ifdef MQTT_TOPIC
    strlcpy(cfg.mqtt_topic, MQTT_TOPIC, sizeof(cfg.mqtt_topic));
#endif
#ifdef MQTT_BASE
    strlcpy(cfg.mqtt_base, MQTT_BASE, sizeof(cfg.mqtt_base));
#endif
}

static void copyStr(char* dst, size_t sz, JsonDocument& doc, const char* key) {
    const char* s = doc[key].as<const char*>();
    if (s) strlcpy(dst, s, sz);
}

void Cfg::load() {
    applyDefaults();

    File f = LittleFS.open("/config.json", "r");
    if (!f) { Serial.println("[cfg] using defaults"); return; }
    JsonDocument doc;
    if (deserializeJson(doc, f) == DeserializationError::Ok) {
        cfg.activate_dist = constrain(doc["activate_dist"]  | cfg.activate_dist,  100.f, 500.f);
        cfg.offset        = constrain(doc["offset"]         | cfg.offset,         -100.f, 100.f);
        cfg.brightness    = constrain((int)(doc["brightness"]    | (int)cfg.brightness), 0, 15);
        cfg.sleep_timeout = constrain((uint32_t)(int)(doc["sleep_timeout"] | (int)cfg.sleep_timeout), 10UL, 300UL);
        copyStr(cfg.description, sizeof(cfg.description), doc, "description");
        copyStr(cfg.wifi_ssid,   sizeof(cfg.wifi_ssid),   doc, "wifi_ssid");
        copyStr(cfg.wifi_pass,   sizeof(cfg.wifi_pass),   doc, "wifi_pass");
        copyStr(cfg.mqtt_server, sizeof(cfg.mqtt_server), doc, "mqtt_server");
        copyStr(cfg.mqtt_user,   sizeof(cfg.mqtt_user),   doc, "mqtt_user");
        copyStr(cfg.mqtt_pass,   sizeof(cfg.mqtt_pass),   doc, "mqtt_pass");
        copyStr(cfg.mqtt_topic,  sizeof(cfg.mqtt_topic),  doc, "mqtt_topic");
        copyStr(cfg.mqtt_base,   sizeof(cfg.mqtt_base),   doc, "mqtt_base");
        if (!doc["mqtt_port"].isNull()) cfg.mqtt_port = constrain((int)doc["mqtt_port"], 1, 65535);
    }
    f.close();
    Serial.printf("[cfg] dist=%.0f off=%.1f bri=%d sleep=%lus wifi=%s mqtt=%s:%d\n",
        cfg.activate_dist, cfg.offset, cfg.brightness, cfg.sleep_timeout,
        cfg.wifi_ssid, cfg.mqtt_server, cfg.mqtt_port);
}

void Cfg::save() {
    File f = LittleFS.open("/config.json", "w");
    if (!f) { Serial.println("[cfg] save failed"); return; }
    JsonDocument doc;
    doc["activate_dist"]  = cfg.activate_dist;
    doc["offset"]         = cfg.offset;
    doc["brightness"]     = cfg.brightness;
    doc["sleep_timeout"]  = cfg.sleep_timeout;
    doc["description"]    = cfg.description;
    doc["wifi_ssid"]      = cfg.wifi_ssid;
    doc["wifi_pass"]      = cfg.wifi_pass;
    doc["mqtt_server"]    = cfg.mqtt_server;
    doc["mqtt_port"]      = cfg.mqtt_port;
    doc["mqtt_user"]      = cfg.mqtt_user;
    doc["mqtt_pass"]      = cfg.mqtt_pass;
    doc["mqtt_topic"]     = cfg.mqtt_topic;
    doc["mqtt_base"]      = cfg.mqtt_base;
    serializeJson(doc, f);
    f.close();
    Serial.println("[cfg] saved");
}

const Config& Cfg::get() { return cfg; }

bool Cfg::apply(JsonObject& obj) {
    bool changed = false;

    auto applyStr = [&](const char* k, char* dst, size_t sz, bool skipEmpty = false) {
        const char* s = obj[k].as<const char*>();
        if (s && (!skipEmpty || *s)) { strlcpy(dst, s, sz); changed = true; }
    };

    if (!obj["brightness"].isNull())    { cfg.brightness    = constrain((int)obj["brightness"],                     0,     15);    changed = true; }
    if (!obj["sleep_timeout"].isNull()) { cfg.sleep_timeout = constrain((uint32_t)(int)obj["sleep_timeout"],        10UL,  300UL); changed = true; }
    if (!obj["activate_dist"].isNull()) { cfg.activate_dist = constrain((float)obj["activate_dist"],               100.f, 500.f); changed = true; }
    if (!obj["offset"].isNull())        { cfg.offset        = constrain((float)obj["offset"],                     -100.f, 100.f); changed = true; }
    if (!obj["mqtt_port"].isNull())     { cfg.mqtt_port     = constrain((int)obj["mqtt_port"],                       1,  65535);  changed = true; }
    applyStr("description", cfg.description, sizeof(cfg.description));
    applyStr("wifi_ssid",   cfg.wifi_ssid,   sizeof(cfg.wifi_ssid));
    applyStr("wifi_pass",   cfg.wifi_pass,   sizeof(cfg.wifi_pass),   true);
    applyStr("mqtt_server", cfg.mqtt_server, sizeof(cfg.mqtt_server));
    applyStr("mqtt_user",   cfg.mqtt_user,   sizeof(cfg.mqtt_user));
    applyStr("mqtt_pass",   cfg.mqtt_pass,   sizeof(cfg.mqtt_pass),   true);
    applyStr("mqtt_topic",  cfg.mqtt_topic,  sizeof(cfg.mqtt_topic));
    applyStr("mqtt_base",   cfg.mqtt_base,   sizeof(cfg.mqtt_base));

    if (changed)
        Serial.printf("[cfg] applied: bri=%d sleep=%lus dist=%.0f off=%.1f wifi=%s mqtt=%s:%d\n",
            cfg.brightness, cfg.sleep_timeout, cfg.activate_dist, cfg.offset,
            cfg.wifi_ssid, cfg.mqtt_server, cfg.mqtt_port);
    return changed;
}
