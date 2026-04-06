#pragma once

#include <Arduino.h>

class OTAUpdate
{
public:
    OTAUpdate();
    void startOTAMode();
    void handle();
    bool isActive() const;

private:
    bool active;
    unsigned long startTime;
    unsigned int lastProgressPercent;
    void setupWiFiAP();
    void setupArduinoOTA();
};
