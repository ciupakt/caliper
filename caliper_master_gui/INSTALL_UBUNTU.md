# Instalacja aplikacji Caliper Master GUI na Ubuntu Linux

## Wymagania wstępne

- Ubuntu 18.04 lub nowszy
- Python 3.7 lub nowszy
- Dostęp do portów szeregowych (USB)

## Krok 1: Instalacja zależności systemowych

```bash
sudo apt update
sudo apt install python3 python3-pip python3-venv
sudo apt install fonts-dejavu-core fonts-dejavu-extra
sudo apt install libgl1-mesa-glx libxrandr2 libxinerama1 libxcursor1 libxi6  # zależności DearPyGui
```

## Krok 2: Dodanie użytkownika do grupy `dialout`

Aby uzyskać dostęp do portów szeregowych:

```bash
sudo usermod -a -G dialout $USER
# Wyloguj się i zaloguj ponownie lub uruchom: newgrp dialout
```

## Krok 3: Utworzenie środowiska wirtualnego i instalacja zależności Python

```bash
cd caliper_master_gui
python3 -m venv venv
source venv/bin/activate
pip install -r requirements.txt
```

## Krok 4: Uruchomienie aplikacji

```bash
python caliper_master_gui.py
```

## Rozwiązywanie problemów

### Problem: "Font file not found"

Jeśli aplikacja nie uruchamia się z powodu braku czcionek:

```bash
sudo apt install fonts-dejavu-core fonts-dejavu-extra
```

### Problem: "Permission denied" dla portów szeregowych

Upewnij się, że użytkownik jest w grupie `dialout`:

```bash
groups $USER
# Powinno zawierać 'dialout'
```

Jeśli nie, dodaj ponownie:

```bash
sudo usermod -a -G dialout $USER
newgrp dialout
```

### Problem: Brak DearPyGui

Upewnij się, że środowisko wirtualne jest aktywne:

```bash
source venv/bin/activate
pip list | grep dearpygui
```

Jeśli nie zainstalowane:

```bash
pip install dearpygui>=1.9.0
```

## Kompatybilność

Aplikacja została przetestowana na:
- Ubuntu 20.04 LTS
- Ubuntu 22.04 LTS

Wymaga DearPyGui >= 1.9.0 i Python >= 3.7.

## Dodatkowe informacje

- Aplikacja automatycznie wykrywa system operacyjny i używa odpowiednich czcionek
- Dla Windows używa Segoe UI
- Dla Linux używa DejaVu Sans
- Polskie znaki są obsługiwane poprzez rozszerzone zakresy czcionek