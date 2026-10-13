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
2. **Zasilanie wejściowe (zmiana 2026-10-11):** gniazdo **USB-C tylko do zasilania** (GCT USB4105,
   montaż maszynowy) zamiast gniazda DC. Rezystory 5,1 kΩ na CC1/CC2 (R16, R17) są konieczne, żeby
   ładowarka z kablem USB-C ↔ USB-C podała 5 V. Ochrona: TVS 5 V (D3, SMAJ5.0A, jednokierunkowy) na wejściu oraz przełącznik obciążenia z miękkim startem
   i ogranicznikiem prądu U2 (AP22804AW5, EN aktywny wysokim, EN połączone z VIN, RDS(on) 65 mΩ) zamiast
   bezpiecznika polimerowego (mniejszy spadek napięcia, miękki start i ograniczenie prądu przy ładowaniu ok. 900 µF; patrz uwagi do weryfikacji niżej); zabezpieczenie przed odwrotną polaryzacją (Q1) usunięte, bo USB-C nie da się
   podłączyć odwrotnie. Linie danych USB (D+/D−, SBU) niepodłączone. Wejście 5 V zasila serwo, HC-SR04,
   MAX7219, bufor i ESP32. Wymagana ładowarka ≥ 5 V / 1,5 A (szczyt poboru około 1–1,3 A); port USB
   komputera (500 mA) może nie wystarczyć.
3. **Pin 5V DevKita (J2 pin 14) jest zasilany z +5V płytki przez diodę Schottky D4** (kierunek płytka →
   DevKit). Poprawka po audycie 2026-10-11: DevKit ma własną diodę 1N5819 od swojego USB do pinu 5V,
   więc bezpośrednie połączenie z +5V pozwalałoby, żeby port USB komputera zasilał całą płytkę (serwo,
   matryca, sonar) i zasilał VBUS gniazda USB-C. Z diodą D4 USB DevKita zasila tylko DevKit
   (programowanie), a przy zasilaniu z USB-C płytki DevKit dostaje około 4,7 V przez D4.
4. **Moduły na złączach 2,54 mm** (gniazda żeńskie na PCB): OLED 1×4, MAX7219 1×5,
   HC-SR04 1×4, serwo 1×3. HC-SR04 i serwo na kablach, bo siedzą na obracanej głowicy.
5. **Bufor SN74AHCT125D w SMD (SOIC-14)** (decyzja po audycie 2026-10-11, montaż maszynowy), wszystkie 4
   kanały użyte (TRIG, DIN, CLK, CS), rezystory 47 kΩ na wejściach A: pull-down na TRIG, DIN i CLK,
   **pull-up do 3,3 V na CS** (w firmware CS spoczynkowo wysoki; na płytce testowej działało z pull-downem),
   100 nF przy VCC.
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
- **Płytka (szkic automatyczny, 2026-10-10):** `radar-pcb.kicad_pcb`, 66 × 128 mm, dwie warstwy,
  generowana skryptem `tools/gen_pcb.py` z netlisty schematu. Układ (widok od frontu): u góry
  złącza kabli do głowicy (sonar, serwo, buzzer, przycisk zewnętrzny), pod nimi gniazda OLED i
  MAX7219 (moduły wtykane od frontu), od tyłu poziomo DevKit (USB po lewej), gniazdo USB-C zasilania i przycisk
  ON/STANDBY na lewej krawędzi między rzędami pinów DevKita, bufor AHCT (SOIC) między rzędami DevKita. Trasowanie: Freerouting 2.5.0
  (jar + Temurin JRE 25, pobrane do `~/tools`, sumy kontrolne zweryfikowane), potem wylewki GND.
- **Stan DRC (2026-10-11, po audycie):** `kicad-cli pcb drc --schematic-parity`: **0 naruszeń
  (także silkscreen), 0 niepołączonych, 0 niezgodności ze schematem**. Freerouting: 0 nietrasowanych.
  Rozmieszczenie jest wrażliwe: przesunięcie części przy rzędach DevKita potrafi zepsuć trasowanie
  (ścieżki przechodzą między pinami rzędu J3), więc po każdej zmianie uruchamiać `build_all.sh`.
  Wszystkie elementy mają ręcznie zaplanowane pozycje (`FIXED` w `gen_pcb.py`), pas 0,9 mm przy
  krawędzi jest wolny od ścieżek, wylewki GND na obu warstwach.
