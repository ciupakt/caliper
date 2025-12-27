# Changelog

Wszystkie istotne zmiany w projekcie Caliper.

## [Unreleased]

### Dodane
- Modularyzacja kodu Slave (sensors/, motor/, power/)
- Modularyzacja kodu Python GUI (src/, tests/)
- Dokumentacja architektury systemu
- Dokumentacja protokołów komunikacji
- Współdzielona biblioteka CaliperShared

### Zmienione
- Przeniesienie logiki suwmiarki do `sensors/caliper.h/cpp`
- Przeniesienie logiki akcelerometru do `sensors/accelerometer.h/cpp`
- Przeniesienie logiki baterii do `power/battery.h/cpp`
- Przeniesienie logiki silnika do `motor/motor_ctrl.h/cpp`
- Zredukowanie `main.cpp` w Slave
- Przeniesienie HTML/CSS/JS do LittleFS w Master

### Usunięte
- Duplikacja kodu (common.h w Master i Slave)
- Stare pliki sterownika silnika (caliper_slave_motor_ctrl.h/cpp)

---

## [1.0.0] - 2025-12-26

### Dodane
- Wstępna implementacja systemu Caliper
- Komunikacja ESP-NOW między Master a Slave
- Obsługa suwmiarki cyfrowej
- Obsługa akcelerometru ADXL345
- Sterowanie silnikiem DC MP6550GG-Z
- Monitorowanie napięcia baterii
- Interfejs webowy Master (HTTP)
- Aplikacja GUI w Pythonie (DearPyGUI)
- Eksport danych do CSV
- Obsługa kalibracji

### Znane problemy
- Duplikacja kodu między Master a Slave
- HTML/CSS/JS inline w kodzie C++
- Adresy MAC hardkodowane
- Brak modularyzacji kodu Python GUI

---

## Format Wersji

Wersje są formatowane zgodnie z [Semantic Versioning](https://semver.org/spec/v2.0.0.html):
- **MAJOR**: Zmiany niekompatybilne wstecz
- **MINOR**: Nowe funkcjonalności kompatybilne wstecz
- **PATCH**: Poprawki błędów kompatybilne wstecz

---

## Kategorie Zmian

- `Dodane` - Nowe funkcjonalności
- `Zmienione` - Zmiany w istniejących funkcjonalnościach
- `Usunięte` - Usunięte funkcjonalności
- `Naprawione` - Poprawki błędów
- `Bezpieczeństwo` - Poprawki bezpieczeństwa
