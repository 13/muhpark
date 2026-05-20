#include "net.h"
#include "cfg.h"
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <Arduino.h>

void Net::begin() {
    const Config& cfg = Cfg::get();

    uint8_t mac[6];
    WiFi.macAddress(mac);
    char hostname[16];
    snprintf(hostname, sizeof(hostname), "esp32-%02X%02X", mac[4], mac[5]);
    WiFi.setHostname(hostname);

    Serial.printf("[wifi] connecting to %s", cfg.wifi_ssid);
    WiFi.mode(WIFI_STA);
    WiFi.begin(cfg.wifi_ssid, cfg.wifi_pass);
    for (int i = 0; i < 40 && WiFi.status() != WL_CONNECTED; i++) {
        delay(500); Serial.print('.');
    }
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("\n[wifi] http://%s\n", WiFi.localIP().toString().c_str());
    } else {
        Serial.println("\n[wifi] fallback AP: muhpark-ap (192.168.4.1)");
        WiFi.mode(WIFI_AP);
        WiFi.softAP("muhpark-ap");
    }

    ArduinoOTA.setHostname(hostname);
#ifdef OTA_PASS
    ArduinoOTA.setPassword(OTA_PASS);
#endif
    ArduinoOTA.onStart([]()              { Serial.println("[ota] start"); });
    ArduinoOTA.onEnd([]()               { Serial.println("\n[ota] done"); });
    ArduinoOTA.onProgress([](unsigned int p, unsigned int t) {
        Serial.printf("[ota] %u%%\r", p * 100 / t);
    });
    ArduinoOTA.onError([](ota_error_t e) { Serial.printf("[ota] error %u\n", e); });
    ArduinoOTA.begin();
    Serial.println("[ota] ready");
}

void Net::loop() {
    ArduinoOTA.handle();
}
