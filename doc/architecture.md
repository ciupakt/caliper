# Architektura Projektu Caliper

## Przegląd

Projekt Caliper składa się z dwóch urządzeń ESP32 (Master i Slave) oraz aplikacji GUI w Pythonie do sterowania i wizualizacji pomiarów.

## Komponenty Systemu

### 1. Caliper Master (ESP32)

**Lokalizacja:** `caliper_master/`

**Odpowiedzialności:**
- Komunikacja z Slave przez ESP-NOW
- Serwer HTTP dla interfejsu webowego
- Obsługa komend z portu szeregowego
- Przesyłanie danych do aplikacji GUI

**Struktura plików:**
```
caliper_master/
├── src/
│   ├── main.cpp              # Główna logika aplikacji
│   ├── config.h              # Konfiguracja specyficzna dla Master
│   ├── communication/
│   │   ├── communication.h
│   │   └── communication.cpp
│   └── web/
│       ├── web_server.h
│       └── web_server.cpp
├── data/                   # Pliki LittleFS (HTML/CSS/JS)
│   ├── index.html
│   ├── style.css
│   └── app.js
└── platformio.ini
```

### 2. Caliper Slave (ESP32)

**Lokalizacja:** `caliper_slave/`

**Odpowiedzialności:**
- Obsługa suwmiarki cyfrowej (protokół zegar/dane)
- Odczyt z akcelerometru ADXL345
- Sterowanie silnikiem DC (MP6550GG-Z)
- Monitorowanie napięcia baterii
- Komunikacja z Master przez ESP-NOW

**Struktura plików:**
```
caliper_slave/
├── src/
│   ├── main.cpp              # Główna logika aplikacji
│   ├── config.h              # Konfiguracja specyficzna dla Slave
│   ├── sensors/              # Moduły sensorów
│   │   ├── caliper.h/cpp    # Obsługa suwmiarki
│   │   └── accelerometer.h/cpp # Obsługa ADXL345
│   ├── motor/                # Moduł sterownika silnika
│   │   ├── motor_ctrl.h
│   │   └── motor_ctrl.cpp
│   └── power/                # Moduł monitorowania baterii
│       ├── battery.h
│       └── battery.cpp
└── platformio.ini
```

### 3. Caliper Master GUI (Python)

**Lokalizacja:** `caliper_master_gui/`

**Odpowiedzialności:**
- Komunikacja z Master przez port szeregowy
- Wizualizacja pomiarów w czasie rzeczywistym
- Sterowanie silnikiem
- Eksport danych do CSV
- Logowanie zdarzeń

**Struktura plików:**
```
caliper_master_gui/
├── caliper_master_gui.py    # Oryginalny plik (do zastąpienia)
├── caliper_master_gui_new.py # Nowa modularna wersja
├── requirements.txt
├── src/
│   ├── __init__.py
│   ├── app.py               # Klasa CaliperApp
│   ├── serial_handler.py     # Obsługa portu szeregowego
│   ├── gui/
│   │   ├── __init__.py
│   │   ├── measurement_tab.py # Zakładka pomiarów
│   │   └── log_tab.py      # Zakładka logów
│   └── utils/
│       ├── __init__.py
│       └── csv_handler.py   # Obsługa CSV
└── tests/
    └── test_serial.py        # Testy jednostkowe
```

### 4. Współdzielone Biblioteki

**Lokalizacja:** `lib/CaliperShared/`

**Zawartość:**
- `shared_common.h` - Wspólne definicje typów i struktur
- `shared_config.h` - Wspólna konfiguracja (piny, stałe)

## Protokół Komunikacji

### ESP-NOW (Master ↔ Slave)

**Format wiadomości:**
```cpp
struct Message {
    char command;           // Komenda (CMD_MEASURE, CMD_FORWARD, itp.)
    float measurement;       // Wartość pomiaru (mm)
    bool valid;            // Flaga poprawności pomiaru
    uint32_t timestamp;    // Timestamp
    uint16_t batteryVoltage; // Napięcie baterii (mV)
    float angleX;           // Kąt X z akcelerometru
};
```

