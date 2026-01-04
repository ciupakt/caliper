#pragma once

#include <Arduino.h>

// Parsowanie liczb z pełną walidacją (brak śmieci po liczbie).
// Zwraca true tylko jeśli cały string (po trim spacji/tabów) jest poprawną liczbą.
bool parseIntStrict(const String &s, long &out);
bool parseFloatStrict(const String &s, float &out);

struct SystemStatus;
class PreferencesManager;
class MeasurementState;

// Minimalny kontekst wymagany przez parser komend po Serial.
// Dzięki temu `main.cpp` pozostaje czytelny, a moduł nie musi znać WebServera.
struct SerialCliContext
{
  SystemStatus *systemStatus = nullptr;
  PreferencesManager *prefsManager = nullptr;
  MeasurementState *measurementState = nullptr;

  // Akcje (implementowane w main.cpp)
  void (*requestMeasurement)() = nullptr;
  void (*requestUpdate)() = nullptr;
  void (*sendMotorTest)() = nullptr;
};

// Inicjalizacja kontekstu. Należy wywołać w setup() przed startem timera.
void SerialCli_begin(const SerialCliContext &ctx);

// Tick / parser liniowy bez blokowania. Zgodny z sygnaturą arduino-timer.
bool SerialCli_tick(void *arg);
