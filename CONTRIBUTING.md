# Współpraca z Projektem Caliper

Dziękujemy za zainteresowanie współpracą z projektem Caliper! Ten dokument zawiera wskazówki dotyczące zgłaszania błędów, proponowania zmian i przesyłania pull requestów.

## Zasady Współpracy

### Kod Postępowania

1. **Bądź uprzejmy i szanuj innych** - Traktuj innych z szacunkiem i cierpliwością.
2. **Bądź konstruktywny** - Skupiaj się na tym, co jest najlepsze dla społeczności.
3. **Bądź otwartym umysłem** - Bądź otwarty na różne punkty widzenia i akceptuj krytykę.

### Standardy Kodu

#### C++ (ESP32)

- Używaj stylu kodowania zgodnego z [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html)
- Komentarze w języku polskim lub angielskim
- Dokumentacja funkcji w formacie Doxygen:
  ```cpp
  /**
   * @brief Krótki opis funkcji
   * @param param_name Opis parametru
   * @return Opis wartości zwracanej
   */
  ```
- Nazewnictwo zmiennych w camelCase
- Nazewnictwo klas w PascalCase
- Stałe w UPPER_SNAKE_CASE

#### Python

- Używaj stylu kodowania zgodnego z [PEP 8](https://www.python.org/dev/peps/pep-0008/)
- Komentarze w języku polskim lub angielskim
- Dokumentacja funkcji w formacie docstring:
  ```python
  def function_name(param1, param2):
      """
      Krótki opis funkcji
      
      Args:
          param1: Opis parametru 1
          param2: Opis parametru 2
      
      Returns:
          Opis wartości zwracanej
      """
      pass
  ```
- Nazewnictwo zmiennych i funkcji w snake_case
- Nazewnictwo klas w PascalCase
- Stałe w UPPER_SNAKE_CASE

## Proces Rozwoju

### 1. Fork i Klonowanie

1. Forknij repozytorium na GitHub
2. Sklonuj swoje fork:
   ```bash
   git clone https://github.com/TWOJE_USERNAME/caliper.git
   cd caliper
   ```

### 2. Tworzenie Brancha

Utwórz nowy branch dla swojej zmiany:
```bash
git checkout -b feature/nazwa-funkcjonalnosci
# lub
git checkout -b fix/opis-bledu
```

### 3. Wprowadzanie Zmian

1. Zrób zmiany w kodzie
2. Dodaj testy (jeśli dotyczy)
3. Zaktualizuj dokumentację (jeśli dotyczy)
4. Zatwierdź zmiany:
   ```bash
   git add .
   git commit -m "Krótki opis zmian"
   ```

### 4. Kompilacja i Testowanie

#### ESP32 (Master/Slave)
```bash
cd caliper_master
pio run --environment esp32doit-devkit-v1

cd ../caliper_slave
pio run --environment esp32doit-devkit-v1
```

#### Python GUI
```bash
cd caliper_master_gui
python -m pytest tests/
python caliper_master_gui_new.py
```

### 5. Push i Pull Request

1. Wypchnij zmiany do swojego forka:
   ```bash
   git push origin feature/nazwa-funkcjonalnosci
   ```

2. Utwórz Pull Request na GitHub
   - Użyj jasnego tytułu
   - Opisz zmiany w opisie
   - Oznacz powiązane issues

## Zgłaszanie Błędów

### Przed zgłoszeniem

1. **Szukaj istniejących issues** - Sprawdź, czy błąd nie został już zgłoszony.
2. **Użyj szablonu issue** - Wypełnij wszystkie wymagane pola.
3. **Podaj szczegóły** - Im więcej informacji, tym łatwiej nam będzie pomóc.

### Szablon Issue

```markdown
## Opis
Krótki opis problemu.

## Kroki do reprodukcji
1. Krok 1
2. Krok 2
3. ...

## Oczekiwane zachowanie
Opisz, co powinno się stać.

## Rzeczywiste zachowanie
Opisz, co się dzieje.

## Środowisko
- System operacyjny: [np. Windows 11, Ubuntu 22.04]
- Wersja PlatformIO: [np. 6.1.11]
- Wersja Python: [np. 3.11]
- Wersja DearPyGUI: [np. 1.9.0]

## Dodatkowe informacje
Logi, zrzuty ekranu, itp.
```

## Proponowanie Zmian

### Przed propozycją

1. **Dyskutuj** - Otwórz issue lub dyskusję przed rozpoczęciem pracy.
2. **Zrozum problem** - Upewnij się, że rozumiesz, co i dlaczego chcesz zmienić.
3. **Zacznij od małych zmian** - Duże zmiany są trudniejsze do zrecenzowania.

### Szablon Propozycji

```markdown
## Opis
Krótki opis proponowanej zmiany.

## Dlaczego?
Dlaczego ta zmiana jest potrzebna?

## Jak?
Jak zamierzasz zaimplementować tę zmianę?

## Alternatywy
Czy rozważałeś inne rozwiązania?

## Dodatkowe informacje
Linki do dyskusji, itp.
```

## Struktura Projektu

```
caliper/
├── caliper_master/          # Firmware Master ESP32
├── caliper_slave/           # Firmware Slave ESP32
├── caliper_master_gui/      # Aplikacja GUI Python
├── lib/                    # Współdzielone biblioteki
│   └── CaliperShared/
├── doc/                    # Dokumentacja
│   ├── architecture.md
│   ├── api/
│   │   └── protocol.md
│   └── hardware/
├── plans/                  # Plany rozwoju
│   └── refactoring-plan.md
├── AGENTS.md               # Instrukcje dla AI
├── README.md               # Główna dokumentacja
├── CHANGELOG.md            # Historia zmian
└── CONTRIBUTING.md         # Ten dokument
```

## Testowanie

### Testy Jednostkowe

- Python: Użyj `unittest` lub `pytest`
- C++: Użyj `Unity` lub `Catch2`

### Testy Integracyjne

- Testuj komunikację ESP-NOW
- Testuj komunikację Serial
- Testuj interfejs webowy

### Testy Manualne

1. Wgraj firmware na urządzenia
2. Uruchom aplikację GUI
3. Przetestuj wszystkie funkcjonalności
4. Sprawdź dokumentację

## Dokumentacja

- Aktualizuj `README.md` przy zmianach funkcjonalności
- Aktualizuj `CHANGELOG.md` przy wydaniach
- Dodawaj komentarze do kodu
- Używaj formatu Doxygen dla C++
- Używaj formatu docstring dla Python

## Wydania

### Proces Wydania

1. Zaktualizuj wersję w `CHANGELOG.md`
2. Utwórz tag na GitHub:
   ```bash
   git tag -a v1.0.0 -m "Wydanie v1.0.0"
   git push origin v1.0.0
   ```
3. Utwórz Release na GitHub z notatkami wydania

### Wersjonowanie

Używaj [Semantic Versioning](https://semver.org/):
- **MAJOR**: Zmiany niekompatybilne wstecz
- **MINOR**: Nowe funkcjonalności kompatybilne wstecz
- **PATCH**: Poprawki błędów kompatybilne wstecz

## Licencja

Przez przesyłanie pull requesta zgadzasz się na udzielenie licencji na swoje zmiany zgodnie z licencją projektu.

## Kontakt

- **Issues**: [GitHub Issues](https://github.com/TWOJE_USERNAME/caliper/issues)
- **Discussions**: [GitHub Discussions](https://github.com/TWOJE_USERNAME/caliper/discussions)

---

Dziękujemy za wkład w projekt Caliper!