- **Opisy na płytce (silkscreen):** przy każdym złączu nazwa modułu i nazwa każdego pinu (sonar,
  serwo, buzzer, przycisk, OLED, MAX7219), od tyłu opis wszystkich 32 pinów DevKita z kierunkiem
  USB-C, przy elementach oznaczenie z wartością (np. „R6 10k”, „C1 470uF 16V”), polaryzacja
  („+” przy elektrolitach, „K” przy katodach diod, „1” przy pinie 1 bufora), opisy punktów
  testowych, gniazda USB-C i przycisku ON/STANDBY. DRC: 0 naruszeń.
- **Przycisk ON/STANDBY:** kątowy (C&K PTS645VL58-2LFS, footprint `SW_Tactile_SPST_Angled_PTS645Vx58-2LFS`)
  na lewej krawędzi; popychacz wystaje ok. 3,3 mm poza płytkę, w osi 65,75 mm od górnej krawędzi
  (otwór w ściance obudowy). Złącze J9 zostaje na ewentualny przycisk panelowy.
- **Rysunek do weryfikacji:** `renders/radar-pcb-v0.1-wymiary-do-weryfikacji.pdf` (A4, strony 1-2
  w skali 1:1: przód i tył z wymiarami W1-W12 oznaczonymi „?”, strona 3: podgląd 3D). Wymiary
  są w skrypcie (`DIMS` w `gen_pcb.py`), legenda na warstwach Dwgs.User (przód) i Cmts.User (tył).
- **Ochrona przed zakłóceniami matrycy (2026-10-11, z testów na breadboardzie):** filtr RC zasilania
  sonaru (R11 10 Ω + C6 100 µF + C7 100 nF, sieć SR04_VCC na J4 pin 1), rezystory szeregowe
  100 Ω na DIN/CLK/CS do MAX7219 (R12–R14, sieci *_J na J6) i kondensatory przy module (C8 100 µF,
  C9 100 nF). Na breadboardzie przełączanie wierszy MAX7219 i transmisja do niej dawały fałszywe
  echa HC-SR04; firmware obchodzi to gaszeniem matrycy na czas pomiaru (widoczne lekkie mruganie).
  Te elementy mają pozwolić z tego obejścia zrezygnować; nie są jeszcze sprawdzone na płytce.
- **Serwo i buzzer (2026-10-11, analiza zakłóceń przed testem):** serwo SG90 pobiera impulsy prądu
  rzędu setek mA i jest zasilane z tej samej szyny 5 V co sonar, bufor i matryca. Dlatego +5V serwa
  idzie przez FB1 (miejsce na ferryt ≥ 1 A albo zworkę 0 Ω) do sieci SERVO_5V, z C4 220 µF i C10 100 nF
  przy złączu J5 (C4 przeniesiony z lewego pasa pod J5). R15 10 kΩ ciągnie sygnał serwa do masy,
  gdy GPIO10 jest nieustawione (reset, start). Buzzer: C11 100 nF na +5V przy sterowniku Q2/J8
  (przełączanie 2 kHz), dioda D2 już była; C12 100 nF równolegle do buzzera jest miejscem
  opcjonalnym. Nie sprawdzone na sprzęcie.
- **Zmiany po audycie (2026-10-11), wszystkie wdrożone:** D4 (punkt 3 wyżej); pod anteną DevKita
  (x ≥ 57, y 91–121) strefa bez miedzi na obu warstwach, punkty TP1/TP3 przeniesione; TVS jednokierunkowy;
  przełącznik obciążenia U2 zamiast F1 (spadek napięcia, miękki start; pełne odcięcie w STANDBY nie jest
  zrobione, bo ESP32 zasila się z tej samej szyny); FB1 domyślnie zworka 0 Ω; punkt testowy SERVO_5V
  (TP6); przycisk zewnętrzny: R18 1 kΩ szeregowo i R19 10 kΩ pull-up po stronie kabla (J9); pomiar
  napięcia 5 V dla firmware (R20/R21 → IO0, C13); złącze awaryjne J10 (RST, GND, BOOT = IO9: zwarcie
  sąsiednich pinów); miejsca R24/R25 4,7 kΩ na pull-upy I2C (nie montować); CS z pull-upem; bufor SOIC.
  Firmware jeszcze nie czyta ADC. Ścieżka VIN ma 0,5 mm (szersza nie mieści się przy padach USB-C 0,5 mm).
  Pomiary CC1 i CC2 przez ADC (R22/R23 100 kΩ, C14/C15 100 nF → IO1 i IO3) są wdrożone; elementy stoją
  tuż przy pinach 8 i 13 DevKita między rzędami (wcześniejsze ustawienie przy złączu USB-C nie dawało
  się poprowadzić).
