# System Obsługi Błędów - Dokumentacja

## Przegląd

Ten dokument opisuje zintegrowany system obsługi błędów dla projektu Caliper ESP32. System zapewnia spójne, szczegółowe i łatwe do debugowania mechanizmy zarządzania błędami w całym projekcie.

## Wersja

- **Wersja**: 1.0
- **Data**: 2026-01-04
- **Autor**: System Generated

## Struktura Systemu

### Pliki

1. **`error_codes.h`** - Definicje kodów błędów, kategorii i modułów
2. **`error_codes.cpp`** - Implementacja funkcji pomocniczych do dekodowania błędów
3. **`error_handler.h`** - Makra do logowania błędów i klasa ErrorHandler
4. **`shared_common.h`** - Zaktualizowany o integrację z nowym systemem

## Format Kodu Błędu

Każdy kod błędu to 16-bitowa wartość w formacie:

```
[Category:4 bits][Module:4 bits][Code:8 bits]
```

### Przykład

```
0x0102 = Category:0x01 (Communication) | Module:0x01 (ESP-NOW) | Code:0x02 (Send Failed)
```

## Kategorie Błędów

| ID | Nazwa | Opis |
|-----|--------|--------|
| 0x00 | NONE | Brak błędu |
| 0x01 | COMMUNICATION | Błędy komunikacji (ESP-NOW, Serial, WiFi) |
| 0x02 | SENSOR | Błędy sensorów (suwmiarka, akcelerometr) |
| 0x03 | MOTOR | Błędy sterownika silnika |
| 0x04 | POWER | Błędy zasilania (bateria, ADC) |
| 0x05 | STORAGE | Błędy pamięci (LittleFS, NVS/Preferences) |
| 0x06 | NETWORK | Błędy sieci (WiFi AP, Web Server) |
| 0x07 | VALIDATION | Błędy walidacji danych |
| 0x08 | SYSTEM | Błędy systemowe |

## Moduły Źródłowe

| ID | Nazwa | Opis |
|-----|--------|--------|
| 0x00 | NONE | Brak określonego modułu |
| 0x01 | ESPNOW | Komunikacja ESP-NOW |
| 0x02 | SERIAL | Komunikacja szeregowa |
| 0x03 | CALIPER | Sensor suwmiarki |
| 0x04 | ACCELEROMETER | Sensor akcelerometru |
| 0x05 | MOTOR_CTRL | Sterownik silnika |
| 0x06 | BATTERY | Monitor baterii |
| 0x07 | LITTLEFS | System plików LittleFS |
| 0x08 | PREFERENCES | Pamięć Preferences/NVS |
| 0x09 | WEB_SERVER | Serwer WWW |
| 0x0A | CLI | Interfejs CLI |

## Pełna Lista Kodów Błędów

### Komunikacja (0x01XX)

| Kod | Nazwa | Opis | Odwracalny |
|------|--------|--------|-------------|
| 0x0101 | ERR_ESPNOW_INIT_FAILED | Inicjalizacja ESP-NOW nieudana | Nie |
| 0x0102 | ERR_ESPNOW_SEND_FAILED | Wysłanie ESP-NOW nieudane | Tak |
| 0x0103 | ERR_ESPNOW_RECV_FAILED | Odbiór ESP-NOW nieudany | Tak |
| 0x0104 | ERR_ESPNOW_PEER_ADD_FAILED | Dodanie peera nieudane | Nie |
| 0x0105 | ERR_ESPNOW_INVALID_LENGTH | Nieprawidłowa długość pakietu | Tak |
| 0x0106 | ERR_SERIAL_COMM_ERROR | Błąd komunikacji szeregowej | Tak |
| 0x0107 | ERR_SERIAL_TIMEOUT | Timeout operacji szeregowej | Tak |

### Sensory (0x02XX)

| Kod | Nazwa | Opis | Odwracalny |
|------|--------|--------|-------------|
| 0x0201 | ERR_CALIPER_TIMEOUT | Timeout pomiaru suwmiarki | Tak |
| 0x0202 | ERR_CALIPER_INVALID_DATA | Nieprawidłowe dane suwmiarki | Tak |
| 0x0203 | ERR_CALIPER_OUT_OF_RANGE | Pomiar poza zakresem | Tak |
| 0x0204 | ERR_CALIPER_HARDWARE_FAILURE | Awaria sprzętu suwmiarki | Nie |
| 0x0205 | ERR_ACCEL_INIT_FAILED | Inicjalizacja akcelerometru nieudana | Nie |
| 0x0206 | ERR_ACCEL_READ_FAILED | Odczyt akcelerometru nieudany | Tak |
| 0x0207 | ERR_ACCEL_I2C_ERROR | Błąd I2C akcelerometru | Tak |

