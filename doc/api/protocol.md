# Dokumentacja Protokołu Komunikacji Caliper

## Przegląd

Dokumentacja opisuje protokoły komunikacji używane w projekcie Caliper:
1. ESP-NOW - komunikacja między Master a Slave
2. Serial - komunikacja między Master a GUI
3. HTTP - interfejs webowy Master

---

## 1. Protokół ESP-NOW (Master ↔ Slave)

### Struktura Wiadomości

Aktualna struktura jest zdefiniowana wspólnie dla Master i Slave w [`shared_common.h`](lib/CaliperShared/shared_common.h:1):

```cpp
// Slave -> Master
struct MessageSlave
{
  uint32_t timestamp;
  float measurement;       // surowy pomiar w mm
  float batteryVoltage;    // V
  CommandType command;
  uint8_t angleX;
};

// Master -> Slave
struct MessageMaster
{
  uint32_t timestamp;
  uint32_t timeout;
  CommandType command;
  MotorState motorState;
  uint8_t motorSpeed;
  uint8_t motorTorque;
};
```

**Zasada:** komunikacja ESP-NOW przenosi pełną strukturę `MessageSlave` (Slave→Master) oraz `MessageMaster` (Master→Slave). Pola nieużywane w danym scenariuszu mogą być ignorowane.

### Komendy

Zgodnie z [`enum CommandType`](lib/CaliperShared/shared_common.h:22):

| Komenda | Kod | Kierunek | Opis |
|---------|-----|----------|------|
| CMD_MEASURE | 'M' | Master → Slave | Żądanie pomiaru suwmiarki |
| CMD_UPDATE  | 'U' | Master → Slave | Żądanie odświeżenia/statusu |
| CMD_MOTORTEST | 'T' | Master → Slave | Generyczne sterowanie silnikiem (używa pól `motorState/motorSpeed/motorTorque`) |

### Przepływ Komunikacji

#### 1. Żądanie Pomiaru

```
Master                                   Slave
  │                                        │
  ├── Message{command=CMD_MEASURE} ───────>│
  │                                        │
  │                                        ├─ Wyzwól TRIG_PIN
  │                                        ├─ Odczytaj dane suwmiarki
  │                                        ├─ Odczytaj akcelerometr
  │                                        ├─ Odczytaj baterię
  │                                        │
  │<── Message{measurement, batteryVoltage, angleX, timestamp} ──┤
  │   - measurement: 12.345 mm                                   │
  │   - batteryVoltage: 4.200 V                                  │
  │   - angleX: 123 (uint8)                                      │
```

#### 2. Sterowanie Silnikiem

Sterowanie silnikiem realizowane jest wyłącznie komendą generyczną `CMD_MOTORTEST`.

```
Master                                                                     Slave
  │                                                                          │
  ├── Message{command=CMD_MOTORTEST, motorState=1, motorSpeed=255} ─────────>│
  │                                                                          ├─ Silnik do przodu (PWM = motorSpeed)
  │                                                                          │
  ├── Message{command=CMD_MOTORTEST, motorState=2, motorSpeed=255} ─────────>│
  │                                                                          ├─ Silnik do tyłu (PWM = motorSpeed)
  │                                                                          │
  ├── Message{command=CMD_MOTORTEST, motorState=0, motorSpeed=0} ───────────>│
  │                                                                          ├─ Stop
  │                                                                          │
```

#### 3. Aktualizacja Statusu (okresowa)

```
Slave                                         Master
  │                                             │
  │<── Message{command=CMD_UPDATE, ...} ─────────┤
  │   - measurement: 0.0 (opcjonalnie)           │
  │   - batteryVoltage: 4.180 V                  │
  │   - angleX: 121 (uint8)                      │
```

**Uwaga:** okresowe wysyłanie `CMD_UPDATE` zależy od implementacji w Slave (w aktualnym kodzie głównie odpowiada on na żądania Master).

### Parametry Sieci

| Parametr | Wartość |
|----------|-----------|
| WiFi Kanał | 1 |
| Tryb WiFi | Station |
| Szyfrowanie | Nie (encrypt = false) |
| Timeout | 100ms |

---

## 2. Protokół Serial (Master ↔ GUI)

### Format Danych

Wszystkie dane przesyłane są jako tekst ASCII zakończony znakiem nowej linii (`\n`).

### Komendy GUI → Master

| Komenda | Format | Opis |
|----------|---------|-------|
| Trigger | `m\n` | Wyzwól pomiar |
| Forward | `f\n` | Silnik do przodu |
| Reverse | `r\n` | Silnik do tyłu |
| Stop | `s\n` | Zatrzymaj silnik |

### Odpowiedzi Master → GUI

Aplikacja GUI czyta logi tekstowe z UART. Dane pomiarowe są emitowane przez [`DEBUG_PLOT`](lib/CaliperShared/MacroDebugger.h:113) jako linie zaczynające się od `>`.

