#pragma once

#include <Arduino.h>

// Number parsing with full validation (no trailing garbage).
// Returns true only if the entire string (after trimming spaces/tabs) is a valid number.
bool parseIntStrict(const String &s, long &out);
bool parseFloatStrict(const String &s, float &out);

struct SystemStatus;
class PreferencesManager;
class MeasurementState;

// Minimal context required by the Serial command parser.
// This keeps `main.cpp` readable, and the module does not need to know about WebServer.
struct SerialCliContext
{
  SystemStatus *systemStatus = nullptr;
  PreferencesManager *prefsManager = nullptr;
  MeasurementState *measurementState = nullptr;

  // Actions (implemented in main.cpp)
  void (*requestMeasurement)() = nullptr;
  void (*requestUpdate)() = nullptr;
  void (*sendMotorTest)() = nullptr;
  void (*sendOTA)() = nullptr;
  void (*enterPairingMode)() = nullptr;
};

// Initialize context. Call in setup() before starting the timer.
void SerialCli_begin(const SerialCliContext &ctx);

// Tick / non-blocking line parser. Compatible with arduino-timer signature.
bool SerialCli_tick(void *arg);
