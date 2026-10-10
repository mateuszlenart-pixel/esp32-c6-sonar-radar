# Płytka PCB radaru (KiCad 10)

Status: **start projektu 2026-10-10.** Zamówienie dopiero po sprawdzeniu na breadboardzie
wszystkich bloków i po kontrakcie mechanicznym (zob. `../README.md`).

## Narzędzia (Dell, Flatpak `org.kicad.KiCad` 10.0.7)

- Biblioteki symboli i footprintów: `/app/extensions/Library/{symbols,footprints,3dmodels}`
  wewnątrz Flatpaka (`flatpak run --command=sh org.kicad.KiCad -c '...'`).
- Python z modułem `pcbnew` (10.0.7) w tym samym Flatpaku: skrypty tworzą i sprawdzają płytkę.
- `kicad-cli`: ERC, DRC, eksport Gerberów, BOM, podgląd 3D.
- Brak Javy, więc bez Freerouting; trasowanie ręczne/skryptem (płytka jest prosta).

## Bloki obwodu (z breadboardu, stan na 2026-10-10)

| Blok | Stan na breadboardzie |
|---|---|
| Zasilanie 5 V z EB2005 | zmierzone (5,13 V) |
| HC-SR04: TRIG przez SN74AHCT125, ECHO przez dzielnik 10 kΩ/15 kΩ | działa, ±1 cm |
| OLED SH1106, I2C, 3,3 V z ESP32 | działa |
| MAX7219 przez kanały 2–4 AHCT125 | zaparkowane, plan gotowy |
| Serwo SG90, 5 V | nie podłączone |
| Buzzer (tranzystor) | nie podłączone, typ nieustalony |

## Domyślne założenia projektowe (DO ZATWIERDZENIA przez użytkownika)

1. **ESP32-C6-DevKitC-1 w gniazdach żeńskich** (decyzja użytkownika z 2026-10-09).
   Rozstaw rzędów do potwierdzenia na posiadanej płytce przed layoutem.
2. **Zasilanie wejściowe:** gniazdo DC 5,5 × 2,5 mm na płytce (pasuje do wtyku EB2005),
   zabezpieczenie przed odwrotną polaryzacją (dioda Schottky lub P-MOSFET) i bezpiecznik
   polimerowy. Wejście 5 V zasila serwo, HC-SR04, MAX7219 i bufor. ESP32 nadal z USB-C.
3. **Nie łączymy 5 V płytki z pinem 5V DevKita** (jak w prototypie); zasilanie ESP32 z
   płytki tylko jeśli świadomie to wybierzemy (do decyzji).
4. **Moduły na złączach 2,54 mm** (gniazda żeńskie na PCB): OLED 1×4, MAX7219 1×5,
   HC-SR04 1×4, serwo 1×3. HC-SR04 i serwo na kablach, bo siedzą na obracanej głowicy.
5. **Bufor SN74AHCT125 w SMD (SOIC-14) lub DIP-14** (do decyzji), wszystkie 4 kanały użyte
   (TRIG, DIN, CLK, CS), rezystory pull-down na wszystkich wejściach A, 100 nF przy VCC.
6. **Dzielnik ECHO 10 kΩ/15 kΩ** na płytce, zgodnie z prototypem.
7. **Buzzer:** tranzystor NPN lub MOSFET z rezystorem bazy i diodą (dla cewki), złącze
   2-pinowe na zewnętrzny buzzer. Typ buzzera do identyfikacji.
8. **Kondensatory odsprzęgające i bulk** na szynie 5 V (np. 470 µF przy serwie i MAX7219).
9. **Punkty testowe** na 5 V, 3V3, GND, TRIG, ECHO (węzeł GPIO6).
10. **Wyłącznik ON/standby:** przycisk chwilowy (tact) na GPIO2 do GND (wewnętrzny pull-up ESP32, opcjonalnie kondensator 100 nF do GND), do tego 2-pinowe złącze na zewnętrzny przycisk w obudowie. To „miękki” wyłącznik: ESP32 zostaje włączone (inaczej nie zadziała Zigbee), a serwo, pomiary, ekrany i buzzer są wyłączone. Opcjonalnie dodatkowo twardy przełącznik w linii 5 V (do decyzji).
11. **Obrys i otwory montażowe:** dopiero z kontraktu mechanicznego (kolumna obudowy
    70 × 50 mm, wnętrze mniejsze o ścianki).

## Stan prac (2026-10-10)

- **Schemat:** `radar-pcb.kicad_sch` (A3), generowany skryptem `tools/gen_schematic.py`
  (połączenia przez etykiety globalne, każdy pin ma sieć albo znacznik NC). 36 elementów,
  41 sieci. ERC (`kicad-cli sch erc`): **0 błędów**, 6 ostrzeżeń `lib_symbol_mismatch`
  dla U1 (osadzona kopia 74AHCT125 jest spłaszczona, bez wpływu na połączenia).
- **Pinout DevKita:** `devkit-pinout.md` (odczytany ze schematu Espressif; poprawka: GND jest
  też na J3 pin 12 i 15).
- **Następne:** kontrakt mechaniczny (obrys), płytka w `pcbnew`, rozmieszczenie, trasowanie, DRC.

## Plan pracy

1. Zatwierdzenie założeń powyżej.
2. Kontrakt mechaniczny: ułożenie płytki w kolumnie 70 × 50 × 155 mm.
3. Schemat KiCad (skrypt generujący `.kicad_sch`, ERC bez błędów).
4. Netlista i płytka (`pcbnew` w Pythonie), rozmieszczenie, trasowanie, DRC.
5. Przegląd użytkownika w KiCadzie (podgląd 3D), poprawki.
6. Gerbery, BOM, CPL do JLCPCB lub PCBWay, zamówienie po teście prototypu.
