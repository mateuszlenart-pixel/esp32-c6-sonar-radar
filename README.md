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

```bash
gcc -std=c11 -Wall -Wextra -Werror -Isrc src/usb_cc.c tests/test_usb_cc.c -o /tmp/test_usb_cc && /tmp/test_usb_cc
```

GitHub Actions uruchamia wszystkie trzy zestawy testów oraz kompiluje firmware przy pushu na `main` i przy pull requestach.

## Buzzer

Płytka ma tranzystorowy stopień (Q2) i diodę (D2), więc pasuje zarówno buzzer pasywny, jak i aktywny
na 5 V. W `src/app_config.h` ustaw `BUZZER_ACTIVE`:

- `0` (domyślnie): buzzer pasywny, firmware generuje ton PWM i alarm krytyczny ma wyraźnie inne tony;
- `1`: buzzer aktywny, firmware tylko włącza i wyłącza, a różnice zostają w rytmie sygnału.

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
- `power_monitor.c` mierzy przez ADC napięcie szyny 5 V (GPIO0) oraz linie CC1/CC2 gniazda USB-C zasilania
  (GPIO1, GPIO3) i czyta flagę przeciążenia przełącznika U2 (GPIO23). Co 30 s oraz przy każdej zmianie
  zapisuje wynik w logu i ostrzega, gdy zasilacz podaje tylko 500/900 mA, szyna 5 V spadnie poniżej 4,5 V
  albo U2 ogranicza prąd. Klasyfikacja CC jest w `usb_cc.c` (testy hostowe). Działa na płytce PCB;
  na breadboardzie bez tych elementów odczyty są przypadkowe, a log tylko informacyjny.
- `network.c` obsługuje połączenie Wi-Fi oraz ponawianie połączenia.
- `zigbee_bridge.c` udostępnia czujniki i przełączniki przez Zigbee.
- `mqtt_bridge.c` publikuje stan radaru oraz każdy kolejny pomiar po MQTT.

## Home Assistant, Zigbee2MQTT i MQTT

ESP32-C6 działa jako urządzenie Zigbee typu End Device. Połączenie Wi-Fi i Zigbee
korzysta z tej samej anteny 2,4 GHz, więc End Device ogranicza ryzyko problemów
ze współpracą obu sieci. Koordynator SONOFF Dongle-PMG24 pozostaje podłączony do
Zigbee2MQTT; nie trzeba zmieniać jego firmware.

### Encje Zigbee

Plik `zigbee2mqtt/radar_converter.mjs` opisuje encje dla Zigbee2MQTT. Umieść go
w folderze `external_converters` obok `configuration.yaml` w katalogu danych
Zigbee2MQTT. W dodatku Home Assistanta domyślny katalog danych to
`/config/zigbee2mqtt`, więc pełna ścieżka pliku to
`/config/zigbee2mqtt/external_converters/radar_converter.mjs`.

W Zigbee2MQTT 2.11 lub nowszym włącz `advanced.enable_external_js`. Konwerter
można też dodać lub zmienić w interfejsie Zigbee2MQTT w **Settings → Dev console
→ External converters**. Po dodaniu pliku uruchom ponownie Zigbee2MQTT. Następnie
włącz dozwolone dołączanie urządzeń i uruchom radar. Po wykryciu radaru zaakceptuj
nowe urządzenie.

Zigbee udostępnia wykrycie celu, alarm krytyczny, odległość, kąt, dwa progi
odległości oraz przełączniki patrolu i buzzera. Próg celu można ustawić w zakresie
10–400 cm, a krytyczny w zakresie 5–300 cm. Próg krytyczny musi być mniejszy od
progu celu. Po dołączeniu Z2M konfiguruje raportowanie zmian.

### MQTT i Home Assistant

Skopiuj `src/wifi_credentials.example.h` do lokalnego pliku
`src/wifi_credentials.h`. W tym lokalnym pliku wpisz dane Wi-Fi i adres brokera
MQTT; jeśli broker wymaga logowania, wpisz je tam. Nie wysyłaj tych wartości na
czat i nie dodawaj lokalnego pliku do Git. MQTT Discovery automatycznie tworzy
encje w Home Assistant. Topic `esp32-c6-sonar-radar/<identyfikator>/state`
zawiera aktualny stan, a topic `.../scan` przekazuje kolejne pomiary z kątem,
odległością i informacją, czy pomiar był prawidłowy.

W tej wersji testowano kompilację i logikę programową. Dołączenie urządzenia,
raportowanie Zigbee, encje Home Assistant i fizyczne pomiary wymagają późniejszego
testu z płytką oraz działającym Zigbee2MQTT i brokerem MQTT.

## Test sprzętowy

Szczegółowy plan testów i szkic połączeń znajduje się w
[`docs/hardware/radar-wiring-and-test-plan.md`](docs/hardware/radar-wiring-and-test-plan.md).
Interaktywny model breadboardu (sieci, kontrola zwarć, edycja położenia elementów)
uruchomisz, otwierając w przeglądarce plik
[`docs/hardware/breadboard-model.html`](docs/hardware/breadboard-model.html).

Kompilacja i testy hostowe nie sprawdzają płytki, serwa ani czujników. Przed pierwszym testem sprzętowym należy osobno zweryfikować zasilanie, poziomy napięć i połączenia dla dokładnych modeli modułów. Plik `src/app_config.h` zawiera przypisania GPIO, ale sam w sobie nie jest schematem bezpiecznego okablowania.

### Poziomy napięć do sprawdzenia

- ESP32-C6 pracuje z GPIO w domenie 3,3 V. Jego wejście nie może przekroczyć 3,6 V ([datasheet Espressif, tabele 5-1 i 5-4](https://documentation.espressif.com/esp32-c6_datasheet_en.html)). Nie podłączaj bezpośrednio wyjścia ECHO typowego modułu HC-SR04 zasilanego z 5 V; obniż poziom sygnału przed GPIO. Dokładne wersje HC-SR04 mogą się różnić ([przykładowa dokumentacja modułu Elecfreaks](https://elecfreaks.com/download/EF03085-HC-SR04_Ultrasonic_Module_User_Guide.pdf)).
- Jeśli sam układ MAX7219 jest zasilany napięciem 5 V, jego datasheet wymaga co najmniej 3,5 V na wejściu logicznym. ESP32-C6 gwarantuje dla wyjścia logicznego wysokiego co najmniej 0,8 × 3,3 V = 2,64 V, więc bezpośrednia współpraca nie jest gwarantowana. Trzeba sprawdzić, czy konkretny moduł ma właściwy translator poziomów ([datasheet Analog Devices MAX7219, tabela na stronie 2](https://www.analog.com/media/en/technical-documentation/data-sheets/max7219-max7221.pdf)).

Te uwagi nie zastępują sprawdzenia oznaczeń i schematów dokładnych modułów. Nie podłączaj serwa do zasilania, dopóki jego napięcie i wydajność prądowa nie zostaną potwierdzone dla użytej płytki zasilającej.
