# Decyzje i uwagi do zamówienia (JLCPCB)

Stan: 2026-10-15. **Wersja przewlekana v0.2**, płytka 76 × 137,5 mm. Nic nie zamówione, nic nie sprawdzone na sprzęcie.

## Zmiana podejścia (2026-10-15)

Montaż SMD w JLCPCB wychodził zbyt drogo, więc **płytki zamawiamy bez montażu, a wszystkie elementy lutuje użytkownik**.
Wersja SMD z USB-C (HRO TYPE-C-31-M-12, AP22802, 0805) została zastąpiona wersją przewlekaną. Historia jest w gitcie
(PR #2–#6). Z tamtej wersji nie przechodzą: BOM i CPL dla JLCPCB, numery LCSC.

Co się zmieniło względem wersji SMD:
- **Zasilanie:** gniazdo DC 5,5/2,1 mm (środek +5 V), bezpiecznik PTC 2 A (hold) i dioda TVS SA5.0A (po bezpieczniku,
  zwiera przy przepięciu i odwróconej polaryzacji, PTC odcina). Zamiast przełącznika AP22802 (brak wersji przewlekanej).
  Nie ma miękkiego startu ani flagi przeciążenia. Zniknęły rezystory i pomiary CC (GPIO1, GPIO3, GPIO23 są wolne).
- **Elementy:** rezystory 1/4 W osiowe (7,62 mm), kondensatory ceramiczne radialne 5 mm, diody 1N4148 (DO-35),
  1N5819 (DO-41), TVS SA5.0A (DO-15), tranzystor 2N3904 (TO-92), bufor SN74AHCT125N w DIP-14 na podstawce, LED 3 mm.
- **Piny DevKita:** gniazda żeńskie J2/J3 od tyłu (bez zmian), a z przodu dwie listwy męskie J11 (kopia pinów J3 DevKita)
  i J12 (kopia pinów J1) z **wszystkimi 30 sygnałami** (pin 5V listwy J12 to szyna +5V płytki), listwa zasilania J13
  (GND, 3V3, 5V) i **pole prototypowe** 14 × 8 otworów 2,54 mm. Pin IO0 ma na płytce dzielnik pomiaru 5 V
  (R20/R21); nie montuj ich, jeśli chcesz mieć IO0 wolny.
- **Decyzje bez zmian:** moduły OLED (J7) i MAX7219 (J6) na kablach, HC-SR04 i serwo (J4, J5), buzzer (J8, aktywny lub
  pasywny, 5 V), przycisk ON/STANDBY (SW1) i zewnętrzny (J9), FB1 jako zworka 0 R, R24/R25 i C12 nie montować.

## Zamówienie w JLCPCB (same płytki)

Plik: `production/gerber-jlcpcb.zip` (wygeneruje go `tools/export_jlcpcb.sh`). Ustawienia w kreatorze: 2 warstwy, FR-4,
1,6 mm, wykończenie HASL (z ołowiem jest najłatwiejsze do lutowania ręcznego), kolor dowolny, ilość 5. **Montaż
(PCB Assembly) wyłączony.** Kreator powinien wykryć 76 × 137,5 mm.

Zgodność z możliwościami JLCPCB (źródło: strony Capabilities): ścieżki i odstępy min. 0,127 mm (u nas 0,3 / 0,2 mm),
przelotka 0,3 / 0,5 mm (u nas 0,35 / 0,7 mm), napisy na silkscreenie min. 1 mm i 0,15 mm kreski (u nas 1,0 / 0,17 mm).

## Lista zakupów

`production/lista-zakupow.csv` (wygeneruje ją `tools/hand_bom.py`): 33 pozycje do kupienia, osobno elementy
opcjonalne. Uwagi:
- **Gniazdo DC:** footprint `BarrelJack_Horizontal` (3 pady: 0 / −6 mm / −3 × 4,7 mm). Przed zamówieniem sprawdź w karcie
  katalogowej wybranego gniazda, że rozstaw pinów (środek, obudowa, styk przełączny) jest taki sam (pasuje CUI PJ-002A i zgodne).
  Wtyk zasilacza 5,5/2,1 mm, środek plus.
- **Przycisk SW1:** C&K PTS645VL58-2LFS lub zgodny kątowy (wciskany od strony krawędzi, popychacz wystaje przez ściankę obudowy).
- **PTC F1:** radialny 2 A hold, rozstaw 5 mm.
- **Gniazda DevKita J2/J3:** żeńskie 1×16, wysokie (ok. 8,5 mm), DevKit z lutowanymi goldpinami wchodzi w nie od tyłu płytki.
- **Podstawka DIP-14** i układ SN74AHCT125N (wersja DIP; SOIC nie pasuje).

## Do zrobienia przed zamówieniem

- Potwierdzić wymiary w PDF (`renders/radar-pcb-v0.1-wymiary-do-weryfikacji.pdf`): oznaczone „?” to W1, W2, W6–W9
  (zmienione położenie lub nowe), zmierzone to W10–W12.
- Wymiary obudowy (wysokość DevKita, okna OLED/MAX, HC-SR04, SG90, buzzer) czekają na pomiary użytkownika.
- Przetestować bloki na breadboardzie (MAX7219, serwo, buzzer). Płytka v0.2 nie była sprawdzana na sprzęcie.
