# Code Review - Rekomendacje Ulepszeń Projektu Caliper

## 1. ARCHITEKTURA - Responsywność i Współbieżność

### 🔴 KRYTYCZNE: Blokująca pętla w handlerach HTTP

**Problem:** [`caliper_master/src/main.cpp:101-121`](caliper_master/src/main.cpp:101)
```cpp
static bool waitForMeasurementReady(uint32_t timeoutMs) {
    const uint32_t startMs = millis();
    while (!measurementReady) {  // ❌ BLOKUJE cały serwer HTTP!
        if (millis() - startMs >= timeoutMs) return false;
        delay(1);  // Czekanie do 1000ms+
    }
    return true;
}
```

**Wpływ:**
- Serwer HTTP nie odpowiada na inne żądania przez ~1-2 sekundy
- Niemożliwa równoczesna obsługa web UI i GUI Python
- Gorsze doświadczenie użytkownika

**Rozwiązanie:**
✅ Implementacja asynchronicznego API z Request ID (szczegóły w [`plans/multi-client-async-architecture.md`](plans/multi-client-async-architecture.md))

---

## 2. PAMIĘĆ I ZARZĄDZANIE ZASOBAMI

### 🟡 Globalny stan zamiast enkapsulacji

**Problem:** [`caliper_master/src/main.cpp:29-34`](caliper_master/src/main.cpp:29)
```cpp
// Zmienne globalne - trudne do testowania i zarządzania
String lastMeasurement = "Brak pomiaru";
String lastBatteryVoltage = "Brak danych";
float lastMeasurementValue = 0.0f;
bool measurementReady = false;
```

**Rekomendacja:**
```cpp
// Enkapsulacja w klasę lub strukturę
class MeasurementState {
private:
    String lastMeasurement;
    float lastValue;
    bool ready;
    
public:
    void setMeasurement(float value);
    bool isReady() const { return ready; }
    float getValue() const { return lastValue; }
};

static MeasurementState measurementState;
```

**Korzyści:**
- Łatwiejsze testowanie jednostkowe
- Lepsze zarządzanie cyklem życia zmiennych
- Unikanie race conditions przy współbieżności

### 🟡 Optymalizacja String w ESP32

**Problem:** Używanie `String` w miejscach krytycznych dla wydajności
```cpp
String lastMeasurement = "Brak pomiaru";  // ❌ Dynamiczna alokacja
lastMeasurement = String("Komenda: ") + commandName;  // ❌ Fragmentacja pamięci
```

**Rekomendacja:**
```cpp
// Użyj statycznych buforów dla krytycznych ścieżek
static char lastMeasurement[64] = "Brak pomiaru";
snprintf(lastMeasurement, sizeof(lastMeasurement), "Komenda: %s", commandName);
```

**Korzyści:**
- Brak fragmentacji heap
- Przewidywalne zużycie pamięci
- Szybsze operacje (brak malloc/free)

---

## 3. BEZPIECZEŃSTWO I WALIDACJA

### 🟡 Walidacja danych wejściowych HTTP

**Problem:** [`caliper_master/src/main.cpp:244`](caliper_master/src/main.cpp:244)
```cpp
void handleCalibrationSetOffset() {
    const String offsetStr = server.arg("offset");
    float offsetValue = 0.0f;
    
    if (!parseFloatStrict(offsetStr, offsetValue)) {
        // ✅ Dobra walidacja
    }
    
    if (offsetValue < -14.999f || offsetValue > 14.999f) {
        // ✅ Dobra walidacja zakresu
    }
}
```

**Rekomendacja:** Dodaj więcej walidacji
```cpp
// Sprawdź czy parametr w ogóle istnieje
if (!server.hasArg("offset")) {
    server.send(400, "application/json", "{\"error\":\"Brak parametru offset\"}");
    return;
}

// Zabezpieczenie przed NaN/Infinity
if (isnan(offsetValue) || isinf(offsetValue)) {
    server.send(400, "application/json", "{\"error\":\"Nieprawidłowa wartość\"}");
    return;
}
```

