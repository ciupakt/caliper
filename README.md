# Caliper — bezprzewodowy system pomiarowy (ESP32 + suwmiarka)

Projekt **Caliper** to system bezprzewodowego pomiaru długości oparty o 2× ESP32, cyfrową suwmiarkę (odczyt strumienia bitów), akcelerometr ADXL345 oraz sterownik silnika MP6550GG-Z. Dane są przesyłane dwukierunkowo przez **ESP-NOW**, a sterowanie odbywa się przez:

- **Web UI** hostowane przez ESP32 Master (WiFi AP + HTTP + LittleFS)
- **Desktop GUI** w Pythonie (Dear PyGui) komunikujące się z Master po **Serial**

## Najważniejsze funkcje

- pomiar z cyfrowej suwmiarki + walidacja zakresów
- odczyt kąta z ADXL345 (I2C)
- pomiar napięcia baterii (ADC)
- sterowanie silnikiem DC przez MP6550GG-Z (PWM na IN1/IN2)
- UI web (LittleFS) + proste API HTTP
- GUI desktop (serial, log, wykresy, zapis do CSV)
- wspólny protokół/typy w bibliotece `lib/CaliperShared`

## Architektura (high-level)

### Komponenty

```mermaid
flowchart TD
    %% Master pracuje równolegle: WiFi AP/HTTP + ESP-NOW, najczęściej WIFI_AP_STA.

    subgraph SYS[System Caliper]
        subgraph M[ESP32 Master<br/>caliper_master]
            M_HTTP[HTTP server :80]
            M_FS[LittleFS<br/>UI web: index.html/style.css/app.js]
            M_ESPNOW[ESP-NOW manager<br/>TX/RX MessageMaster/MessageSlave]
            M_SERIAL[Serial CLI<br/>komendy ASCII -> msgMaster]
            M_STATE[Stan systemu<br/>ostatni pomiar, offset, status slave]
        end

        subgraph S[ESP32 Slave<br/>caliper_slave]
            S_ESPNOW[ESP-NOW<br/>RX komend / TX danych]
            S_CAL[Suwmiarka<br/>odczyt i dekoder]
            S_ACC[ADXL345<br/>I2C]
            S_BATT[Bateria<br/>ADC]
            S_MOTOR[Silnik<br/>MP6550GG-Z]
        end

        WEB[Przeglądarka<br/>Web UI]
        GUI[Python GUI<br/>Dear PyGui]
    end

    WEB <--> |HTTP| M_HTTP
    M_HTTP --> M_FS
    GUI <--> |USB Serial| M_SERIAL

    M_ESPNOW <--> |ESP-NOW| S_ESPNOW
    S_CAL --> S_ESPNOW
    S_ACC --> S_ESPNOW
    S_BATT --> S_ESPNOW
    S_MOTOR <--> S_ESPNOW

    M_SERIAL --> M_STATE
    M_HTTP --> M_STATE
    M_ESPNOW --> M_STATE
```

### Przepływ pomiaru (typowy)

```mermaid
sequenceDiagram
    participant U as Użytkownik
    participant GUI as GUI przez Serial
    participant WEB as Web UI przez HTTP
    participant M as ESP32 Master
    participant S as ESP32 Slave
    participant CAL as Suwmiarka
    participant ACC as ADXL345
    participant BAT as Bateria

    alt Sterowanie z GUI
        U->>GUI: „Pomiar”
        GUI->>M: Serial: m + LF
    else Sterowanie z Web UI
        U->>WEB: klik „Pomiar”
        WEB->>M: HTTP /measure albo /measure_session
    end

    M->>S: ESP-NOW: MessageMaster{CMD_MEASURE,...}
    S->>CAL: odczyt danych CLK/DATA + dekodowanie
    S->>ACC: odczyt kąta przez I2C
    S->>BAT: ADC read
    S-->>M: ESP-NOW: MessageSlave{measurement, angleX, batteryVoltage}
    M-->>GUI: Serial log/plot, wartości i offset
    M-->>WEB: JSON, raw i corrected
```

### Połączenia hardware (skrót)

