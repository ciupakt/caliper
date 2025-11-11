# TB67H453FNG Motor Control Module

## Struktura modułu

Moduł sterowania silnikiem DC został przekształcony z pliku `.ino` na standardową strukturę C/C++:

### Pliki:
- **`caliper_slave_motor_ctrl.h`** - Plik nagłówkowy z definicjami, strukturami i deklaracjami funkcji
- **`caliper_slave_motor_ctrl.cpp`** - Plik implementacji C++ z definicjami wszystkich funkcji
- **`caliper_slave_motor_ctrl.c`** - Placeholder (zastąpiony przez .cpp)
- **`caliper_slave.ino`** - Główny plik zaktualizowany do używania nowego modułu

### Główne funkcje publiczne:

#### Inicjalizacja:
- `initializeMotorController()` - Inicjalizacja sterownika i konfiguracja pinów
- `setMotorConfig(MotorConfig config)` - Ustawienie pełnej konfiguracji

#### Sterowanie silnikiem:
- `setMotorState(MotorState state)` - Ustawienie stanu silnika
- `motorForward()` - Rotacja do przodu
- `motorReverse()` - Rotacja do tyłu
- `motorStop()` - Zatrzymanie (tryb coast)
- `motorSleep()` - Tryb uśpienia
- `motorWake()` - Wybudzenie z trybu uśpienia

#### Monitorowanie:
- `readMotorCurrent()` - Odczyt prądu silnika
- `checkMotorFault()` - Sprawdzenie błędów
- `getMotorStatus()` - Status silnika jako string
- `getMotorState()` - Aktualny stan silnika

#### Konfiguracja:
- `setCurrentLimit(float currentAmps)` - Ograniczenie prądu
- `configureCurrentMode(CurrentMode mode)` - Tryb kontroli prądu

#### Dodatkowe:
- `demoMotorControl()` - Demo funkcji
- `emergencyStop()` - Awaryjne zatrzymanie
- `resetMotorController()` - Reset sterownika

### Struktury danych:

```c
typedef struct {
  float maxCurrent;           // Maksymalny prąd (A)
  float vrefVoltage;          // Napięcie VREF 
  CurrentMode currentMode;    // Tryb kontroli prądu
} MotorConfig;

typedef enum {
  MOTOR_SLEEP = -1,      // Tryb uśpienia
  MOTOR_STOP = 0,        // Zatrzymanie/Coast
  MOTOR_FORWARD = 1,     // Rotacja do przodu
  MOTOR_REVERSE = 2      // Rotacja do tyłu
} MotorState;

typedef enum {
  CURRENT_DISABLED = 0,      // Brak kontroli prądu
  CURRENT_CONSTANT = 1,      // Stały prąd PWM
  CURRENT_FIXED_OFF = 2      // Stały czas wyłączenia
} CurrentMode;
```

### Użycie w głównym programie:

```cpp
#include "caliper_slave_motor_ctrl.h"

// W setup():
initializeMotorController();

// W loop():
if (setMotorState(MOTOR_FORWARD)) {
  // Silnik pracuje do przodu
}

float current = readMotorCurrent();
bool fault = checkMotorFault();

if (fault) {
  emergencyStop();
}
```

### Zabezpieczenia:

- Ograniczenie prądu do 2.45A (70% z maksymalnego)
- Automatyczna detekcja i obsługa błędów
- Sprawdzenie inicjalizacji przed operacjami
- Zabezpieczenie przed rekurencyjną inicjalizacją
- Auto-recovery w przypadku przekroczenia prądu

### Specyfikacje techniczne:

- **Sterownik**: TB67H453FNG (Single H-Bridge)
- **Maksymalny prąd**: 3.5A (ograniczony do 2.45A)
- **Napięcie zasilania**: 4.5V - 44V
- **Tryb sterowania**: Phase/Enable (PMODE = Low)
- **Rozdzielczość prądu**: ~1.5mA (z 1.5kΩ RISENSE)
- **Czas reakcji**: < 2ms (tWAKE)

Moduł jest gotowy do użycia w projekcie kalibratora z ESP32.