### 🟢 Pozytyw: Dobra walidacja nazwy sesji

[`caliper_master/src/serial_cli.cpp:78-106`](caliper_master/src/serial_cli.cpp:78) - Walidacja jest kompleksowa i bezpieczna! ✅

---

## 4. KOMUNIKACJA ESP-NOW

### 🟡 Brak mechanizmu ponawiania prób na Slave

**Problem:** [`caliper_slave/src/main.cpp:107-127`](caliper_slave/src/main.cpp:107)
```cpp
esp_err_t sendResult = esp_now_send(masterAddress, (uint8_t *)&msgSlave, sizeof(msgSlave));
if (sendResult == ESP_OK) {
    DEBUG_I("Wynik wysłany do Mastera");
} else {
    DEBUG_E("BŁĄD wysyłania wyniku: %d", (int)sendResult);
    
    // ✅ Ponawianie próby
    delay(ESPNOW_RETRY_DELAY_MS);
    sendResult = esp_now_send(...);
}
```

**Rekomendacja:** Ujednolicenie z Masterem
```cpp
// Master ma lepszą implementację z retry loop
// Przenieś logikę do wspólnego modułu w lib/CaliperShared
ErrorCode espnow_send_with_retry(
    const uint8_t* mac, 
    const void* data, 
    size_t len,
    int maxRetries = ESPNOW_MAX_RETRIES
);
```

### 🟡 Brak Request ID w obecnej komunikacji

**Problem:** [`lib/CaliperShared/shared_common.h:59-74`](lib/CaliperShared/shared_common.h:59)
```cpp
struct MessageSlave {
    float measurement;
    float batteryVoltage;
    CommandType command;
    uint8_t angleX;
    // ❌ Brak requestId
};
```

**Rekomendacja:** Dodaj Request ID (opisane w architekturze asynchronicznej)
```cpp
struct MessageSlave {
    uint32_t requestId;      // ✅ NOWE
    float measurement;
    float batteryVoltage;
    CommandType command;
    uint8_t angleX;
};
```

---

## 5. ZARZĄDZANIE BŁĘDAMI

### 🟡 Niekonsekwentna obsługa błędów

**Problem:** Różne style obsługi błędów w różnych miejscach

**Przykłady:**
```cpp
// Styl 1: Bezpośredni return z kodem błędu
if (!file) {
    server.send(500, "text/plain", "Failed to open index.html");
    return;
}

// Styl 2: ErrorCode enum
ErrorCode result = commManager.sendMessage(systemStatus.msgMaster);
if (result != ERR_NONE) {
    DEBUG_E("BLAD wysylania komendy %s: %d", commandName, (int)result);
}

// Styl 3: Boolean
bool success = prefsManager.begin();
if (!success) {
    DEBUG_W("PreferencesManager initialization failed");
}
```

**Rekomendacja:** Ujednolicony system błędów
```cpp
// Rozszerz ErrorCode enum
enum ErrorCode : uint8_t {
    ERR_NONE = 0,
    ERR_ESPNOW_SEND,
    ERR_MEASUREMENT_TIMEOUT,
    ERR_INVALID_DATA,
    ERR_ADC_READ,
    ERR_INVALID_COMMAND,
    // ✅ NOWE
    ERR_FILE_NOT_FOUND,
    ERR_HTTP_INVALID_REQUEST,
    ERR_PREFERENCES_FAILED,
    ERR_OUT_OF_MEMORY
};

// Użyj wszędzie
ErrorCode handleRoot() {
    File file = LittleFS.open("/index.html", "r");
    if (!file) {
        return ERR_FILE_NOT_FOUND;
    }
    // ...
    return ERR_NONE;
}
```

### 🟢 Pozytyw: Dobra obsługa timeout

[`caliper_slave/src/sensors/caliper.cpp:90-119`](caliper_slave/src/sensors/caliper.cpp:90) - Timeout + walidacja wyniku ✅

---

