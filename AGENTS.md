# Instrukcje kompilacji projektu Caliper dla modeli AI

## Przegląd projektu

Projekt Caliper składa się z dwóch podprojektów PlatformIO oraz aplikacji GUI:

- `caliper_master` – główny sterownik (ESP32) z AP WiFi + HTTP + ESP-NOW + LittleFS (UI web w `data/`)
- `caliper_slave` – sterownik (ESP32) suwmiarki + akcelerometru + silnika + baterii + ESP-NOW
- `caliper_master_gui` – aplikacja GUI w Pythonie (Dear PyGui) do sterowania i wizualizacji
- `lib/CaliperShared` – współdzielona biblioteka dla Master i Slave (wspólne typy/stałe + makra debug)

## Wymagania

- PlatformIO zainstalowane w systemie
- Ścieżka do skryptów PlatformIO: `C:\Users\tiim\.platformio\penv\Scripts`
- Python 3.x (dla `caliper_master_gui`)

## Kompilacja projektu (PlatformIO)

Używany environment w obu projektach: `esp32doit-devkit-v1`.

Uwaga: w poniższych poleceniach używany jest bezpośrednio `platformio.exe` z venv PlatformIO. Jeśli masz `pio` w `PATH`, możesz zamienić `C:\Users\tiim\.platformio\penv\Scripts\platformio.exe` na `pio`.

### Kompilacja `caliper_slave`

```powershell
cd caliper_slave && C:\Users\tiim\.platformio\penv\Scripts\platformio.exe run --environment esp32doit-devkit-v1
```

### Kompilacja `caliper_master`

```powershell
cd caliper_master && C:\Users\tiim\.platformio\penv\Scripts\platformio.exe run --environment esp32doit-devkit-v1
```

## Wgrywanie programu na urządzenie

Uwaga: porty są domyślnie ustawione w `platformio.ini` przez `monitor_port`:
- Master: `COM7`
- Slave: `COM8`

Jeśli port upload jest inny, dodaj `--upload-port COMx`.

### Wgranie `caliper_slave`

```powershell
cd caliper_slave && C:\Users\tiim\.platformio\penv\Scripts\platformio.exe run --target upload -s --environment esp32doit-devkit-v1 --upload-port COM8
```

### Wgranie `caliper_master`

```powershell
cd caliper_master && C:\Users\tiim\.platformio\penv\Scripts\platformio.exe run --target upload -s --environment esp32doit-devkit-v1 --upload-port COM7
```

### Wgranie systemu plików LittleFS (UI web) – tylko `caliper_master`

Pliki UI web znajdują się w `caliper_master/data/` i są flashowane do LittleFS.

```powershell
cd caliper_master && C:\Users\tiim\.platformio\penv\Scripts\platformio.exe run --target uploadfs -s --environment esp32doit-devkit-v1 --upload-port COM7
```

## Czyszczenie plików kompilacji

```powershell
cd caliper_slave && C:\Users\tiim\.platformio\penv\Scripts\platformio.exe run --target clean --environment esp32doit-devkit-v1
cd caliper_master && C:\Users\tiim\.platformio\penv\Scripts\platformio.exe run --target clean --environment esp32doit-devkit-v1
```

## Struktura projektu

### `caliper_master`

```
caliper_master/
├── src/
│   ├── main.cpp                    # Główna logika: AP WiFi + HTTP + ESP-NOW + obsługa LittleFS
│   ├── config.h                    # Konfiguracja specyficzna dla Master (SSID, hasło, MAC, stałe)
│   ├── communication.h/.cpp        # Menedżer komunikacji ESP-NOW (wysyłanie komend + retry)
│   ├── serial_cli.h/.cpp           # Proste CLI po Serial (komendy serwisowe/diagnostyczne)
│   ├── measurement_state.h/.cpp    # Zarządzanie stanem pomiarowym z buforami tekstowymi
│   └── preferences_manager.h/.cpp  # Przechowywanie ustawień w NVS (Preferences)
├── data/                           # Pliki LittleFS (HTML/CSS/JS)
│   ├── index.html
│   ├── style.css
│   └── app.js
└── platformio.ini
```

### `caliper_slave`