**Komendy:**
- `CMD_MEASURE ('m')` - Żądanie pomiaru
- `CMD_FORWARD ('f')` - Silnik do przodu
- `CMD_REVERSE ('r')` - Silnik do tyłu
- `CMD_STOP ('s')` - Zatrzymaj silnik
- `CMD_UPDATE ('u')` - Aktualizacja statusu

### Serial (Master ↔ GUI)

**Format danych (UART, `DEBUG_PLOT`):**
- `>measurement:<wartość>` - Surowy pomiar (mm)
- `>calibrationOffset:<wartość>` - Offset kalibracji (mm)
- `>angleX:<wartość>` - Kąt X
- `>batteryVoltage:<wartość>` - Napięcie baterii
- `>measurementReady:<nazwa> <wartość>` - Pomiar sesji (wartość skorygowana)

**Uwaga:** korekcja jest liczona po stronie klienta: `corrected = measurement + calibrationOffset`.

**Komendy GUI → Master:**
- `m` - Wyzwól pomiar
- `f` - Silnik do przodu
- `r` - Silnik do tyłu
- `s` - Zatrzymaj silnik

## Przepływ Danych

```
┌─────────────┐
│   Slave    │
│  (ESP32)   │
└──────┬──────┘
       │ ESP-NOW
       │
┌──────▼──────┐
│   Master   │
│  (ESP32)   │
└──────┬──────┘
       │ Serial
       │
┌──────▼──────┐
│    GUI     │
│  (Python)   │
└─────────────┘
```

## Konfiguracja Sprzętowa

### Piny GPIO (Slave)

| Funkcja | Pin ESP32 | Stała w config.h |
|----------|-------------|------------------|
| Caliper Clock | GPIO 4 | CALIPER_CLOCK_PIN |
| Caliper Data | GPIO 5 | CALIPER_DATA_PIN |
| Caliper Trigger | GPIO 18 | CALIPER_TRIG_PIN |
| Motor IN1 | GPIO 13 | MOTOR_IN1_PIN |
| Motor IN2 | GPIO 12 | MOTOR_IN2_PIN |
| Battery ADC | GPIO 34 | BATTERY_VOLTAGE_PIN |
| I2C SDA | GPIO 21 | - |
| I2C SCL | GPIO 22 | - |

### Parametry Sieci

| Parametr | Wartość |
|----------|-----------|
| WiFi Kanał | 1 |
| WiFi SSID | ESP32_Pomiar |
| WiFi IP | 192.168.4.1 |
| WiFi Port | 80 |

## Zależności

### Caliper Master
- PlatformIO (ESP32)
- Arduino Framework
- WiFi
- ESP-NOW
- LittleFS

### Caliper Slave
- PlatformIO (ESP32)
- Arduino Framework
- WiFi
- ESP-NOW
- Wire (I2C)
- ADXL345_WE

### Caliper Master GUI
- Python 3.8+
- DearPyGUI >= 1.9.0
- pyserial >= 3.5

## Kompilacja i Wgrywanie

Szczegółowe instrukcje znajdują się w pliku [`AGENTS.md`](../AGENTS.md:1).

## Testowanie

### Testy Jednostkowe (Python)
```bash
cd caliper_master_gui
python -m pytest tests/
```

### Testy Integracyjne
1. Wgraj firmware na oba urządzenia ESP32
2. Uruchom aplikację GUI
3. Połącz się z Master przez port szeregowy
4. Przetestuj:
   - Pomiary suwmiarki
   - Sterowanie silnikiem
   - Monitorowanie baterii
   - Eksport CSV
   - Interfejs webowy (http://192.168.4.1)

---

*Dokument utworzony: 2025-12-27*
*Wersja: 1.0*
