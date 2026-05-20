#include "web.h"
#include "cfg.h"
#include "display.h"
#include <ESPAsyncWebServer.h>
#include <AsyncJson.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <Update.h>
#include <Arduino.h>
#include "esp_system.h"

static constexpr uint32_t WS_BROADCAST_MS = 200;

static AsyncWebServer httpServer(80);
static AsyncWebSocket wsServer("/ws");
static ParkState*     statePtr        = nullptr;
static uint32_t       lastBroadcastMs = 0;

static const char* stateName(const ParkState& s) {
    switch (s.sysState) {
        case SysState::ACTIVE:   return "ACTIVE";
        case SysState::IDLE:     return "IDLE";
        default:                 return "SLEEPING";
    }
}

static const char* levelLabel(const ParkState& s) {
    if (s.parkLevel == 0) return "PARKED";
    if (s.parkLevel == 6) return "---";
    static char buf[2] = { '0', '\0' };
    buf[0] = '0' + s.parkLevel;
    return buf;
}

static const char* resetReasonStr() {
    switch (esp_reset_reason()) {
        case ESP_RST_POWERON:   return "Power On";
        case ESP_RST_EXT:       return "External";
        case ESP_RST_SW:        return "Software";
        case ESP_RST_PANIC:     return "Panic";
        case ESP_RST_INT_WDT:   return "Int Watchdog";
        case ESP_RST_TASK_WDT:  return "Task Watchdog";
        case ESP_RST_WDT:       return "Watchdog";
        case ESP_RST_DEEPSLEEP: return "Deep Sleep";
        case ESP_RST_BROWNOUT:  return "Brownout";
        default:                return "Unknown";
    }
}

static void broadcast(const ParkState& s) {
    if (wsServer.count() == 0) return;
    const Config& cfg = Cfg::get();

    JsonDocument doc;
    doc["state"]        = stateName(s);
    doc["level"]        = levelLabel(s);
    doc["level_num"]    = s.parkLevel;
    doc["distance"]     = (s.distFiltered >= 9000.f) ? -1 : (int)s.distFiltered;
    doc["raw"]          = (s.distRaw      >= 9000.f) ? -1 : (int)s.distRaw;
    doc["last_seen"]    = (s.lastSeenDist  < 0.f)    ? -1 : (int)s.lastSeenDist;
    doc["uptime"]       = millis() / 1000UL;
    doc["rssi"]         = (WiFi.getMode() == WIFI_STA) ? (int)WiFi.RSSI() : 0;
    doc["heap"]         = ESP.getFreeHeap();
    doc["sensor_ok"]    = (s.distRaw < 9000.f);
    doc["description"]  = cfg.description;
    doc["hostname"]     = WiFi.getHostname();
    doc["reset_reason"] = resetReasonStr();
    doc["ssid"]         = (WiFi.getMode() == WIFI_STA) ? WiFi.SSID() : String("muhpark-ap");
    doc["ip"]           = (WiFi.getMode() == WIFI_STA)
                            ? WiFi.localIP().toString()
                            : WiFi.softAPIP().toString();
#ifdef VERSION
    doc["version"]      = VERSION;
#endif

    JsonObject cfgObj = doc["cfg"].to<JsonObject>();
    cfgObj["brightness"]    = cfg.brightness;
    cfgObj["sleep_timeout"] = cfg.sleep_timeout;
    cfgObj["activate_dist"] = (int)cfg.activate_dist;
    cfgObj["offset"]        = cfg.offset;

    String payload;
    serializeJson(doc, payload);
    wsServer.textAll(payload);
}

