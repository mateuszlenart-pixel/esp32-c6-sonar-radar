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

## Zgodność z możliwościami JLCPCB (2 warstwy, źródło: strony Capabilities JLCPCB)

| Parametr | Wymaganie JLCPCB | Płytka |
|---|---|---|
| ścieżka / odstęp | min. 0,127 mm | min. ścieżka 0,15 mm, odstęp 0,2 mm: OK |
| przelotka | otwór min. 0,3 mm, średnica min. 0,5 mm | 0,35 / 0,7 mm: OK |
| napisy na silkscreenie | wysokość min. 1 mm, grubość min. 0,15 mm (0,8 mm w trybie „high precision”) | wysokość **0,8 mm**, grubość 0,15 mm: poniżej standardu |
| odstęp napisu od padu | min. 0,15 mm | DRC płytki: 0 naruszeń |

## Co JLCPCB zmontuje, a co lutujemy sami

- Z przodu są elementy SMD (rezystory, kondensatory 0805, U1, U2, Q2, D1–D4, USB-C J1) oraz elementy
  przewlekane. Montaż maszynowy dotyczy tylko SMD.
- **Do lutowania ręcznego (THT):** gniazda DevKita J2/J3 (od tyłu), listwy J4–J10, przycisk SW1,
  cztery kondensatory elektrolityczne C1, C4, C6, C8 (przewlekane), punkty testowe są padami.
- Zamawiane części znane z katalogu: GCT USB4105-GF-A (C3020560), SN74AHCT125DR (C155176),
  SMAJ5.0A (kilku producentów). **AP22804AW5-7 (U2) nie znalazłem w katalogu JLCPCB/LCSC:** sprawdzić
  w kreatorze zamówienia; jeśli go nie ma, potrzebny jest zamiennik (np. inny przełącznik 5 V z limitem prądu
  i tym samym układem wyprowadzeń SOT-23-5, ze sprawdzeniem polaryzacji EN i flagi FLG) albo lutowanie ręczne.
