# Projekt PCB i obudowy radaru

Dwa podprojekty rozwijane równolegle. Dokumentacja prototypu na breadboardzie
(stan sprzętu, pomiary) jest w `docs/hardware/`; tutaj są projekty docelowe.

| Katalog | Zawartość | Narzędzie |
|---|---|---|
| `pcb/` | projekt płytki drukowanej | KiCad 10 (Flatpak `org.kicad.KiCad`) |
| `enclosure/` | koncepcja obudowy, eksporty STL/OBJ | Blender 5.2 (Flatpak `org.blender.Blender`), szczegóły w Autodesk Inventor |

## Środowisko pracy (Dell Wyse 5070, Linux Mint 22.3)

- KiCad 10.0.7 i Blender 5.2.0 LTS zainstalowane przez Flatpak (2026-10-09).
- CLI KiCada: `flatpak run --command=kicad-cli org.kicad.KiCad <polecenie>`
  (ERC, DRC, eksport Gerberów, BOM).
- Blender bez okna: `flatpak run org.blender.Blender --background --python <skrypt>`.
- Dell ma słabą grafikę. Ciężkie podglądy 3D otwieramy na laptopie (Asus) z tego
  samego repo.

## Kolejność prac

0. Przygotowanie środowiska (KiCad, Blender) — zrobione.
1. Specyfikacja sygnałów i zasilania (poniżej).
2. Decyzje architektury (poniżej, „Do rozstrzygnięcia”).
3. Kontrakt mechaniczny: obrys PCB, otwory, położenie złączy i USB, okno OLED,
   miejsce na serwo z HC-SR04. Wspólny dla `pcb/` i `enclosure/`.
4. Schemat w KiCadzie, ERC.
5. Layout, DRC.
6. Koncepcja obudowy w Blenderze, szczegóły w Inventorze, dopasowanie do modelu 3D PCB.
7. Gerbery, BOM, CPL, przegląd, zamówienie 5 sztuk, wydruk próbny obudowy.

Warunek przed krokiem 7: prototyp na breadboardzie działa sprzętowo (test
zwarcia, pomiar HC-SR04, kolejne moduły). Do tego czasu nic nie zamawiamy.

## Specyfikacja sygnałów (źródło: `src/app_config.h`)

| Sygnał | GPIO | Kierunek | Poziom na płytce | Uwagi |
|---|---|---|---|---|
| Serwo PWM | 10 | wyjście | 3,3 V | zasilanie serwa z 5 V zewnętrznego |
| HC-SR04 TRIG | 11 | wyjście | 3,3 V → bufor AHCT → 5 V | SN74AHCT125 |
| HC-SR04 ECHO | 6 | wejście | 5 V → dzielnik 10 kΩ/15 kΩ → ok. 3,1 V | ponad 3,3 V = ryzyko dla ESP32 |
| MAX7219 DIN | 7 | wyjście | 3,3 V → bufor AHCT → 5 V | |
| MAX7219 CLK | 18 | wyjście | 3,3 V → bufor AHCT → 5 V | |
| MAX7219 CS | 19 | wyjście | 3,3 V → bufor AHCT → 5 V | |
| OLED SDA | 20 | I2C | 3,3 V | adres 0x3C, 100 kHz, SH1106 1,3" |
| OLED SCL | 21 | I2C | 3,3 V | |
| Buzzer | 22 | wyjście PWM | 3,3 V → tranzystor | tylko przez stopień tranzystorowy |

Unikamy GPIO4/GPIO5 (piny strapping w ESP32-C6-DevKitC-1).

## Zasilanie

- 5 V z zewnętrznego zasilacza (EB2005, 5 V/4 A): serwo, HC-SR04, MAX7219, bufor AHCT.
- ESP32 z USB-C. Nigdy nie łączyć +5 V zasilacza z pinem 5V DevKita przy podłączonym USB.
- Wspólna masa zasilacza i ESP32.
- OLED z 3,3 V ESP32.

## Decyzje

- **ESP32 (2026-10-09, user):** DevKitC-1 w gniazdach żeńskich na PCB, nie goły moduł.
- **MAX7219 (2026-10-09, ze zdjęć):** gotowy moduł Elegoo „MAX7219-DotMatrix-Module-V02”
  z matrycą 8×8, pięć pinów wejścia (VCC, GND, DIN, CS, CLK) i pięć pinów wyjścia
  (VCC, GND, DOUT, CS, CLK) po przeciwnych krawędziach, cztery otwory montażowe
  w rogach. Na PCB zostaje tylko złącze 1×5 wejścia (piny wyjścia nieużywane).
  Obecność RSET na module nie jest potwierdzona (spód nie pokazuje elementów,
  zwykle leży pod matrycą); do sprawdzenia przed pierwszym zasilaniem.

- **Obudowa (2026-10-09, user):** stoi na biurku, opcjonalnie wisi na ścianie
  (jeśli nie komplikuje projektu, w przeciwnym razie tylko biurko). OLED i matryca
  oba widoczne, układ względem sonaru do ustalenia na wizualizacjach. USB-C
  dostępne z zewnątrz. Druk 3D zamawiany jako usługa zewnętrzna, wykonawca i
  wymagania (materiał, tolerancje, rozmiar) dopiero pod koniec projektu.

- **Wariant obudowy (2026-10-09, user):** D, pionowa kolumna (sonar na górze, pod nim
  OLED i matryca), kształt opływowy zamiast kostki, osobna podstawka, w którą kolumna
  jest wsuwana (stabilność na biurku). Zdjęcie podstawki umożliwia powieszenie samej
  kolumny na ścianie. Koncepcja: `enclosure/concepts/wariant-D2.py`
  (rendery odtwarzalne skryptem). Wymiary orientacyjne, do zastąpienia zmierzonymi.
- **Koncepcja obudowy: ZAPARKOWANA (2026-10-09, user zadowolony).** Aktualna wersja
  to `enclosure/concepts/wariant-D3*` (ścięte krawędzie, dwa kolory, głowica z
  sonarem; render Cycles ok. 4 min na CPU Della, F12 w Blenderze na Asusie).
  Kształt, wymiary kolumny (70×50×155 mm) i podstawki (120×110×28 mm) są
  orientacyjne. Szczegóły (podział na połówki, wnętrze, montaż PCB i wyświetlaczy)
  dopiero po zmierzeniu modułów i w Inventorze. Skrypty i pliki `.blend`
  wariantów A–D2 zostają w `enclosure/concepts/` (rendery odtwarzalne); w repo trzymamy
  tylko render zatwierdzony przez użytkownika: `wariant-D3-front.png`.

## Do rozstrzygnięcia przed schematem

1. Zasilanie na PCB: gniazdo DC lub złącze śrubowe; zabezpieczenie przed odwrotną polaryzacją i bezpiecznik.
2. Moduły (OLED, HC-SR04, serwo, MAX7219): złącza goldpin/JST, czy lutowane bezpośrednio.
3. Buzzer: typ nieustalony (pasywny tak/nie, prąd), wymagany do doboru tranzystora.
4. Wymiary modułów i ich mocowanie (potrzebne zmierzone wymiary, zob. kontrakt mechaniczny).
5. Kontrakt mechaniczny: obrys PCB, otwory, położenie złączy, USB, okno OLED.