## 6. SENSORY I POMIARY

### 🟡 Dekodowanie calipers - magic numbers

**Problem:** [`caliper_slave/src/sensors/caliper.cpp:42-70`](caliper_slave/src/sensors/caliper.cpp:42)
```cpp
uint8_t shifted[52];  // ❌ Co to jest 52?
for (int i = 0; i < 52; i++) {
    if (i + 8 < 52)  // ❌ Co to jest 8?
        shifted[i] = bitBuffer[i + 8];
}

uint8_t nibbles[13];  // ❌ Co to jest 13?
```

**Rekomendacja:**
```cpp
// Zdefiniuj stałe z opisami
static constexpr uint8_t CALIPER_TOTAL_BITS = 52;
static constexpr uint8_t CALIPER_HEADER_BITS = 8;
static constexpr uint8_t CALIPER_DATA_NIBBLES = 13;
static constexpr uint8_t CALIPER_VALUE_NIBBLES = 5;
static constexpr float CALIPER_DIVISION_FACTOR = 1000.0f;
static constexpr float INCH_TO_MM = 25.4f;

float CaliperInterface::decodeCaliper() {
    uint8_t shifted[CALIPER_TOTAL_BITS];
    for (int i = 0; i < CALIPER_TOTAL_BITS; i++) {
        if (i + CALIPER_HEADER_BITS < CALIPER_TOTAL_BITS)
            shifted[i] = bitBuffer[i + CALIPER_HEADER_BITS];
        // ...
    }
}
```

### 🟡 Accelerometer - brak error handling

**Problem:** [`caliper_slave/src/sensors/accelerometer.cpp:32-35`](caliper_slave/src/sensors/accelerometer.cpp:32)
```cpp
void AccelerometerInterface::update() {
    myAcc.getAngles(&angle);  // ❌ Co jeśli sensor nie odpowiada?
}
```

**Rekomendacja:**
```cpp
bool AccelerometerInterface::update() {
    if (!myAcc.isConnected()) {
        DEBUG_W("ADXL345 disconnected!");
        return false;
    }
    
    if (!myAcc.getAngles(&angle)) {
        DEBUG_E("Failed to read angles from ADXL345");
        return false;
    }
    
    return true;
}
```

---

## 7. WEB UI I INTERFEJS UŻYTKOWNIKA

### 🟡 JavaScript - brak error boundaries

**Problem:** [`caliper_master/data/app.js:45-83`](caliper_master/data/app.js:45)
```javascript
function calibrationMeasure() {
    fetch('/api/calibration/measure', { method: 'POST' })
    .then(response => {
        // ❌ Co jeśli network error?
        return response.json();
    })
    .catch(error => {
        elStatus.textContent = 'Błąd: ' + error.message;
        // ❌ Tylko wyświetlenie, brak recovery
    });
}
```

**Rekomendacja:**
```javascript
async function calibrationMeasure() {
    const elStatus = document.getElementById('cal-status');
    
    try {
        elStatus.textContent = 'Pobieranie bieżącego pomiaru...';
        
        const controller = new AbortController();
        const timeoutId = setTimeout(() => controller.abort(), 5000);
        
        const response = await fetch('/api/calibration/measure', {
            method: 'POST',
            signal: controller.signal
        });
        
        clearTimeout(timeoutId);
        
        if (!response.ok) {
            throw new Error(`HTTP ${response.status}: ${response.statusText}`);
        }
        
        const data = await response.json();
        // ... process data
        
    } catch (error) {
        if (error.name === 'AbortError') {
            elStatus.textContent = 'Błąd: Timeout (>5s)';
        } else if (!navigator.onLine) {
            elStatus.textContent = 'Błąd: Brak połączenia sieciowego';
        } else {
            elStatus.textContent = 'Błąd: ' + error.message;
        }
        
        // Retry logic
        console.error('Measurement failed:', error);
    }
}
```

### 🟢 Pozytyw: Walidacja po stronie JS

