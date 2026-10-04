# Plan podłączenia i testowania radaru

Ten dokument przygotowuje bezpieczną kolejność pracy dla zestawu: ESP32-C6-DevKitC-1,
serwo SG90, HC-SR04, OLED SH1106 1,3 cala I2C, matryca MAX7219, buzzer pasywny
i system Zigbee2MQTT/Home Assistant.

## Co sprawdzono i co nadal zależy od konkretnej sztuki

- Espressif podaje GPIO4 i GPIO5 jako piny startowe (strapping). Firmware został
  przestawiony z GPIO4/5 na GPIO10/11 dla serwa i TRIG; pozostałe przypisania
  są wymienione niżej. GPIO10/11 są wyprowadzone na nagłówku DevKitC-1.
- ESP32-C6 pracuje z logiką 3,3 V. Datasheet podaje maksymalnie 3,6 V na wejściach
  oraz minimalny poziom logicznej jedynki równy 0,75 × VDD. Żaden sygnał 5 V nie
  może iść bezpośrednio na GPIO.
- Typowy HC-SR04 jest zasilany z 5 V i wystawia ECHO jako impuls 5 V. Proponowany
  dzielnik 10 kΩ/15 kΩ obniża impuls do ok. 3,0 V przy ECHO=5 V. Rezystor 10 kΩ
  łączy ECHO z wejściem węzła GPIO; 15 kΩ łączy ten węzeł z GND.
- MAX7219 zasilany z 5 V wymaga według karty katalogowej co najmniej 3,5 V na
  wejściu logicznym HIGH. Poziom 3,3 V z ESP32 nie jest gwarantowany. W szkicu
  dlatego jest bufor SN74AHCT125 zasilany z 5 V (AHCT rozpoznaje wejście 3,3 V
  jako HIGH i daje wyjście 5 V). Sam układ MAX7219 wymaga też rezystora RSET
  ustawiającego prąd LED; przed włączeniem trzeba potwierdzić, że breakout go ma.
- SG90 wymaga zasilania zewnętrznego. Dla serwa przyjmujemy regulowane 5 V,
  osobną od ESP32 gałąź zasilania i wspólną masę. Zasilacz 9 V/1 A z modułem
  Elegoo Power MB V2 nie jest przewidziany do zasilania serwa: dostępny arkusz
  MB-V2 podaje maksymalnie 700 mA. Rzeczywiste wymagania prądowe zależą od
  konkretnego serwa i obciążenia.
- Moduły SH1106 i MAX7219 występują w różnych wersjach. Bez zdjęcia nadruków
  na obu stronach konkretnego modułu nie można potwierdzić ich zasilania,
  rozmieszczenia pinów ani obecności rezystorów podciągających/elementów
  ochronnych. OLED w tym planie zasilamy z 3,3 V, nie z 5 V.
- Dla zakupionego zasilacza uniwersalnego instrukcja potwierdza polaryzację
  końcówki beczkowej: plus w środku, minus na zewnątrz. Nadal nie potwierdza to
  rzeczywistego napięcia ani prądu przy ustawieniu 5 V. Co ważniejsze, zdjęcia
  opakowania pasują do opisu modelu „Universal Adapter 30W / 12W” w raporcie
  bezpieczeństwa brytyjskiego OPSS nr 2207-0381. Raport opisuje ryzyko porażenia
  prądem z powodu dostępnych części pod napięciem i informuje o wycofaniu
  produktu ze sprzedaży. Dopóki nie potwierdzimy, że to inny produkt, nie używać
  ani nie podłączać tego adaptera do gniazdka. Do projektu wybrać inny,
  sprawdzony zasilacz 5 V o odpowiedniej wydajności prądowej.
- Kupiony zasilacz MW Power EB2005 ma stałe wyjście 5 V / 4 A / 20 W, wtyk
  5,5 × 2,1 mm i plus w środku. Karta katalogowa potwierdza 20 W; spotykany
  w tytule oferty zapis 24 W jest niespójny z 5 V × 4 A. Użyjemy go tylko do
  zasilania peryferiów, a ESP32 podczas programowania pozostanie zasilane z USB.
- Buzzer nie ma znanego numeru części. Dlatego jego końcowy obwód jest oznaczony
  jako do potwierdzenia: nie podłączamy nieznanego buzzera bezpośrednio do GPIO.

To jest szkic połączeń oparty na znanych modelach, nie potwierdzony testem
sprzętowym. Należy sprawdzić nadruki i napięcia przed podaniem zasilania.

## Potrzebne elementy do bezpiecznego prototypu

