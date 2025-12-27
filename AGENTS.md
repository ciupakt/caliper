# Instrukcje kompilacji projektu Caliper dla modeli AI

## Przegląd projektu

Projekt Caliper składa się z dwóch podprojektów PlatformIO oraz aplikacji GUI:
- `caliper_master` - główny sterownik z interfejsem webowym
- `caliper_slave` - sterownik suwmiarki, akcelerometru i silnika
- `caliper_master_gui` - aplikacja GUI w Pythonie do sterowania i wizualizacji
- `lib/CaliperShared` - współdzielona biblioteka dla Master i Slave

## Wymagania

- PlatformIO zainstalowane w systemie
- Ścieżka do skryptów PlatformIO: `C:\Users\tiim\.platformio\penv\Scripts`

## Kompilacja projektu

### Kompilacja caliper_slave

```powershell
cd caliper_slave && C:\Users\tiim\.platformio\penv\Scripts\platformio.exe run --environment esp32doit-devkit-v1
```

### Kompilacja caliper_master

```powershell
cd caliper_master && C:\Users\tiim\.platformio\penv\Scripts\platformio.exe run --environment esp32doit-devkit-v1
```

## Wgrywanie programu na urządzenie

### Wgranie caliper_slave

```powershell
cd caliper_slave && C:\Users\tiim\.platformio\penv\Scripts\platformio.exe run --target upload -s --environment esp32doit-devkit-v1 --upload-port COM8
```

### Wgranie caliper_master

```powershell
cd caliper_master && C:\Users\tiim\.platformio\penv\Scripts\platformio.exe run --target upload -s --environment esp32doit-devkit-v1 --upload-port COM7
```

## Czyszczenie plików kompilacji

```powershell
cd caliper_slave && C:\Users\tiim\.platformio\penv\Scripts\platformio.exe run --target clean --environment esp32doit-devkit-v1
cd caliper_master && C:\Users\tiim\.platformio\penv\Scripts\platformio.exe run --target clean --environment esp32doit-devkit-v1
```

## Struktura Projektu

### caliper_master
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

### caliper_slave
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

### caliper_master_gui
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

### lib/CaliperShared
```
lib/CaliperShared/
├── shared_common.h    # Wspólne definicje typów i struktur
└── shared_config.h    # Wspólna konfiguracja (piny, stałe)
```

## Informacje o sprzęcie

### caliper_slave
- Platforma: Espressif 32 (ESP32 DOIT DEVKIT V1)
- Mikrokontroler: ESP32 240MHz, 320KB RAM, 4MB Flash
- Biblioteki: ADXL345_WE, WiFi, Wire

### caliper_master
- Platforma: Espressif 32 (ESP32 DOIT DEVKIT V1)
- Mikrokontroler: ESP32 240MHz, 320KB RAM, 4MB Flash

## Uruchamianie aplikacji Python GUI

### Instalacja zależności

```powershell
cd caliper_master_gui
pip install -r requirements.txt
```

### Uruchomienie

```powershell
cd caliper_master_gui
python caliper_master_gui_new.py
```

### Testy jednostkowe

```powershell
cd caliper_master_gui
python -m pytest tests/
```

## Uwagi

- Kompilacja może generować ostrzeżenia, które nie przerywają procesu
- Pliki firmware są generowane w katalogu `.pio\build\[board]\firmware.bin`
- Aby włączyć tryb szczegółowy, dodaj opcję `-v` lub `--verbose`
- Nowa wersja GUI (`caliper_master_gui_new.py`) używa modułowej struktury
- Oryginalna wersja GUI (`caliper_master_gui.py`) jest zachowana dla kompatybilności