[`caliper_master/data/app.js:127-145`](caliper_master/data/app.js:127) - Dobra walidacja nazwy sesji ✅

---

## 8. PYTHON GUI

### 🟡 Threading i synchronizacja

**Problem:** [`caliper_master_gui/caliper_master_gui.py:260-286`](caliper_master_gui/caliper_master_gui.py:260)
```python
def key_press_handler(self, sender, key):
    if key == dpg.mvKey_P:
        # ❌ Bezpośrednie wywołanie I/O w handler UI
        self.serial_handler.write("m")
```

**Rekomendacja:**
```python
import queue
import threading

class CaliperGUI:
    def __init__(self):
        # Command queue dla bezpiecznej komunikacji między wątkami
        self.command_queue = queue.Queue()
        self.command_thread = threading.Thread(target=self._command_worker, daemon=True)
        self.command_thread.start()
    
    def _command_worker(self):
        """Worker thread dla komend serial"""
        while True:
            cmd = self.command_queue.get()
            try:
                self.serial_handler.write(cmd)
            except Exception as e:
                self.calibration_tab.add_app_log(f"ERROR: {e}")
            finally:
                self.command_queue.task_done()
    
    def key_press_handler(self, sender, key):
        if key == dpg.mvKey_P:
            # ✅ Asynchroniczne wywołanie
            self.command_queue.put("m")
```

### 🟡 Parsowanie danych - magic strings

**Problem:** [`caliper_master_gui/caliper_master_gui.py:84-124`](caliper_master_gui/caliper_master_gui.py:84)
```python
if data.startswith("measurement:"):  # ❌ Magic string
    val_str = data.split(":", 1)[1].strip()
```

**Rekomendacja:**
```python
# Constants module
class PlotDataKeys:
    MEASUREMENT = "measurement:"
    ANGLE_X = "angleX:"
    BATTERY = "batteryVoltage:"
    CALIBRATION = "calibrationOffset:"
    TIMEOUT = "timeout:"
    # ...

def process_measurement_data(self, data: str):
    if data.startswith(PlotDataKeys.MEASUREMENT):
        val_str = data[len(PlotDataKeys.MEASUREMENT):].strip()
        # ...
```

---

## 9. KONFIGURACJA I MAINTENANCE

### 🟢 Pozytyw: Centralna konfiguracja

[`lib/CaliperShared/shared_config.h`](lib/CaliperShared/shared_config.h) - Dobra separacja konfiguracji współdzielonej! ✅

### 🟡 Hardcoded wartości w GUI

**Problem:** [`caliper_master_gui/caliper_master_gui.py:301-312`](caliper_master_gui/caliper_master_gui.py:301)
```python
with dpg.font("C:/Windows/Fonts/segoeui.ttf", 22) as default_font:
    # ❌ Hardcoded path - nie działa na Linux/Mac
```

**Rekomendacja:**
```python
import platform
import os

def get_system_font():
    """Get appropriate font path for current OS"""
    system = platform.system()
    
    if system == "Windows":
        return "C:/Windows/Fonts/segoeui.ttf"
    elif system == "Darwin":  # macOS
        return "/System/Library/Fonts/Helvetica.ttc"
    else:  # Linux
        # Fallback to default DearPyGUI font
        return None

font_path = get_system_font()
if font_path and os.path.exists(font_path):
    with dpg.font(font_path, 22) as default_font:
        # ...
```

---

## 10. TESTY I DOKUMENTACJA

### 🔴 BRAK: Testy jednostkowe dla firmware

**Problem:** Brak testów dla kluczowych funkcji:
- Dekodowanie calipers
- Parsing komend serial
- Walidacja danych

**Rekomendacja:** Dodaj testy z PlatformIO + Unity
```cpp
// test/test_caliper_decode.cpp
#include <unity.h>
#include "../src/sensors/caliper.h"

void test_decode_positive_value() {
    CaliperInterface caliper;
    // Setup mock bitBuffer
    caliper.bitBuffer = { /* known good data */ };
    
    float result = caliper.decodeCaliper();
    TEST_ASSERT_FLOAT_WITHIN(0.001, 12.345, result);
}

void test_decode_negative_value() {
    // ...
}

void test_decode_inch_mode() {
    // ...
}
```