void Web::begin(ParkState& s) {
    statePtr = &s;

    wsServer.onEvent([](AsyncWebSocket*, AsyncWebSocketClient* client,
                        AwsEventType type, void* arg, uint8_t* data, size_t len) {
        if (type == WS_EVT_CONNECT) {
            Serial.printf("[ws] #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
            if (statePtr) broadcast(*statePtr);
            return;
        }
        if (type == WS_EVT_DISCONNECT) {
            Serial.printf("[ws] #%u disconnected\n", client->id());
            return;
        }
        if (type == WS_EVT_DATA) {
            auto* info = reinterpret_cast<AwsFrameInfo*>(arg);
            if (info->opcode != WS_TEXT || !info->final || info->index != 0) return;

            String text(reinterpret_cast<char*>(data), len);
            JsonDocument doc;
            if (deserializeJson(doc, text) != DeserializationError::Ok) return;

            if (doc["type"] == "settings") {
                JsonObject obj = doc.as<JsonObject>();
                if (Cfg::apply(obj)) {
                    Display::setBrightness(Cfg::get().brightness);
                    Cfg::save();
                    if (statePtr) broadcast(*statePtr);
                }
            }
        }
    });
    httpServer.addHandler(&wsServer);

    // ── Utility endpoints ──────────────────────────────────────────────────────

    httpServer.on("/ping", HTTP_GET, [](AsyncWebServerRequest* req) {
        req->send(200, "text/plain", "ok");
    });

    httpServer.on("/reboot", HTTP_GET, [](AsyncWebServerRequest* req) {
        req->onDisconnect([]() { ESP.restart(); });
        req->send(200, "text/plain", "rebooting");
    });

    httpServer.on("/api/reboot", HTTP_POST, [](AsyncWebServerRequest* req) {
        req->onDisconnect([]() { ESP.restart(); });
        req->send(200, "application/json", R"({"ok":true})");
    });

    // ── Settings ───────────────────────────────────────────────────────────────

    httpServer.on("/api/settings", HTTP_GET, [](AsyncWebServerRequest* req) {
        const Config& cfg = Cfg::get();
        JsonDocument doc;
        doc["description"]  = cfg.description;
        doc["brightness"]   = cfg.brightness;
        doc["sleep_timeout"]= cfg.sleep_timeout;
        doc["activate_dist"]= (int)cfg.activate_dist;
        doc["offset"]       = cfg.offset;
        doc["wifi_ssid"]    = cfg.wifi_ssid;
        doc["wifi_pass"]    = cfg.wifi_pass;
        doc["mqtt_server"]  = cfg.mqtt_server;
        doc["mqtt_port"]    = cfg.mqtt_port;
        doc["mqtt_user"]    = cfg.mqtt_user;
        doc["mqtt_pass"]    = cfg.mqtt_pass;
        doc["mqtt_topic"]   = cfg.mqtt_topic;
        doc["mqtt_base"]    = cfg.mqtt_base;
#ifdef VERSION
        doc["version"]      = VERSION;
#endif
        String body; serializeJson(doc, body);
        req->send(200, "application/json", body);
    });

    auto* settingsPost = new AsyncCallbackJsonWebHandler("/api/settings",
        [](AsyncWebServerRequest* req, JsonVariant& body) {
            if (!body.is<JsonObject>()) {
                req->send(400, "application/json", R"({"ok":false,"err":"bad json"})");
                return;
            }
            JsonObject obj = body.as<JsonObject>();
            if (Cfg::apply(obj)) {
                Display::setBrightness(Cfg::get().brightness);
                Cfg::save();
                if (statePtr) broadcast(*statePtr);
            }
            req->send(200, "application/json", R"({"ok":true})");
        }
    );
    httpServer.addHandler(settingsPost);

    // ── Status ─────────────────────────────────────────────────────────────────

    httpServer.on("/api/status", HTTP_GET, [](AsyncWebServerRequest* req) {
        if (!statePtr) { req->send(503, "application/json", R"({"ok":false})"); return; }
        const ParkState& s = *statePtr;
        JsonDocument doc;
        doc["state"]    = stateName(s);
        doc["level"]    = levelLabel(s);
        doc["distance"] = (s.distFiltered >= 9000.f) ? -1 : (int)s.distFiltered;
        doc["uptime"]   = millis() / 1000UL;
        String body; serializeJson(doc, body);
        req->send(200, "application/json", body);
    });

    // ── OTA upload ─────────────────────────────────────────────────────────────

    httpServer.on("/update", HTTP_POST,
        [](AsyncWebServerRequest* req) {
            bool ok = !Update.hasError();
            auto* resp = req->beginResponse(200, "application/json",
                ok ? R"({"success":true})" : R"({"success":false,"message":"Update failed"})");
            resp->addHeader("Connection", "close");
            req->send(resp);
        },
        [](AsyncWebServerRequest*, String filename, size_t index,
           uint8_t* data, size_t len, bool final) {
            if (!index) {
                int cmd = (filename.indexOf("littlefs") >= 0) ? U_SPIFFS : U_FLASH;
                Serial.printf("[ota] start: %s (%s)\n", filename.c_str(),
                    cmd == U_SPIFFS ? "fs" : "fw");
                if (!Update.begin(UPDATE_SIZE_UNKNOWN, cmd))
                    Update.printError(Serial);
            }
            if (!Update.hasError() && Update.write(data, len) != len)
                Update.printError(Serial);
            if (final) {
                if (Update.end(true))
                    Serial.printf("[ota] done: %u bytes\n", index + len);
                else
                    Update.printError(Serial);
            }
        }
    );

    // ── Static files ───────────────────────────────────────────────────────────

    httpServer.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");
    httpServer.onNotFound([](AsyncWebServerRequest* req) {
        req->send(404, "text/plain", "not found");
    });

    httpServer.begin();
    Serial.println("[http] server started");
}

void Web::loop() {
    wsServer.cleanupClients();
    uint32_t now = millis();
    if (statePtr && now - lastBroadcastMs >= WS_BROADCAST_MS) {
        lastBroadcastMs = now;
        broadcast(*statePtr);
    }
}

void Web::notify(const ParkState& s) {
    broadcast(s);
}
