# Dokumentacja Protokołu Komunikacji Caliper

## Przegląd

Dokumentacja opisuje protokoły komunikacji używane w projekcie Caliper:
1. ESP-NOW - komunikacja między Master a Slave
2. Serial - komunikacja między Master a GUI
3. HTTP - interfejs webowy Master

---

## 1. Protokół ESP-NOW (Master ↔ Slave)

### Struktura Wiadomości

```cpp
struct Message {
    char command;           // Komenda (1 bajt)
    float measurement;       // Wartość pomiaru w mm (4 bajty)
    bool valid;            // Flaga poprawności pomiaru (1 bajt)
    uint32_t timestamp;    // Timestamp w ms (4 bajty)
    uint16_t batteryVoltage; // Napięcie baterii w mV (2 bajty)
    float angleX;           // Kąt X z akcelerometru (4 bajty)
};
```

**Rozmiar struktury:** 16 bajtów

### Komendy

| Komenda | Kod | Kierunek | Opis |
|---------|-------|-----------|-------|
| CMD_MEASURE | 'm' | Master → Slave | Żądanie pomiaru suwmiarki |
| CMD_FORWARD | 'f' | Master → Slave | Silnik do przodu |
| CMD_REVERSE | 'r' | Master → Slave | Silnik do tyłu |
| CMD_STOP | 's' | Master → Slave | Zatrzymaj silnik |
| CMD_UPDATE | 'u' | Slave → Master | Aktualizacja statusu (bateria, kąt) |

### Przepływ Komunikacji

#### 1. Żądanie Pomiaru

```
Master                          Slave
  │                               │
  ├── CMD_MEASURE ('m') ────────>│
  │                               │
  │                               ├─ Wyzwól TRIG_PIN
  │                               ├─ Odczytaj dane suwmiarki
  │                               ├─ Odczytaj akcelerometr
  │                               ├─ Odczytaj baterię
  │                               │
  │<── Message ────────────────────┤
  │   - measurement: 12.345 mm      │
  │   - valid: true                 │
  │   - batteryVoltage: 4200 mV      │
  │   - angleX: 1.23°             │
```

#### 2. Sterowanie Silnikiem

```
Master                          Slave
  │                               │
  ├── CMD_FORWARD ('f') ─────────>│
  │                               ├─ Silnik do przodu (PWM 110/255)
  │                               │
  ├── CMD_REVERSE ('r') ─────────>│
  │                               ├─ Silnik do tyłu (PWM 150/255)
  │                               │
  ├── CMD_STOP ('s') ────────────>│
  │                               ├─ Zatrzymaj silnik (PWM 0)
  │                               │
```

#### 3. Aktualizacja Statusu (okresowa)

```
Slave                           Master
  │                               │
  │<── Message (CMD_UPDATE) ────────┤
  │   - measurement: 0.0            │
  │   - valid: true                 │
  │   - batteryVoltage: 4180 mV      │
  │   - angleX: 1.21°              │
```

**Interwał:** Co ~1 sekunda

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

#### Pomiar Suwmiarki

```
VAL_1:<wartość>
```

**Przykład:**
```
VAL_1:12.345
```

**Walidacja:**
- Zakres: -1000.0 do 1000.0 mm
- Wartości poza zakresem są odrzucane

#### Kąt Akcelerometru

```
>Angle X:<wartość>
```

**Przykład:**
```
>Angle X:1.23
```

#### Napięcie Baterii

```
>Napiecie baterii:<wartość>
```

**Przykład:**
```
>Napiecie baterii:4200
```

#### Kalibracja - Offset

```
CAL_OFFSET:<wartość>
```

**Przykład:**
```
CAL_OFFSET:0.123
```

#### Kalibracja - Błąd

```
CAL_ERROR:<wartość>
```

**Przykład:**
```
CAL_ERROR:Invalid value
```

#### Sesja Pomiarowa

```
MEAS_SESSION:<nazwa> <wartość>
```

**Przykład:**
```
MEAS_SESSION:test1 12.345
```

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

#### GET /api/status

Zwraca aktualny status systemu.

**Odpowiedź JSON:**
```json
{
  "batteryVoltage": 4200,
  "angleX": 1.23,
  "lastMeasurement": 12.345,
  "timestamp": 1703673600000
}
```

#### POST /api/measure

Wyzwala pomiar suwmiarki.

**Żądanie:**
```json
{}
```

**Odpowiedź JSON:**
```json
{
  "success": true,
  "measurement": 12.345,
  "timestamp": 1703673600000
}
```

#### POST /api/motor

Steruje silnikiem.

**Żądanie:**
```json
{
  "command": "forward" | "reverse" | "stop"
}
```

**Odpowiedź JSON:**
```json
{
  "success": true
}
```

### Przykłady Użycia

#### Wyzwolenie Pomiaru (curl)
```bash
curl -X POST http://192.168.4.1/api/measure
```

#### Sterowanie Silnikiem (curl)
```bash
curl -X POST http://192.168.4.1/api/motor \
  -H "Content-Type: application/json" \
  -d '{"command": "forward"}'
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