**Klucze (Master → GUI):**

```
>measurement:<wartość>
>calibrationOffset:<wartość>
>angleX:<wartość>
>batteryVoltage:<wartość>
>measurementReady:<nazwa_sesji> <wartość>
```

**Znaczenie:**
- `measurement` — surowy pomiar z suwmiarki (mm)
- `calibrationOffset` — offset utrzymywany na Master (mm)
- korekcja jest liczona po stronie GUI/WWW: `corrected = measurement + calibrationOffset`
- `measurementReady` — (dla sesji) wartość już skorygowana wysyłana jako event/log

### Przepływ Komunikacji

```
GUI                             Master
  │                               │
  ├── m\n ───────────────────────>│
  │                               │
  │                               ├─ Prześlij CMD_MEASURE do Slave
  │                               │
  │<── VAL_1:12.345\n ──────────┤
  │                               │
  │<── >Angle X:1.23\n ──────────┤
  │                               │
  │<── >Napiecie baterii:4200\n ──┤
  │                               │
```

### Parametry Portu Szeregowego

| Parametr | Wartość |
|----------|-----------|
| Baud rate | 115200 |
| Data bits | 8 |
| Stop bits | 1 |
| Parity | None |
| Timeout | 200ms |

---

## 3. Protokół HTTP (Interfejs Webowy)

### Endpointy

#### GET /

Zwraca główną stronę HTML z interfejsem użytkownika.

**Pliki serwowane przez LittleFS:**
- `/index.html` - Główna strona
- `/style.css` - Style CSS
- `/app.js` - Logika JavaScript

#### (Aktualne endpointy w firmware)

Firmware Master używa endpointów:
- `GET /` (UI)
- `GET /measure`
- `GET /read`
- `POST /start_session`
- `POST /measure_session`
- `POST /api/calibration/measure`
- `POST /api/calibration/offset`

W odpowiedzi z `POST /measure_session` zwracane są m.in. `measurementRaw` i `calibrationOffset`, a korekcja jest liczona w UI.

#### POST /motor

Steruje silnikiem wyłącznie przez `CMD_MOTORTEST`.

**Parametry (application/x-www-form-urlencoded):**
- `state` – 0..3 (STOP/FORWARD/REVERSE/BRAKE)
- `speed` – 0..255
- `torque` – 0..255

**Odpowiedź JSON:**
```json
{
  "state": 1,
  "speed": 255,
  "torque": 0
}
```

### Przykłady Użycia

#### Sterowanie Silnikiem (curl)
```bash
curl -X POST http://192.168.4.1/motor \
  -H "Content-Type: application/x-www-form-urlencoded" \
  -d "state=1&speed=255&torque=0"
```

#### Pobranie Statusu (curl)
```bash
curl http://192.168.4.1/api/status
```

---

## Obsługa Błędów

### ESP-NOW

| Błąd | Kod | Akcja |
|--------|------|--------|
| Peer nie dodany | ESP_ERR_ESPNOW_NOT_FOUND | Ponów dodanie peer |
| Wysłanie nieudane | !ESP_OK | Ponów wysłanie (max 2 próby) |
| Timeout pomiaru | dataReady = false | Zwróć INVALID_MEASUREMENT_VALUE |

### Serial

| Błąd | Akcja |
|--------|--------|
| Port nie otwarty | Wyświetl "Port not open!" |
| Timeout odczytu | Kontynuuj pętlę |
| Błąd dekodowania | Zaloguj błąd, kontynuuj |

### HTTP

| Błąd | Kod HTTP | Opis |
|--------|-----------|-------|
| Bad Request | 400 | Nieprawidłowe żądanie |
| Not Found | 404 | Endpoint nie istnieje |
| Internal Error | 500 | Błąd serwera |

---

## Bezpieczeństwo

### ESP-NOW
- Brak szyfrowania (encrypt = false)
- Ograniczenie do jednego kanału WiFi
- Weryfikacja adresu MAC peer

### Serial
- Brak szyfrowania
- Walidacja zakresu pomiarów
- Obsługa błędów dekodowania

### HTTP
- Brak autoryzacji (sieć lokalna)
- Walidacja JSON
- Obsługa błędów

---

## Wydajność

### ESP-NOW
- **Latencja:** < 10ms
- **Przepustowość:** ~100 wiadomości/s
- **Zasięg:** ~100m (w warunkach optymalnych)

### Serial
- **Baud rate:** 115200 bps
- **Latencja:** < 1ms
- **Przepustowość:** ~11520 bajtów/s

### HTTP
- **Port:** 80
- **Timeout:** 5s
- **Maksymalny rozmiar żądania:** 1KB

---

*Dokument utworzony: 2025-12-27*
*Wersja: 1.0*