- **Układ lewej krawędzi (2026-10-13, decyzja użytkownika):** oba gniazda USB-C (zasilania na froncie, programowania
  DevKita od tyłu) są po lewej stronie; gniazdo zasilania ma oś 100 mm od górnej krawędzi (W6), przycisk
  ON/STANDBY 113 mm (W7), oba między rzędami pinów DevKita (94–117 mm), czyli nisko na płytce. Na prawej
  krawędzi, w strefie anteny DevKita, nie ma już metalu ani miedzi. Przeniesiono za gniazdem: R16/R17 (CC),
  D3 (TVS), U2, C16, LED zasilania (D1/R1, wyżej po lewej), R18/R19/C5 (przycisk), punkty TP2/TP4 i dzielnik
  R20/R21/C13 (na dole). Na obudowie opisać, które gniazdo jest zasilające, a które do programowania.
  **Uwaga:** W6 i W7 są do zatwierdzenia po obejrzeniu PDF; DRC i trasowanie przechodzą (0 naruszeń).
  Generator usuwa ślepe przelotki po imporcie wyniku Freeroutingu (Freerouting zgłasza 3 nietrasowane
  połączenia, a KiCad po imporcie ma 0 niepołączonych; sprawdzone przez DRC).
- **DevKit (zmierzone 2026-10-13):** laminat 52,0 × 25,4 mm, z anteną 58,5 × 25,4 mm; od pinu 1 do końca
  płytki od strony anteny oraz od pinu 16 do końca od strony USB po ok. 7 mm (zgadza się z (52,0 − 38,1)/2 ≈ 6,95).
  Przy pinie 1 na x = 55 mm laminat sięga x ≈ 62, a antena modułu ok. 68,5 mm, czyli ok. 2,5 mm poza prawą
  krawędź płytki. Strefa bez miedzi (x ≥ 57, y 91–121) obejmuje antenę z zapasem, więc **nie zmienia się**.
  Koniec USB DevKita leży przy x ≈ 10 mm, ok. 10 mm od lewej krawędzi płytki.
- **Moduły (zmierzone 2026-10-13):** OLED SH1106 33,5 × 35,5 mm, piny 13,5 mm od krawędzi do osi pierwszego i
  ostatniego, kolejność VCC, GND, SCK, SDA od strony wyświetlacza; MAX7219 32 × 39 mm, piny 11 mm od krawędzi,
  piny wystają poza obrys na wymiarze 39 mm po ok. 6 mm na stronę; HC-SR04 VCC/TRIG/ECHO/GND i serwo SG90
  3-pin 2,54 mm potwierdzone. Gniazda J7 (OLED) i J6 (MAX7219) są wyśrodkowane na x = 33 mm, co zgadza się z pomiarami.