### Motor (0x03XX)

| Kod | Nazwa | Opis | Odwracalny |
|------|--------|--------|-------------|
| 0x0301 | ERR_MOTOR_INVALID_DIRECTION | Nieprawidłowy kierunek silnika | Tak |
| 0x0302 | ERR_MOTOR_HARDWARE_FAILURE | Awaria sprzętu silnika | Nie |

### Zasilanie (0x04XX)

| Kod | Nazwa | Opis | Odwracalny |
|------|--------|--------|-------------|
| 0x0401 | ERR_BATTERY_READ_FAILED | Odczyt baterii nieudany | Tak |
| 0x0402 | ERR_BATTERY_LOW_VOLTAGE | Niskie napięcie baterii | Nie |
| 0x0403 | ERR_ADC_READ_FAILED | Błąd odczytu ADC | Tak |

### Pamięć (0x05XX)

| Kod | Nazwa | Opis | Odwracalny |
|------|--------|--------|-------------|
| 0x0501 | ERR_LITTLEFS_MOUNT_FAILED | Montowanie LittleFS nieudane | Nie |
| 0x0502 | ERR_LITTLEFS_FILE_NOT_FOUND | Plik nie znaleziony | Tak |
| 0x0503 | ERR_LITTLEFS_READ_FAILED | Odczyt LittleFS nieudany | Tak |
| 0x0504 | ERR_LITTLEFS_WRITE_FAILED | Zapis LittleFS nieudany | Tak |
| 0x0505 | ERR_PREFS_INIT_FAILED | Inicjalizacja Preferences nieudana | Nie |
| 0x0506 | ERR_PREFS_LOAD_FAILED | Wczytanie Preferences nieudane | Tak |
| 0x0507 | ERR_PREFS_SAVE_FAILED | Zapis Preferences nieudany | Tak |
| 0x0508 | ERR_PREFS_INVALID_VALUE | Nieprawidłowa wartość Preferences | Tak |

### Sieć (0x06XX)

| Kod | Nazwa | Opis | Odwracalny |
|------|--------|--------|-------------|
| 0x0601 | ERR_WEB_SERVER_INIT_FAILED | Inicjalizacja serwera WWW nieudana | Nie |
| 0x0602 | ERR_WEB_SERVER_ROUTE_FAILED | Obsługa trasy nieudana | Tak |
| 0x0603 | ERR_WIFI_INIT_FAILED | Inicjalizacja WiFi nieudana | Nie |
| 0x0604 | ERR_WIFI_AP_CONFIG_FAILED | Konfiguracja AP WiFi nieudana | Nie |

### Walidacja (0x07XX)

| Kod | Nazwa | Opis | Odwracalny |
|------|--------|--------|-------------|
| 0x0701 | ERR_VALIDATION_INVALID_PARAM | Nieprawidłowy parametr | Tak |
| 0x0702 | ERR_VALIDATION_OUT_OF_RANGE | Wartość poza zakresem | Tak |
| 0x0703 | ERR_VALIDATION_INVALID_FORMAT | Nieprawidłowy format danych | Tak |
| 0x0704 | ERR_VALIDATION_SESSION_INACTIVE | Sesja nieaktywna | Tak |
| 0x0705 | ERR_VALIDATION_INVALID_COMMAND | Nieprawidłowa komenda | Tak |

### System (0x08XX)

| Kod | Nazwa | Opis | Odwracalny |
|------|--------|--------|-------------|
| 0x0801 | ERR_SYSTEM_WIFI_INIT_FAILED | Inicjalizacja WiFi systemowa nieudana | Nie |
| 0x0802 | ERR_SYSTEM_MEMORY_ALLOC_FAILED | Alokacja pamięci nieudana | Tak |
| 0x0803 | ERR_SYSTEM_UNKNOWN_ERROR | Nieznany błąd systemowy | Tak |
| 0x0804 | ERR_SYSTEM_NULL_POINTER | Odwołanie do null pointer | Nie |

## Funkcje Pomocnicze

### Dekodowanie Błędów

```cpp
// Pobranie kategorii z kodu błędu
ErrorCategory cat = getErrorCategory(errorCode);

// Pobranie modułu z kodu błędu
ErrorModule mod = getErrorModule(errorCode);

// Pobranie kodu specyficznego
uint8_t code = getErrorCode(errorCode);

// Pobranie nazwy kategorii
const char* catName = getErrorCategoryName(cat);

// Pobranie nazwy modułu
const char* modName = getErrorModuleName(mod);

// Pobranie opisu błędu
const char* desc = getErrorDescription(errorCode);

// Pobranie akcji naprawczej
const char* recovery = getErrorRecoveryAction(errorCode);
```

