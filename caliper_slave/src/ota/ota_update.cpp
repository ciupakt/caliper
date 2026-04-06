#include "ota_update.h"
#include "../config.h"
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <esp_now.h>
#include <shared_common.h>
#include <error_handler.h>
#include <MacroDebugger.h>

OTAUpdate::OTAUpdate()
    : active(false)
    , startTime(0)
    , lastProgressPercent(0)
{
}

void OTAUpdate::setupWiFiAP()
{
    WiFi.mode(WIFI_AP);
    delay(WIFI_INIT_DELAY_MS);

    bool apStarted = WiFi.softAP(OTA_SSID, OTA_PASSWORD, OTA_AP_CHANNEL);
    if (!apStarted)
    {
        RECORD_ERROR(ERR_OTA_WIFI_AP_FAILED, "Failed to start WiFi AP: %s", OTA_SSID);
        return;
    }

    DEBUG_I("OTA AP started: %s", OTA_SSID);
    DEBUG_I("OTA AP IP: %s", WiFi.softAPIP().toString().c_str());
}

void OTAUpdate::setupArduinoOTA()
{
    ArduinoOTA.setHostname("caliper-slave");
    ArduinoOTA.setPassword(OTA_PASSWORD);

    ArduinoOTA.onStart([this]() {
        String type = (ArduinoOTA.getCommand() == U_FLASH) ? "sketch" : "filesystem";
        DEBUG_I("OTA start - updating %s", type.c_str());
        digitalWrite(LED_GREEN, HIGH);
        digitalWrite(LED_RED, LOW);
    });

    ArduinoOTA.onEnd([this]() {
        DEBUG_I("OTA update complete!");
        for (int i = 0; i < 3; i++)
        {
            digitalWrite(LED_GREEN, HIGH);
            delay(100);
            digitalWrite(LED_GREEN, LOW);
            delay(100);
        }
        ESP.restart();
    });

    ArduinoOTA.onProgress([this](unsigned int progress, unsigned int total) {
        unsigned int percent = (progress / (total / 100));
        if (percent >= lastProgressPercent + 10)
        {
            DEBUG_I("OTA progress: %u%%", percent);
            lastProgressPercent = percent;
            digitalWrite(LED_GREEN, !digitalRead(LED_GREEN));
        }
    });

    ArduinoOTA.onError([this](ota_error_t error) {
        const char* errMsg;
        switch (error)
        {
        case OTA_AUTH_ERROR:
            errMsg = "Auth Failed";
            break;
        case OTA_BEGIN_ERROR:
            errMsg = "Begin Failed";
            break;
        case OTA_CONNECT_ERROR:
            errMsg = "Connect Failed";
            break;
        case OTA_RECEIVE_ERROR:
            errMsg = "Receive Failed";
            break;
        case OTA_END_ERROR:
            errMsg = "End Failed";
            break;
        default:
            errMsg = "Unknown";
            break;
        }
        RECORD_ERROR(ERR_OTA_UPDATE_FAILED, "OTA error[%u]: %s", error, errMsg);
        digitalWrite(LED_GREEN, LOW);
        digitalWrite(LED_RED, HIGH);
        delay(3000);
        ESP.restart();
    });

    ArduinoOTA.begin();
    DEBUG_I("ArduinoOTA ready");
}

void OTAUpdate::startOTAMode()
{
    DEBUG_I("Entering OTA mode...");
    active = true;
    startTime = millis();
    lastProgressPercent = 0;

    esp_now_deinit();
    DEBUG_I("ESP-NOW deinitialized");

    setupWiFiAP();

    if (WiFi.getMode() != WIFI_AP)
    {
        RECORD_ERROR(ERR_OTA_WIFI_AP_FAILED, "WiFi AP not in expected mode after setup");
        active = false;
        return;
    }

    setupArduinoOTA();

    digitalWrite(LED_GREEN, HIGH);
    delay(250);
    digitalWrite(LED_GREEN, LOW);
    delay(250);
    digitalWrite(LED_GREEN, HIGH);

    DEBUG_I("OTA mode active - waiting for upload (timeout %d s)", OTA_TIMEOUT_MS / 1000);
}

void OTAUpdate::handle()
{
    if (!active)
    {
        return;
    }

    ArduinoOTA.handle();

    if (millis() - startTime > OTA_TIMEOUT_MS)
    {
        RECORD_ERROR(ERR_OTA_TIMEOUT, "OTA timeout - no upload received within %d s", OTA_TIMEOUT_MS / 1000);
        DEBUG_W("OTA timeout - rebooting...");
        delay(100);
        ESP.restart();
    }
}

bool OTAUpdate::isActive() const
{
    return active;
}