- **Kupione:** zasilacz MW Power EB2005, 5 V / 4 A / 20 W, wtyk 5,5 × 2,1 mm,
  plus w środku. Zasila zewnętrzną szynę peryferiów; nie podłączaj jego plusa
  do pinu 5V ESP32, gdy płytka jest zasilana z USB;
- **Kupione, dostawa w czwartek:** układ Texas Instruments SN74AHCT125N
  w obudowie DIP-14 i podstawka DIP-14. Układ zasilamy z tej samej szyny 5 V;
- kondensator ceramiczny 100 nF przy układzie SN74AHCT125N;
- rezystory 10 kΩ i 15 kΩ do dzielnika ECHO (zalecana tolerancja 5% lub lepsza);
- cztery rezystory 47 kΩ do utrzymania wejść AHCT w stanie niskim przy
  wyłączonym ESP32;
- gniazdo DC 5,5 × 2,1 mm z wyjściem śrubowym lub inny pewny rozdzielacz do
  przewodów; sprawdzić jego obciążalność i nie prowadzić prądu serwa przez
  delikatne styki breadboardu;
- multimetr; opcjonalnie oscyloskop lub analizator logiczny do obserwacji ECHO,
  spadków zasilania i zakłóceń;
- jumpery i breadboard o dobrym styku; przed włączeniem sprawdzić, że szyny
  zasilania na breadboardzie nie są przerwane w połowie.

EB2005 ma zapas względem wstępnego wymagania 5 V / 3 A. Rzeczywiste napięcie
i spadki podczas ruchu serwa sprawdzimy multimetrem przy testach; zasilacz
podłączymy przez właściwe gniazdo/rozdzielacz DC, nie bezpośrednio do GPIO.

## Przypisanie sygnałów

| Funkcja | Pin ESP32-C6-DevKitC-1 | Połączenie |
|---|---:|---|
| PWM serwa | GPIO10 | Sygnał serwa; zasilanie serwa osobno z 5 V |
| Sonar TRIG | GPIO11 | Przez kanał SN74AHCT125 do TRIG |
| Sonar ECHO | GPIO6 | Przez dzielnik 10 kΩ/15 kΩ do GPIO6 |
| Matryca DIN | GPIO7 | Przez kanał SN74AHCT125 |
| Matryca CLK | GPIO18 | Przez kanał SN74AHCT125 |
| Matryca CS/LOAD | GPIO19 | Przez kanał SN74AHCT125 |
| OLED SDA | GPIO20 | I2C, zasilanie OLED 3,3 V |
| OLED SCL | GPIO21 | I2C, zasilanie OLED 3,3 V |
| Buzzer | GPIO22 | Tylko przez odpowiedni stopień tranzystorowy po identyfikacji buzzera |

![Szkic sygnałów i zasilania](radar-wiring.svg)

### Szczegóły połączeń i zasilania

1. ESP32 zasilaj z USB-C podczas programowania. Nie podawaj zewnętrznego 5 V na
   pin 5V płytki, kiedy jest podłączona do USB. Instrukcja Espressifa opisuje te
   metody jako wzajemnie wykluczające się.
2. EB2005 zasila serwo, HC-SR04, moduł MAX7219 i SN74AHCT125; połącz wspólnie
   GND zasilacza i GND ESP32. Nie podłączaj plusa EB2005 do pinu 5V DevKitC-1,
   kiedy płytka jest zasilana z USB. Nie podłączaj EB2005 do wejścia Elegoo
   Power MB V2, które wymaga wyższego napięcia wejściowego.
3. OLED podłączamy do 3V3, GND, GPIO20/SDA i GPIO21/SCL. Zasilenie typowego
   modułu OLED napięciem 5 V może podciągnąć SDA/SCL do 5 V i uszkodzić ESP32.
   Trzeba potwierdzić, że moduł działa przy 3,3 V i ma właściwy interfejs I2C.
4. Dzielnik ECHO: HC-SR04 ECHO → rezystor 10 kΩ → węzeł → GPIO6; węzeł → rezystor
   15 kΩ → GND. Masa sonaru i ESP32 musi być wspólna. Nie pomijamy dzielnika.
5. Cztery kanały SN74AHCT125 mogą podać poprawną logikę 5 V na TRIG i trzy
   wejścia matrycy. Zasilanie układu: VCC=5 V, GND=wspólna masa, wszystkie cztery
   wejścia `/OE` podłącz do GND, żeby bufory były stale włączone. `/OE` jest
   aktywne stanem niskim, więc podciągnięcie go do plusa wyłączyłoby wyjście.
   Każde wejście A1–A4 połącz przez 47 kΩ do GND, aby nie wisiało w powietrzu,
   gdy ESP32 jest wyłączone. Umieść kondensator 100 nF blisko VCC/GND układu.
   Piny niewykorzystane, jeśli breakout różni się od wariantu 14-pinowego,
   podłącz zgodnie z kartą konkretnego układu.