```mermaid
flowchart LR
    %% Układ: urządzenia po lewej, ESP32 po środku, obciążenia po prawej.

    subgraph CAL[Cyfrowa suwmiarka]
        CAL_CLK[CLK]
        CAL_DATA[DATA]
        CAL_TRIG[TRIG]
    end

    subgraph ACC[ADXL345]
        ACC_I2C[I2C]
    end

    subgraph BATT[Bateria]
        BATT_V[Vbat]
    end

    subgraph SL[ESP32 Slave]
        GPIO18[GPIO18<br/>CALIPER_CLOCK]
        GPIO19[GPIO19<br/>CALIPER_DATA]
        GPIO5[GPIO5<br/>CALIPER_TRIG]

        GPIO21[GPIO21<br/>I2C SDA]
        GPIO22[GPIO22<br/>I2C SCL]

        GPIO34[GPIO34<br/>BATTERY_ADC]

        GPIO13[GPIO13<br/>MOTOR_IN1]
        GPIO12[GPIO12<br/>MOTOR_IN2]
    end

    subgraph DRV[MP6550GG-Z]
        DRV_IN1[IN1]
        DRV_IN2[IN2]
        DRV_OUT1[OUT1]
        DRV_OUT2[OUT2]
    end

    subgraph MOT[Silnik DC]
        MOT_A[Motor +]
        MOT_B[Motor -]
    end

    CAL_CLK -->|CLK| GPIO18
    CAL_DATA -->|DATA| GPIO19
    GPIO5 -->|TRIG| CAL_TRIG

    GPIO21 <--> |SDA| ACC_I2C
    GPIO22 <--> |SCL| ACC_I2C

    BATT_V -->|ADC| GPIO34

    GPIO13 -->|IN1| DRV_IN1
    GPIO12 -->|IN2| DRV_IN2
    DRV_OUT1 --> MOT_A
    DRV_OUT2 --> MOT_B
```

## Repozytorium — najważniejsze katalogi

- `caliper_master/` — firmware Master (PlatformIO)
  - źródła: `caliper_master/src/` (entry-point: `main.cpp`)
  - Web UI (LittleFS): `caliper_master/data/`
- `caliper_slave/` — firmware Slave (PlatformIO)
  - źródła: `caliper_slave/src/` (entry-point: `main.cpp`)
- `caliper_master_gui/` — GUI w Pythonie (Dear PyGui)
  - entry-point: `caliper_master_gui.py`
- `lib/CaliperShared/` — wspólne definicje protokołu i konfiguracji

## Protokół (w skrócie)

Wiadomości Master↔Slave są zdefiniowane we wspólnej bibliotece:

- `lib/CaliperShared/shared_common.h` — `CommandType`, `MessageMaster`, `MessageSlave`, `SystemStatus`
- `lib/CaliperShared/shared_config.h` — wspólne stałe (np. kanał WiFi dla ESP-NOW, piny)

## Build / Flash (Windows + PlatformIO)

Najbardziej aktualne komendy utrzymujemy w **AGENTS.md**.

### Kompilacja

```powershell
cd caliper_slave && C:\Users\tiim\.platformio\penv\Scripts\platformio.exe run --environment esp32doit-devkit-v1
cd caliper_master && C:\Users\tiim\.platformio\penv\Scripts\platformio.exe run --environment esp32doit-devkit-v1
```

### Wgrywanie firmware

```powershell
cd caliper_slave && C:\Users\tiim\.platformio\penv\Scripts\platformio.exe run --target upload -s --environment esp32doit-devkit-v1 --upload-port COM8
cd caliper_master && C:\Users\tiim\.platformio\penv\Scripts\platformio.exe run --target upload -s --environment esp32doit-devkit-v1 --upload-port COM7
```

### Wgrywanie UI web (LittleFS) — tylko Master

Po zmianach w `caliper_master/data/`:

```powershell
cd caliper_master && C:\Users\tiim\.platformio\penv\Scripts\platformio.exe run --target uploadfs -s --environment esp32doit-devkit-v1 --upload-port COM7
```

## Uruchomienie GUI (Python)

```powershell
cd caliper_master_gui
python -m pip install -r requirements.txt
python caliper_master_gui.py
```

## Web UI

1. Połącz się z WiFi utworzonym przez Master (SSID/hasło w `caliper_master/src/config.h`).
2. Otwórz w przeglądarce: `http://192.168.4.1`

## Kluczowe pliki (punkt startowy)

- Master firmware: `caliper_master/src/main.cpp`
- Slave firmware: `caliper_slave/src/main.cpp`
- ESP-NOW manager: `caliper_master/src/communication.h` + `caliper_master/src/communication.cpp`
- Serial CLI: `caliper_master/src/serial_cli.h` + `caliper_master/src/serial_cli.cpp`
- Wspólne typy/protokół: `lib/CaliperShared/shared_common.h`
- Wspólna konfiguracja/piny: `lib/CaliperShared/shared_config.h`
- Web UI: `caliper_master/data/index.html`, `caliper_master/data/style.css`, `caliper_master/data/app.js`
- Python GUI: `caliper_master_gui/caliper_master_gui.py`

## Licencja

Projekt hobbystyczny/edukacyjny.
