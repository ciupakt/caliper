# Instrukcje kompilacji projektu Caliper dla modeli AI

## Przegląd projektu

Projekt Caliper składa się z dwóch podprojektów PlatformIO:
- `caliper_master` - główny sterownik
- `caliper_slave` - sterownik silnika

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

## Informacje o sprzęcie

### caliper_slave
- Platforma: Espressif 32 (ESP32 DOIT DEVKIT V1)
- Mikrokontroler: ESP32 240MHz, 320KB RAM, 4MB Flash
- Biblioteki: ADXL345_WE, WiFi, Wire

### caliper_master
- Platforma: Espressif 32 (ESP32 DOIT DEVKIT V1)
- Mikrokontroler: ESP32 240MHz, 320KB RAM, 4MB Flash

## Uwagi

- Kompilacja może generować ostrzeżenia, które nie przerywają procesu
- Pliki firmware są generowane w katalogu `.pio\build\[board]\firmware.bin`
- Aby włączyć tryb szczegółowy, dodaj opcję `-v` lub `--verbose`