6. Matryca MAX7219 dostaje 5 V i GND z zewnętrznej szyny. Jej DIN/CLK/CS dostają
   wyjścia bufora AHCT, nie GPIO bezpośrednio. Nie stosujemy L293D ani 74HC595
   jako konwerterów poziomów.
7. Serwo: przewód sygnałowy → GPIO10; czerwony/zasilanie → zewnętrzne 5 V;
   masa → wspólna GND. Kolory przewodów są typowe, ale należy sprawdzić oznaczenia
   i pinout własnego egzemplarza. Na początku zdejmujemy orczyk i nie obciążamy
   serwa mechanicznie.
8. Buzzer pozostaje odłączony do identyfikacji. Po identyfikacji dobieramy
   tranzystor i rezystor bazy/bramki. Dioda zabezpieczająca jest potrzebna przy
   buzzerze elektromagnetycznym/cewce, nie zakładamy jej bez ustalenia typu.

## Kolejność testów

### A. Przygotowanie i kontrola bez napięcia

1. Spisać rewizję DevKitC-1 oraz zrobić czytelne zdjęcia przodu i tyłu OLED,
   MAX7219, buzzera i Power MB V2. Sprawdzić nazwy i polaryzację złącz.
2. Nie używać sfotografowanego adaptera uniwersalnego ze względu na możliwe
   dopasowanie do ostrzeżenia OPSS. Użyj kupionego EB2005 jako osobnego zasilania
   peryferiów. Zasilacza 9 V/1 A nie podłączać do żadnego pinu ESP32 ani
   bezpośrednio do serwa.
3. Ustawić firmware na GPIO10/11 (zrobione w `src/app_config.h`) i skompilować
   poleceniem `$HOME/.platformio/penv/bin/pio run`. Przejść testy hostowe detektora
   i sterowania skanowaniem:

   ```sh
   gcc -std=c11 -Wall -Wextra -Werror -Isrc src/detector.c tests/test_detector.c -o /tmp/test_detector && /tmp/test_detector
   gcc -std=c11 -Wall -Wextra -Werror -Isrc src/scan_control.c tests/test_scan_control.c -o /tmp/test_scan_control && /tmp/test_scan_control
   ```
4. Przy wyłączonym zasilaniu zbudować tylko dzielnik ECHO i bufor AHCT. Omomierzem
   sprawdzić brak zwarcia między 5 V, 3,3 V i GND oraz poprawne połączenia mas.

### B. Testy modułów osobno

1. Włączyć sam ESP32 z USB. Sprawdzić log startowy i brak restartów.
2. Bez ESP32, włączyć wybrany zasilacz peryferiów bez obciążenia i multimetrem
   potwierdzić 5 V oraz polaryzację. Zasilacz wyłączyć.
3. Podłączyć OLED do 3,3 V/I2C. Uruchomić ekran i sprawdzić wykrycie adresu
   oczekiwanego przez kod (0x3C). W razie problemu sprawdzić nadruki, I2C/SPI
   jumpery i adres modułu.
4. Podłączyć HC-SR04: zasilanie 5 V, TRIG przez AHCT, ECHO przez dzielnik.
   Najpierw sprawdzić, że węzeł GPIO nie może osiągnąć 5 V; następnie wykonać
   serię pomiarów płaskiej, twardej przeszkody w znanej odległości. Nie używać
   tkaniny ani miękkiej powierzchni jako pierwszego celu.
5. Podłączyć matrycę MAX7219 przez AHCT. Sprawdzić obraz przy małej jasności,
   stabilne napięcie 5 V i brak przegrzewania modułu/bufora. Nie włączać układu
   MAX7219, jeśli nie potwierdzono obecności właściwego rezystora RSET.
6. Podłączyć serwo jako ostatnie z urządzeń mocy. Zostawić zdjęty orczyk, zacząć
   od środka i krótkich ruchów. Obserwować 5 V i logi ESP32 pod kątem brownoutów,
   resetów i drgań. Nie blokować mechanicznie serwa.
7. Dopiero po poznaniu modelu buzzera dołączyć tranzystorowy sterownik i sprawdzić
   go osobno.

### C. Test całego radaru

