#include "mqtt.h"
#include "cfg.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <Arduino.h>

static constexpr uint32_t RECONNECT_MS = 5000;

static WiFiClient   wifiClient;
static PubSubClient mqttClient(wifiClient);
static uint32_t     lastReconnectMs    = 0;
static int          lastPublishedLevel = -99;
static char         deviceId[16];    // esp32-XXXX
static char         defaultTopic[32]; // muh/parking/park-XXXX

static void publishStatus() {
    const Config& cfg = Cfg::get();
    if (!*cfg.mqtt_base) return;

    char topic[96];
    String ip = (WiFi.getMode() == WIFI_STA)
                    ? WiFi.localIP().toString()
                    : WiFi.softAPIP().toString();

    snprintf(topic, sizeof(topic), "%s/%s/LWT",      cfg.mqtt_base, deviceId);
    mqttClient.publish(topic, "online", true);

    snprintf(topic, sizeof(topic), "%s/%s/IP",      cfg.mqtt_base, deviceId);
    mqttClient.publish(topic, ip.c_str(), true);

#ifdef VERSION
    snprintf(topic, sizeof(topic), "%s/%s/VERSION", cfg.mqtt_base, deviceId);
    mqttClient.publish(topic, VERSION, true);
#endif
}

static bool reconnect() {
    if (WiFi.status() != WL_CONNECTED) return false;
    const Config& cfg = Cfg::get();
    if (!*cfg.mqtt_server) return false;

    char lwtTopic[96] = {};
    if (*cfg.mqtt_base)
        snprintf(lwtTopic, sizeof(lwtTopic), "%s/%s/LWT", cfg.mqtt_base, deviceId);

    Serial.printf("[mqtt] connecting to %s:%d ...", cfg.mqtt_server, cfg.mqtt_port);

    bool ok = *lwtTopic
        ? mqttClient.connect(deviceId, cfg.mqtt_user, cfg.mqtt_pass, lwtTopic, 0, true, "offline")
        : mqttClient.connect(deviceId, cfg.mqtt_user, cfg.mqtt_pass);

    if (ok) {
        Serial.println(" connected");
        publishStatus();
    } else {
        Serial.printf(" failed rc=%d\n", mqttClient.state());
    }
    return ok;
}

void Mqtt::begin() {
    uint8_t mac[6];
    WiFi.macAddress(mac);
    snprintf(deviceId,     sizeof(deviceId),     "esp32-%02X%02X",        mac[4], mac[5]);
    snprintf(defaultTopic, sizeof(defaultTopic), "muh/parking/park-%02X%02X", mac[4], mac[5]);

    // Default is 15 s; that long a stall in connect() freezes sensor/display and
    // can trip the watchdog when the broker is unreachable. Keep it short.
    mqttClient.setSocketTimeout(2);

    const Config& cfg = Cfg::get();
    if (!*cfg.mqtt_server) return;
    mqttClient.setServer(cfg.mqtt_server, cfg.mqtt_port);
    reconnect();
}

void Mqtt::loop() {
    if (mqttClient.connected()) { mqttClient.loop(); return; }
    uint32_t now = millis();
    if (now - lastReconnectMs >= RECONNECT_MS) {
        lastReconnectMs = now;
        reconnect();
    }
}

void Mqtt::publish(const ParkState& s) {
    if (s.parkLevel == lastPublishedLevel) return;
    if (!mqttClient.connected()) return;
    const char* topic = *Cfg::get().mqtt_topic ? Cfg::get().mqtt_topic : defaultTopic;
    char buf[4];
    itoa(s.parkLevel, buf, 10);
    if (mqttClient.publish(topic, buf, true)) {
        lastPublishedLevel = s.parkLevel;
        Serial.printf("[mqtt] %s → %s\n", topic, buf);
    }
}