### Walidacja Błędów

```cpp
// Sprawdzenie czy kod błędu jest prawidłowy
bool valid = isValidErrorCode(errorCode);

// Sprawdzenie czy błąd jest odwracalny
bool recoverable = isRecoverableError(errorCode);

// Pobranie poziomu ważności (0=info, 1=warning, 2=error, 3=critical)
uint8_t severity = getErrorSeverity(errorCode);
```

## Makra do Logowania

### Podstawowe Makra

```cpp
// Logowanie błędu z pełnymi szczegółami
LOG_ERROR(errorCode, "Dodatkowe informacje...");

// Logowanie ostrzeżenia
LOG_WARNING(errorCode, "Dodatkowe informacje...");

// Logowanie informacji
LOG_INFO(errorCode, "Dodatkowe informacje...");
```

### Makra Warunkowe

```cpp
// Zwróć błąd i zaloguj go
RETURN_ERROR(errorCode, "Szczegóły...");

// Zwróć błąd jeśli warunek jest prawdziwy
RETURN_IF_ERROR(condition, errorCode, "Szczegóły...");

// Zwróć błąd jeśli kod nie jest ERR_NONE
RETURN_IF_NOT_OK(errorCode, "Szczegóły...");

// Zaloguj ostrzeżenie jeśli warunek jest prawdziwy
WARN_IF(condition, errorCode, "Szczegóły...");

// Asercja z logowaniem błędu
ERROR_ASSERT(condition, errorCode, "Szczegóły...");
```

### Makra do Śledzenia

```cpp
// Zapisz błąd i zaloguj go
RECORD_ERROR(errorCode, "Szczegóły...");

// Zapisz błąd, zaloguj go i zwróć
RECORD_AND_RETURN_ERROR(errorCode, "Szczegóły...");

// Pobierz instancję handlera błędów
ERROR_HANDLER
```

## Klasa ErrorHandler

Klasa `ErrorHandler` udostępnia singleton do śledzenia statystyk błędów.

### Metody

```cpp
// Inicjalizacja handlera
ERROR_HANDLER.initialize();

// Zapisanie błędu do statystyk
ERROR_HANDLER.recordError(errorCode);

// Pobranie statystyk
const ErrorStats& stats = ERROR_HANDLER.getStats();

// Reset statystyk
ERROR_HANDLER.resetStats();

// Pobranie ostatniego błędu
ErrorCode lastError = ERROR_HANDLER.getLastError();

// Czas od ostatniego błędu
uint32_t timeSince = ERROR_HANDLER.getTimeSinceLastError();
```

### Struktura ErrorStats

```cpp
struct ErrorStats {
  uint32_t totalErrors;      // Łączna liczba błędów
  uint32_t recoverableErrors; // Liczba błędów odwracalnych
  uint32_t criticalErrors;    // Liczba błędów krytycznych
  uint32_t lastErrorTime;    // Czas ostatniego błędu (ms)
  ErrorCode lastErrorCode;    // Ostatni kod błędu
};
```

## Przykłady Użycia

### Przykład 1: Obsługa błędu komunikacji

```cpp
ErrorCode result = commManager.sendMessage(msg);
if (result != ERR_NONE) {
  LOG_ERROR(result, "Failed to send message to slave");
  return result;
}
```

### Przykład 2: Walidacja parametrów

```cpp
if (value < MIN_VALUE || value > MAX_VALUE) {
  RETURN_ERROR(ERR_VALIDATION_OUT_OF_RANGE, 
    "Value %.2f out of range [%.2f, %.2f]", 
    value, MIN_VALUE, MAX_VALUE);
}
```

### Przykład 3: Obsługa błędu sensora

```cpp
float measurement = caliper.performMeasurement();
if (measurement == INVALID_MEASUREMENT_VALUE) {
  RECORD_ERROR(ERR_CALIPER_TIMEOUT, "Caliper measurement timeout");
  return ERR_CALIPER_TIMEOUT;
}
```

### Przykład 4: Śledzenie statystyk błędów

```cpp
void setup() {
  ERROR_HANDLER.initialize();
  // ... inicjalizacja
}

void loop() {
  if (errorOccurred) {
    ERROR_HANDLER.recordError(lastError);
    
    const ErrorStats& stats = ERROR_HANDLER.getStats();
    DEBUG_I("Total errors: %u, Critical: %u", 
      stats.totalErrors, stats.criticalErrors);
  }
}
```

## Poziomy Ważności Błędów

