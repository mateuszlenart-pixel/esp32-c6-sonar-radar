# Lista pomiarów do kontraktu mechanicznego

Suwmiarka, milimetry, jedna cyfra po przecinku. Zdjęcie z linijką lub suwmiarką
przy każdym module pomaga, ale liczby podawaj tekstem. Oznaczenia: **W** =
wymiar potrzebny do obrysu/obudowy, **E** = potrzebny do schematu/elektroniki.

## 1. ESP32-C6-DevKitC-1 (płytka dotarła? dostawa 2026-10-10)
- [ ] W długość i szerokość płytki
- [ ] W wysokość całości (z wlutowanymi listwami, od spodu pinów do góry USB)
- [ ] W położenie gniazda USB-C (od krawędzi, szerokość i wysokość wtyku)
- [ ] W długość części antenowej bez miedzi (strefa, gdzie nic nie może leżeć)
- [ ] E rozstaw listew 23,9 mm (zmierzony), długość listwy 38 mm (zmierzona) — potwierdzić na nowej płytce
- [ ] E położenie przycisków BOOT/RESET i diody

## 2. MAX7219 (Elegoo V02)
- [ ] W długość i szerokość płytki
- [ ] W rozstaw środków otworów montażowych (poziomo i pionowo) i średnica otworu
- [ ] W wysokość z matrycą, patrząc z boku
- [ ] W wymiar i położenie okna matrycy (część świecąca)
- [ ] E odległość pinów wejścia od krawędzi, rozstaw 2,54 mm (potwierdzić)

## 3. OLED SH1106 1,3" I2C
- [ ] W długość i szerokość płytki, wysokość całości (opis sprzedawcy: 33,5 × 35,5 mm, do weryfikacji)
- [ ] W wymiar i położenie okna ekranu (opis sprzedawcy: 29,42 × 14,7 mm, do weryfikacji) (część aktywna) względem krawędzi płytki
- [ ] W rozstaw otworów montażowych (jeśli są) i średnica
- [ ] E kolejność i opis pinów (zdjęcie spodu), napięcie zasilania z opisu/zdjęcia

## 4. HC-SR04
- [ ] W długość i szerokość płytki, wysokość z przetwornikami
- [ ] W średnica przetworników i odległość między ich środkami
- [ ] W jak głęboko przetworniki wystają przed płytkę
- [ ] E położenie i kolejność 4 pinów (VCC/TRIG/ECHO/GND)

## 5. Serwo SG90
- [ ] W korpus: długość, szerokość, wysokość
- [ ] W uszy montażowe: rozstaw otworów i średnica, odległość od osi wału
- [ ] W oś wału: odległość od krawędzi korpusu, wysokość wału nad korpusem
- [ ] W orczyk (jeśli użyty): średnica i wysokość
- [ ] E długość kabla, typ wtyczki (3-pin, rozstaw 2,54 mm?)

## 6. Mocowanie sonaru na serwie
- [ ] W jak HC-SR04 jest/będzie przymocowany do orczyka (uchwyt, klej, własny element?)
- [ ] W zakres ruchu 15–165°: ile miejsca musi mieć sonar przy obrocie (promień obrotu)

## 7. Buzzer
- [ ] E typ: opis ze zdjęcia, plus/minus, aktywny czy pasywny (pasywny: bez dźwięku po podaniu stałego 5 V)
- [ ] W średnica i wysokość, rozstaw nóżek

## 8. Zasilanie i złącza
- [ ] W gniazdo USB-C zasilania (po zmianie z DC; dotyczy też obudowy): wymiary korpusu, rozstaw pinów, odległość otworu od krawędzi
- [ ] W wtyk zasilacza EB2005 (kabel, wtyk 5,5×2,5): długość wtyku, średnica kabla (przejście w obudowie)
- [ ] E jeśli będzie bezpiecznik/szybkozłącze: ich wymiary

## 9. Obudowa (decyzje, nie pomiary)
- [ ] gdzie stoi radar (biurko, półka, ściana)? orientacja okna sonaru
- [ ] czy OLED i matryca mają być widoczne z przodu?
- [ ] jak dostępne ma być USB-C (programowanie) po zamknięciu obudowy?
- [ ] druk 3D: własny czy zamówiony? maksymalny rozmiar druku

Liczba pozycji do zmierzenia: 6 modułów/podzespołów (ESP32, MAX7219, OLED,
HC-SR04, SG90, buzzer) plus złącza zasilania, razem ok. 35 punktów.