1. Przy kompletnym okablowaniu włącz najpierw zewnętrzne 5 V, a dopiero potem
   USB ESP32. Przed wyłączeniem odłącz najpierw USB, potem 5 V. Nie przepinaj
   przewodów przy włączonym zasilaniu.
2. Uruchomić patrol bez buzzera i sprawdzić ruch serwa w zakresie projektu
   (15–165°), prawidłowe wskazania OLED/matrycy i brak kolizji mechanicznej.
3. Zmierzyć kilka znanych odległości; sprawdzić brak celu, wykrycie celu,
   zanik celu i alarm krytyczny. Powtórzyć z serwem zatrzymanym, aby oddzielić
   błędy sonaru od drgań i hałasu zasilania.
4. Dopiero po stabilnym teście lokalnym uruchomić Wi-Fi i MQTT; sprawdzić status,
   scan, dostępność urządzenia, encje Home Assistant i polecenia przełączników.
5. Włączyć Zigbee2MQTT `permit_join`, uruchomić urządzenie, zaakceptować je,
   sprawdzić encje pomiarów, alarmów, progów, patrolu i buzzera, a następnie
   sterowanie w obie strony. Sprawdzić, że zły próg nie zostaje przyjęty.
6. Wykonać kilka restartów i dłuższy test pracy. Po każdym zmianie przewodów
   wyłączyć wszystkie zasilania i odłączyć USB.

Testy opisane powyżej nie zostały jeszcze wykonane na fizycznym sprzęcie.

## Schematy i karty katalogowe

Na Dellu pobrane kopie offline znajdują się w `docs/hardware/references/`:
schemat DevKitC-1 v1.4, datasheet ESP32-C6 v1.5, datasheet SN74AHCT125,
ogólny HC-SR04 oraz arkusz MB-V2 (kopia CircuitAttic). Te PDF-y są lokalnym
zbiorem referencyjnym i nie są dodawane do GitHub; poniżej są trwałe źródła online.

Dokumenty producenta i dodatkowe odniesienia online:

- [Instrukcja i schematy ESP32-C6-DevKitC-1 (wszystkie rewizje)](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32c6/esp32-c6-devkitc-1/user_guide.html)
- [Schemat DevKitC-1 v1.4, PDF Espressif](https://dl.espressif.com/dl/schematics/esp32-c6-devkitc-1-schematics_v1.4.pdf)
- [Karta katalogowa ESP32-C6 v1.5](https://documentation.espressif.com/esp32-c6_dataSheet_en.pdf)
- [TowerPro SG90, strona producenta](https://towerpro.com.tw/product/sg90-7/)
- [MAX7219/MAX7221, karta katalogowa Analog Devices](https://www.analog.com/media/en/technical-documentation/data-sheets/max7219-max7221.pdf)
- [SN74AHCT125, karta katalogowa Texas Instruments](https://www.ti.com/lit/ds/symlink/sn74ahct125.pdf)
- [HC-SR04, ogólna karta modułu](https://cdn.sparkfun.com/datasheets/Sensors/Proximity/HCSR04.pdf) — nie potwierdza konkretnego klona.
- [SH1106, przykładowy moduł Waveshare 1,3 cala](https://www.waveshare.com/wiki/1.3inch_OLED_%28A%29) — dokument dotyczy modułu Waveshare, nie potwierdza pinoutu ani zasilania posiadanego modułu.
- [MB-V2, arkusz danych kopii CircuitAttic](https://pgccphy.net/1020/datasheets/ELEGOO%20Breadboard%20Power%20Supply%20Module.pdf) — nie znaleziono aktualnej oficjalnej karty Elegoo; wartość 700 mA traktujemy zachowawczo jako granicę, nie jako gwarancję każdego klona.
- [MW Power EB2005, karta katalogowa](https://techniczne.mplpower.pl/repository/inne/techniczne/ZASILACZE/EB%20series/EB2005/EB2005%20ver.3/EB2005-spec-PL-R1.pdf) — podaje 5 V, 4 A, 20 W oraz parametry stabilizacji i zabezpieczeń.
- [Raport bezpieczeństwa OPSS 2207-0381, Universal Adapter 30W / 12W](https://assets.publishing.service.gov.uk/media/64c01f1590b54500143e82f2/2207-0381-product-safety-report-ac-dc-charger.pdf) — podobieństwo oznaczeń i opakowania wskazuje na możliwe dopasowanie; dokładny egzemplarz użytkownika nie został niezależnie zidentyfikowany.

Rewizja płytki oraz producent modułów poza DevKitC-1 nie są jeszcze potwierdzone.
