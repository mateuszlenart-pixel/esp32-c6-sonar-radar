# Decyzje i uwagi do zamówienia (JLCPCB)

Stan: 2026-10-14. Płytka kompaktowa 66 × 91 mm. **Nic nie zamówione**, płytka nie była sprawdzana na sprzęcie.

## Decyzje

- **Buzzer:** użytkownik ma aktywny i pasywny, bez modelu. Stopień Q2 (MMBT3904) + D2 (1N4148W) obsługuje oba,
  pod warunkiem że są na **5 V** i pobierają do ok. 50 mA. Domyślnie pasywny (`BUZZER_ACTIVE 0` w
  `src/app_config.h`): firmware gra różne tony. Aktywny działa po zmianie na `1`, ale ma jeden stały ton.
- **FB1:** footprint 0805 przyjmie zarówno zworkę 0 Ω, jak i ferryt, bez zmiany płytki. Na płytkach
  z montażem zamawiamy zworkę 0 Ω; ferryt (≥ 1 A, DCR do ok. 0,1 Ω) dolutowujemy ręcznie, jeśli w testach
  serwo będzie zakłócać sonar. Sprawdzić przy zamówieniu, czy ferryt wybrany z katalogu mieści się
  w pasie 0805 i ma prąd znamionowy ≥ 1 A (SG90 w szczycie około 0,7 A).
- **R24, R25:** nie montować (opcjonalne pull-upy I2C; moduł OLED ma własne).

## Decyzje z 2026-10-14

- **Zamówienie:** płytka z **zamontowanymi elementami SMD** (montaż JLCPCB), a gniazda DevKita J2/J3, listwy
  J4–J10, przycisk SW1 i kondensatory elektrolityczne C1, C4, C6, C8 lutuje użytkownik. Wszystkie elementy SMD
  są po przedniej stronie, THT jest po przedniej (listwy, SW1, kondensatory) i tylnej (gniazda DevKita).
- **U2 zmieniony na AP22802AW5-7 (LCSC C211404, SMT do montażu w JLCPCB).** AP22804AW5 nie ma w katalogu.
  Układ wyprowadzeń SOT-25 jest ten sam (1 OUT, 2 GND, 3 FLG, 4 EN aktywny wysokim, 5 IN), więc płytka się nie
  zmienia. Różnice (karta katalogowa Diodes): znamionowo 2 A zamiast 2,5 A (radar potrzebuje ok. 1–1,3 A),
  limit prądu 2,2–3,2 A (typ. 2,7 A), wolniejszy start (rise time typ. 2,1 ms przy 4,7 µF).
- **USB-C (zmiana 2026-10-14):** GCT USB4105-GF-A (C3020560) był w katalogu, ale kreator pokazał brak na stanie,
  więc J1 zmieniony na Korean Hroparts **TYPE-C-31-M-12 (LCSC C165948, Extended, 390 755 szt. na stanie)**,
  footprint `USB_C_Receptacle_HRO_TYPE-C-31-M-12` z biblioteki KiCad. Pady sygnałowe są 0,36 mm dalej od krawędzi,
  więc gniazdo przesunięto o 0,36 mm ku krawędzi (x = 2,74 mm), korpus wystaje 1,43 mm poza płytkę. Dwa odcinki
  obrysu silkscreenu przy krawędzi usunięte (DRC pokazuje przez to ostrzeżenie `lib_footprint_mismatch`). Strona JLCPCB oznacza montaż jako trudny („High”);
  w kreatorze sprawdzić, czy nie ma dopłaty ani ostrzeżenia o gabarycie lub kołkach.
- **Silkscreen:** wszystkie napisy mają wysokość **1,0 mm** i grubość kreski 0,17 mm, obrysy elementów min. 0,15 mm
  (wymóg JLCPCB). DRC: 0 naruszeń. Wartości C14, C15, R22, R23 (100 kΩ / 100 nF) i R24, R25 (4,7 kΩ, nie montować)
  są podane w osobnych opisach zamiast przy elementach (brak miejsca); oznaczenia „H1–H4” przy otworach ukryte.

## Zgodność z możliwościami JLCPCB (2 warstwy, źródło: strony Capabilities JLCPCB)

