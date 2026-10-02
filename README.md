# Radar sonarowy ESP32-C6

Firmware dla płytki ESP32-C6-DevKitC-1. Radar obraca serwo, wykonuje pomiary odległości sonarem i pokazuje stan na ekranie OLED oraz matrycy LED. Buzzer sygnalizuje stan krytyczny. Opcjonalna obsługa Wi-Fi działa w trybie stacji.

## Wymagania

- Linux z PlatformIO Core
- ESP32-C6-DevKitC-1 do późniejszego testu sprzętowego
- Płytka nie jest potrzebna do kompilacji ani testów hostowych

## Kompilacja firmware'u

W katalogu repozytorium uruchom:

```bash
$HOME/.platformio/penv/bin/pio run
```

PlatformIO używa środowiska `esp32-c6-devkitc-1` z `platformio.ini`. Kompilacja tworzy pliki w katalogu `.pio/`.

## Testy hostowe

Testy nie wymagają ESP32 ani podłączonych czujników. Uruchom je w katalogu repozytorium:

```bash
gcc -std=c11 -Wall -Wextra -Werror -Isrc src/detector.c tests/test_detector.c -o /tmp/test_detector && /tmp/test_detector
```

```bash
gcc -std=c11 -Wall -Wextra -Werror -Isrc src/scan_control.c tests/test_scan_control.c -o /tmp/test_scan_control && /tmp/test_scan_control
```

GitHub Actions uruchamia oba zestawy testów oraz kompiluje firmware przy pushu na `main` i przy pull requestach.

## Wi-Fi

Bez lokalnego pliku z danymi Wi-Fi firmware pomija uruchomienie Wi-Fi, a lokalne funkcje radaru nadal działają.

Aby skonfigurować Wi-Fi na swoim komputerze:

1. Skopiuj `src/wifi_credentials.example.h` do `src/wifi_credentials.h`.
2. Wypełnij lokalnie nazwę sieci 2,4 GHz i jej hasło.
3. Skompiluj firmware ponownie.

Plik `src/wifi_credentials.h` jest ignorowany przez Git. Nie umieszczaj danych sieciowych w repozytorium, logach ani wiadomościach.

## Jak działa firmware

- `radar.c` steruje ruchem patrolowym serwa. Patrol obejmuje kąty od 15° do 165° i zatrzymuje się podczas blokady na wykrytym celu.
- `sonar.c` wykonuje pojedynczy pomiar, a `sonar_task.c` przekazuje wyniki dalej.
- `detector.c` wykrywa cel i alarm krytyczny na podstawie kolejnych pomiarów.
- `ui_task.c`, `oled.c`, `matrix.c` i `buzzer.c` obsługują ekran, matrycę i sygnał dźwiękowy.
- `network.c` obsługuje połączenie Wi-Fi oraz ponawianie połączenia.

## Test sprzętowy

Kompilacja i testy hostowe nie sprawdzają płytki, serwa ani czujników. Przed pierwszym testem sprzętowym należy osobno zweryfikować zasilanie, poziomy napięć i połączenia dla dokładnych modeli modułów. Plik `src/app_config.h` zawiera przypisania GPIO, ale sam w sobie nie jest schematem bezpiecznego okablowania.