### 🟢 Pozytyw: Python ma testy

[`caliper_master_gui/tests/test_serial.py`](caliper_master_gui/tests/test_serial.py) - Dobry początek! ✅

---

## 11. PERFORMANCE

### 🟡 Optymalizacja CPU

**Problem:** [`caliper_master/src/main.cpp:467-471`](caliper_master/src/main.cpp:467)
```cpp
void loop() {
    server.handleClient();  // Polling HTTP - może być wolne
    timerWorker.tick();
}
```

**Rekomendacja:**
```cpp
// Dodaj yield dla WDT (watchdog timer)
void loop() {
    server.handleClient();
    timerWorker.tick();
    
    // Opcjonalne: sleep jeśli idle
    if (shouldSleep()) {
        delay(10);  // Reduce CPU usage
    }
    
    yield();  // ✅ Feed watchdog
}
```

### 🟡 Prealokacja buforów

**Problem:** Dynamiczna alokacja w pętli pomiarowej

**Rekomendacja:**
```cpp
// Zamiast dynamicznych String
static char jsonBuffer[512];  // ✅ Prealokowane
snprintf(jsonBuffer, sizeof(jsonBuffer), 
    "{\"measurement\":%.3f,\"battery\":%.3f}", 
    measurement, battery);
server.send(200, "application/json", jsonBuffer);
```

---

## PODSUMOWANIE PRIORYTETÓW

### 🔴 WYSOKIE (Implementuj najpierw)

1. ✅ **Asynchroniczna architektura z Request ID** - rozwiązuje problem responsywności
2. ✅ **Ujednolicony system błędów** - lepsze debugowanie i obsługa edge cases
3. ✅ **Walidacja danych wejściowych** - bezpieczeństwo i stabilność

### 🟡 ŚREDNIE (Implementuj później)

4. ✅ **Optymalizacja pamięci** - zmniejszenie fragmentacji heap
5. ✅ **Testy jednostkowe** - jakość i maintainability
6. ✅ **Lepsze error handling w JS/Python** - UX

### 🟢 NISKIE (Nice to have)

7. ✅ **Refactoring magic numbers** - czytelność kodu
8. ✅ **Cross-platform font paths** - kompatybilność
9. ✅ **Performance tweaks** - optymalizacja CPU/RAM

---

## NASTĘPNE KROKI

1. **Zaimplementuj asynchroniczną architekturę** (najważniejsze!)
   - Request ID w strukturach komunikacyjnych
   - Bufor wyników
   - Nowe API HTTP endpoints

2. **Dodaj testy jednostkowe**
   - Caliper decoding
   - Serial command parsing
   - Validation functions

3. **Ujednolicenie error handling**
   - Rozszerz ErrorCode enum
   - Konsekwentne używanie w całym projekcie

4. **Code cleanup**
   - Zamień magic numbers na stałe
   - Optymalizuj String → char buffers
   - Dodaj komentarze do złożonej logiki

## METRYKI JAKOŚCI KODU

| Aspekt | Obecny stan | Cel | Priorytet |
|--------|-------------|-----|-----------|
| Responsywność | ⚠️ Blokujące API | ✅ Async + Request ID | 🔴 WYSOKI |
| Error handling | ⚠️ Niespójne | ✅ Ujednolicone | 🟡 ŚREDNI |
| Testy | ❌ Brak (firmware) | ✅ >80% coverage | 🟡 ŚREDNI |
| Dokumentacja | ✅ Dobra | ✅ Excellent | 🟢 NISKI |
| Optymalizacja RAM | ⚠️ String overuse | ✅ Static buffers | 🟡 ŚREDNI |
| Cross-platform | ⚠️ Windows-only GUI | ✅ Linux/Mac/Win | 🟢 NISKI |