| Poziom | Wartość | Opis |
|----------|-----------|--------|
| INFO | 0 | Informacyjne |
| WARNING | 1 | Ostrzeżenie |
| ERROR | 2 | Błąd |
| CRITICAL | 3 | Błąd krytyczny |

## Zalecenia dla Deweloperów

### Dodawanie Nowych Kodów Błędów

1. **Wybierz odpowiednią kategorię** (0x00-0x08)
2. **Wybierz odpowiedni moduł** (0x00-0x0A)
3. **Przypisz unikalny kod** (0x01-0xFF)
4. **Dodaj opis błędu** w `error_codes.cpp`
5. **Dodaj akcję naprawczą** w `error_codes.cpp`
6. **Zaktualizuj `isRecoverableError()`** jeśli potrzebne
7. **Zaktualizuj `getErrorSeverity()`** jeśli potrzebne

### Przykład Dodania Nowego Błędu

```cpp
// W error_codes.h
enum ErrorCode : uint16_t {
  // ... istniejące kody ...
  ERR_MY_NEW_ERROR = 0x0208,  // Category: SENSOR, Module: CALIPER, Code: 0x08
};

// W error_codes.cpp
const char* getErrorDescription(ErrorCode code) {
  switch (code) {
    // ... istniejące przypadki ...
    case ERR_MY_NEW_ERROR:
      return "My new error description";
    default:
      return "Unknown error code";
  }
}

const char* getErrorRecoveryAction(ErrorCode code) {
  switch (code) {
    // ... istniejące przypadki ...
    case ERR_MY_NEW_ERROR:
      return "Check XYZ, verify ABC, retry operation";
    default:
      return "Unknown error - check logs and contact support";
  }
}
```

### Zalecenia dotyczące Logowania

1. **Używaj makr LOG_* zamiast DEBUG_E/W/I** dla błędów
2. **Dołączaj kontekst** w komunikatach logowania (wartości parametrów, stany)
3. **Używaj makr warunkowych** (RETURN_IF_ERROR, WARN_IF) dla czytelności
4. **Zapisuj błędy do statystyk** używając RECORD_ERROR

### Zalecenia dotyczące Obsługi Błędów

1. **Zawsze sprawdzaj kody powrotne** z funkcji
2. **Używaj RETURN_IF_NOT_OK** dla czytelnego sprawdzania błędów
3. **Nie ignoruj błędów** - loguj je i podejmij akcję naprawczą
4. **Dokumentuj możliwe błędy** w komentarzach funkcji

## Kompatybilność Wsteczna

Stary system kodów błędów (`ErrorCodeLegacy`) jest zachowany dla kompatybilności:

| Stary Kod | Nowy Kod |
|------------|------------|
| ERR_LEGACY_NONE | ERR_NONE (0x0000) |
| ERR_LEGACY_ESPNOW_SEND | ERR_ESPNOW_SEND_FAILED (0x0102) |
| ERR_LEGACY_MEASUREMENT_TIMEOUT | ERR_CALIPER_TIMEOUT (0x0201) |
| ERR_LEGACY_INVALID_DATA | ERR_CALIPER_INVALID_DATA (0x0202) |
| ERR_LEGACY_ADC_READ | ERR_ADC_READ_FAILED (0x0403) |
| ERR_LEGACY_INVALID_COMMAND | ERR_VALIDATION_INVALID_COMMAND (0x0705) |

**Uwaga**: Nowy kod powinien używać `ErrorCode` z `error_codes.h` zamiast `ErrorCodeLegacy`.

## Przyszłe Rozwinięcia

Możliwości rozszerzenia systemu:

1. **Dodanie obsługi wyjątków C++** dla lepszej integracji z kodem
2. **Dodanie mechanizmu retry** z automatycznym ponawianiem operacji
3. **Dodanie logowania do pliku** na LittleFS dla trwałego przechowywania
4. **Dodanie powiadomień push** o krytycznych błędach
5. **Dodanie dashboardu błędów** w interfejsie WWW/GUI

## Podsumowanie

Nowy system obsługi błędów zapewnia:

✅ **Spójność** - Jednolity format kodów w całym projekcie
✅ **Szczegółowość** - Kategorie, moduły, opisy i akcje naprawcze
✅ **Czytelność** - Łatwe do zrozumienia logi z pełnym kontekstem
✅ **Debugowalność** - Możliwość śledzenia statystyk i czasu wystąpienia
✅ **Rozszerzalność** - Łatwe dodawanie nowych kodów błędów
✅ **Kompatybilność** - Zachowanie starego systemu dla wstecznej kompatybilności

System jest gotowy do użycia we wszystkich modułach projektu Caliper ESP32.