- **Drugi audyt (2026-10-11), wdrożone:** U2 i C16 stoją tuż przy gnieździe USB-C (VIN jest krótki,
  dalej idzie `+5V` ścieżką 0,6–0,8 mm; wcześniej VIN miał ok. 41 mm i spadek napięcia nie malał); LED
  zasilania (D1, R1) przeniesiona niżej; złącze J10 stoi między rzędami DevKita blisko pinów RST (J1 pin 2)
  i GPIO9 (J3 pin 11), więc linie EN i BOOT są krótkie (wcześniej 63 i 21 mm); FLG z U2 prowadzi do GPIO23
  (J3 pin 5, pull-up R26 10 kΩ do 3,3 V; niski = przeciążenie), firmware do dopisania.
  **J10:** kolejność BOOT, GND, RST. Wgrywanie awaryjne: zworka na BOOT–GND, odłączyć i podłączyć
  zasilanie, wgrać firmware, zdjąć zworkę i zrobić reset (zworka na GND–RST na chwilę). Jedna zworka
  obejmuje tylko jedną parę pinów naraz.
  **Otwór H4** (62,5; 124) leży ok. 8 mm od anteny; zalecane nylonowe śruby i tuleje (metal przy
  antenie obniża zasięg). Położenie otworów jest częścią kontraktu mechanicznego i nie zostało zmienione.
  **Uwagi dla firmware:** odczyt CC1/CC2 (GPIO1, GPIO3) dopiero po kilku sekundach od podłączenia
  (ładowarki PD wysyłają komunikaty po CC), mediana z kilku próbek, wewnętrzne pull-upy na tych pinach
  wyłączone. GPIO0 i GPIO1 to też piny kwarcu 32 kHz; na schemacie DevKitC-1 v1.4 kwarcu nie ma
  (sprawdzone wyszukiwaniem w tekście schematu), więc nie koliduje.
  **Przepięcie (świadomie bez zmiany):** TVS SMAJ5.0A ogranicza impulsy, ale przy trwałym nadnapięciu
  z wadliwej ładowarki (np. 9 V) spali się, a AP22804 nie ma ochrony nadnapięciowej. Zgodna ładowarka
  przy rezystorach 5,1 kΩ na CC nie poda więcej niż 5 V, więc ryzyko jest szczątkowe. Zamiana na eFuse
  z OVP (rodzina TI TPS2595x) wymaga potwierdzenia dostępności, footprintu i karty katalogowej, a
  symbolu nie ma w bibliotece KiCada. Opcja na kolejną rewizję.
  **Do sprawdzenia przed zamówieniem:** wariant AP22804**A**W5 (EN aktywny wysokim; potwierdzone
  wyszukiwaniem w sieci, nie w pełnym datasheecie Diodes); dopuszczalna pojemność wyjściowa i
  zachowanie przy przegrzaniu (za U2 jest ok. 900 µF, start trwa ok. 2 ms w limicie prądu 2,5–3 A);
  start z pełną pojemnością i ładowarką 1,5 A na prototypie. Limit prądu U2 jest wyższy niż 1,5 A
  ładowarek USB-C: chroni przed zwarciem, ale ładowarka ma własne zabezpieczenie.
- **Rendery i PDF:** `tools/export_docs.sh` (po `build_all.sh`).
- **Do sprawdzenia przed zamówieniem:** gniazdo USB-C GCT USB4105-xx-A (footprint z biblioteki KiCad) – potwierdzić
  dostępność u wykonawcy montażu; dopuszczalny odstęp 0,19 mm pól GND od otworów pozycjonujących jest
  z zaleceń producenta (ustawione w `.kicad_pro`); położenie gniazd modułów po pomiarach (rozstaw rzędów DevKita zmierzony: 23,1 mm).
- **Obrys 66 × 128 mm i położenie elementów to propozycja** do zatwierdzenia po zmierzeniu
  modułów i DevKita. **Zmierzone 2026-10-13 (suwmiarka 0,05 mm):** rozstaw rzędów DevKita 23,1 mm (W10;
  wcześniej zakładane 23,3), długość listwy 38,1 mm (W11), pin 1 11,0 mm od prawej krawędzi (W12), odstęp
  gniazd OLED–MAX7219 42,0 mm (W5). Otwory M3: H1–H4 tworzą prostokąt 59 × 92 mm, x = 3,5 i 62,5, y = 32 i 124.
- **Odtworzenie całości jednym poleceniem** (z katalogu repo): `hardware/pcb/tools/build_all.sh`
  (schemat, ERC, netlista, rozmieszczenie, Freerouting, wylewki GND, DRC; ok. 1,5 min).
- **Następne:** poprawić rozmieszczenie i trasowanie do DRC bez błędów, przegląd użytkownika w
  KiCadzie, kontrakt mechaniczny.

## Plan pracy

1. Zatwierdzenie założeń powyżej.
2. Kontrakt mechaniczny: ułożenie płytki w kolumnie 70 × 50 × 155 mm.
3. Schemat KiCad (skrypt generujący `.kicad_sch`, ERC bez błędów).
4. Netlista i płytka (`pcbnew` w Pythonie), rozmieszczenie, trasowanie, DRC.
5. Przegląd użytkownika w KiCadzie (podgląd 3D), poprawki.
6. Gerbery, BOM, CPL do JLCPCB lub PCBWay, zamówienie po teście prototypu.
