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
- **USB-C:** obecny GCT USB4105-GF-A (LCSC C3020560, footprint `USB4105-xx-A_16P_TopMnt_Horizontal`) jest w katalogu
  JLCPCB i nadaje się do montażu, więc **bez zamiennika**. Strona JLCPCB oznacza montaż jako trudny („High”);
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

## Do zrobienia przed zamówieniem

- Wygenerować Gerbery, BOM i CPL (nie zrobione) i sprawdzić je w kreatorze JLCPCB (numery LCSC, rotacje części).
- Numery LCSC znane z katalogu: GCT USB4105-GF-A (C3020560), AP22802AW5-7 (C211404), SN74AHCT125DR (C155176),
  SMAJ5.0A (kilku producentów). Pozostałe części (rezystory i kondensatory 0805, MMBT3904, 1N4148W, 1N5819HW,
  LED 0805, ferryt/zworka FB1) dopiero dobrać; sprawdzić, które są „Basic” (bez dopłaty za element).
- Zamówić z montażem tylko strony przedniej i sprawdzić podgląd rozmieszczenia w kreatorze, w szczególności
  obrót USB-C, U1 (SOIC-14, pin 1), U2 i Q2.