```
caliper_slave/
├── src/
│   ├── main.cpp                    # Główna logika: ESP-NOW + harmonogram (timery) + spinanie modułów
│   ├── config.h                    # Konfiguracja specyficzna dla Slave (MAC, piny, stałe)
│   ├── sensors/
│   │   ├── caliper.h/.cpp          # Obsługa suwmiarki + dekodowanie danych (interrupt-based)
│   │   └── accelerometer.h/.cpp    # Obsługa IIS328DQ (I2C) + wyliczanie kątów
│   ├── motor/
│   │   └── motor_ctrl.h/.cpp       # Sterowanie silnikiem STSPIN250 (PWM, PH, REF, EN, FAULT)
│   └── power/
│       └── battery.h/.cpp          # Pomiar napięcia baterii (ADC)
└── platformio.ini
```

### `caliper_master_gui`

Uwaga: plik wejściowy aplikacji to `caliper_master_gui.py` (modularna wersja), która importuje kod z katalogu `src/`.

```
caliper_master_gui/
├── caliper_master_gui.py      # Entry-point GUI (Dear PyGui) – klasa CaliperGUI
├── requirements.txt           # Zależności Python (dearpygui, pyserial)
├── INSTALL_UBUNTU.md          # Instrukcja instalacji na Ubuntu
├── src/
│   ├── __init__.py
│   ├── serial_handler.py      # Obsługa portu szeregowego (wrapper pyserial)
│   ├── gui/
│   │   ├── __init__.py
│   │   ├── calibration_tab.py  # Zakładka kalibracji (motor, offset, logi)
│   │   └── measurement_tab.py # Zakładka pomiarów (historia, wykres, sesje)
│   └── utils/
│       ├── __init__.py
│       └── csv_handler.py     # Obsługa CSV (zapis pomiarów)
└── tests/
    └── test_serial.py         # Testy jednostkowe SerialHandler
```

### `lib/CaliperShared`

```
lib/CaliperShared/
├── shared_common.h        # Wspólne definicje typów/struktur/protokołu (CommandType, MotorState, MessageMaster, MessageSlave, SystemStatus)
├── shared_config.h        # Wspólna konfiguracja (piny, stałe, limity walidacji)
├── MacroDebugger.h        # Makra debug/log/plot (DEBUG_I, DEBUG_E, DEBUG_W, DEBUG_PLOT)
├── error_codes.h/.cpp     # System kodów błędów (8 kategorii, 10 modułów, ~50+ kodów)
├── error_handler.h        # Makra logowania błędów i klasa ErrorHandler (RECORD_ERROR, CHECK_ERROR, HANDLE_ERROR)
├── espnow_helper.h/.cpp   # Funkcje pomocnicze ESP-NOW z retry (espnow_send_with_retry)
└── ERROR_HANDLING.md      # Dokumentacja systemu obsługi błędów
```

## Informacje o sprzęcie

### `caliper_slave`

- Platforma: Espressif 32 (ESP32 DOIT DEVKIT V1)
- Mikrokontroler: ESP32 240MHz, 320KB RAM, 4MB Flash
- Biblioteki: `arduino-timer`

### `caliper_master`

- Platforma: Espressif 32 (ESP32 DOIT DEVKIT V1)
- Mikrokontroler: ESP32 240MHz, 320KB RAM, 4MB Flash
- LittleFS: UI web jest serwowane z pamięci flash (folder `data/`)

## Uruchamianie aplikacji Python GUI

### Instalacja zależności

```powershell
cd caliper_master_gui
python -m pip install -r requirements.txt
```

### Uruchomienie

```powershell
cd caliper_master_gui
python caliper_master_gui.py
```

### Testy jednostkowe

Uwaga: `pytest` nie jest obecnie na liście zależności w [`requirements.txt`](caliper_master_gui/requirements.txt:1), więc przed uruchomieniem testów doinstaluj go ręcznie.

```powershell
cd caliper_master_gui
python -m pip install pytest
python -m pytest tests/
```

## Uwagi

- Kompilacja może generować ostrzeżenia, które nie przerywają procesu.
- Pliki firmware są generowane w katalogu `.pio\build\[board]\firmware.bin` (np. `.pio\build\esp32doit-devkit-v1\firmware.bin`).
- Aby włączyć tryb szczegółowy PlatformIO, dodaj opcję `-v` / `--verbose`.
- `caliper_master` wymaga jednorazowego wgrania LittleFS (`uploadfs`) po zmianach w `caliper_master/data/`.