| Parametr | Wymaganie JLCPCB | Płytka |
|---|---|---|
| ścieżka / odstęp | min. 0,127 mm | min. ścieżka 0,15 mm, odstęp 0,2 mm: OK |
| przelotka | otwór min. 0,3 mm, średnica min. 0,5 mm | 0,35 / 0,7 mm: OK |
| napisy na silkscreenie | wysokość min. 1 mm, grubość min. 0,15 mm | wysokość 1,0 mm, grubość 0,17 mm: OK (poprawione 2026-10-14) |
| odstęp napisu od padu | min. 0,15 mm | DRC płytki: 0 naruszeń |

## Pliki produkcyjne (wygenerowane 2026-10-14)

`bash hardware/pcb/tools/export_jlcpcb.sh` (po `build_all.sh`) tworzy w `hardware/pcb/production/`:
`gerber-jlcpcb.zip` (Gerbery, wiercenia, 2 warstwy, początek współrzędnych w lewym dolnym rogu płytki),
`bom-jlcpcb.csv` (20 pozycji, 44 elementy SMD) i `cpl-jlcpcb.csv` (pozycje elementów z przodu).
Numery LCSC wpisane w `tools/jlcpcb_bom.py` i sprawdzone na stronach `jlcpcb.com/partdetail/<numer>`:

| Element | LCSC | Typ w bibliotece JLCPCB |
|---|---|---|
| R 1 k / 10 k / 47 k / 100 Ω / 330 Ω / 5,1 k / 15 k, 0805 1 % | C17513 / C17414 / C17713 / C17408 / C17630 / C27834 / C17475 | Basic |
| R 100 k 0805 1 % (R10, R20, R22, R23) | C96346 (Yageo RC0805FR-07100KL; C17407 był niedostępny) | Extended |
| R 10 Ω 0805 (R11) | C17415 | nie potwierdzono |
| zworka 0 Ω 0805 (FB1) | C17477 | Basic |
| C 100 nF 50 V X7R 0805 / 1 µF 50 V X7R 0805 | C49678 / C28323 | Basic |
| D1 LED czerwona 0805 | C84256 | Basic |
| D2 1N4148W | C81598 | Basic |
| D3 SMAJ5.0A (MDD) | C113952 | nie potwierdzono |
| D4 1N5819HW-7-F | C82544 | nie potwierdzono |
| Q2 MMBT3904 | C20526 | Basic |
| U1 SN74AHCT125DR | C155176 | nie potwierdzono |
| U2 AP22802AW5-7 | C211404 | nie potwierdzono |
| J1 TYPE-C-31-M-12 | C165948 | Extended |

Nie montowane (nie są w BOM): C12, R24, R25 (opcje), punkty testowe, gniazda, listwy, SW1, C1, C4, C6, C8.
Kolor LED zasilania nie był ustalony, wybrałem czerwony (zmiana = inny numer LCSC w `tools/jlcpcb_bom.py`).
Typ „Basic/Extended” decyduje o dopłacie za każdy unikalny element; stany magazynowe nie były sprawdzane.

## Do zrobienia przed zamówieniem

- W kreatorze JLCPCB wgrać `gerber-jlcpcb.zip`, potem BOM i CPL, i **obejrzeć podgląd rozmieszczenia**: obrót
  U1 (pin 1), U2, Q2, D2–D4, USB-C J1. JLCPCB potrafi obrócić część względem KiCada (inne definicje pinu 1),
  a CPL jest wygenerowany bez korekt rotacji. Poprawki robi się w kreatorze (przycisk obrotu), nie w plikach.
- Sprawdzić w kreatorze, czy U2 (C211404) i USB-C (C165948) są na stanie i czy nie ma dopłaty lub ostrzeżenia.
- Montaż tylko strony przedniej (wszystkie SMD są z przodu), grubość 1,6 mm, 2 warstwy.
- Opcja „numer zamówienia na płytce”: JLCPCB dodaje własny numer na silkscreenie, jeśli nie ma na płytce
  znacznika; można zamówić usunięcie za dopłatą lub zostawić.
- Nic nie było sprawdzone na sprzęcie. Przed zamówieniem zalecam przetestować bloki na breadboardzie
  (MAX7219, serwo, buzzer) i pomiary obudowy, gdy będą możliwe.
