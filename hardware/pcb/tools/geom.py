"""Geometria płytki wersji przewlekanej: obrys, kotwice (elementy w stałych miejscach), strefy.

Współrzędne w mm od lewego górnego rogu płytki (widok od frontu), oś y w dół.
Używane przez autoplace.py (rozmieszczanie reszty elementów) i gen_pcb.py (budowa płytki).
"""
W = 76.0                      # szerokość płytki
ROW = 23.1                    # zmierzony rozstaw rzędów pinów DevKita
XR = W - 11.0                 # pin 1 DevKita (zmierzone: 11,0 mm od prawej krawędzi)
A_BOTTOM = 62.0               # dolna granica strefy A (swobodne rozmieszczanie nad rzędem listwy J11)
Y3 = A_BOTTOM + 11.6          # oś rzędu J3 DevKita
Y2 = Y3 + ROW                 # oś rzędu J2 DevKita
YC = (Y3 + Y2) / 2            # środek pasa DevKita
PROTO_Y0 = Y2 + 5.08 + 12.0    # pierwszy rząd pola prototypowego
H = round(PROTO_Y0 + 3 * 5.08 + 8.5, 1)   # wysokość płytki (z miejscem na logo w prawym dolnym rogu)
HOLES = [(3.5, 24.0), (W - 3.5, 24.0), (3.5, H - 4.0), (W - 3.5, H - 4.0)]
LOGO = (46.5, PROTO_Y0 + 7.5, 69.5, H - 2.0)   # miejsce na logo (silkscreen) w prawym dolnym rogu: x0, y0, x1, y1
ANT = (W - 9.0, Y3 - 3.0, W, Y2 + 4.0)    # strefa anteny DevKita: bez miedzi (x0, y0, x1, y1)

# ref -> (x, y, obrót, strona, (rozszerzenie prostokąta o opisy: lewo, góra, prawo, dół))
ANCHORS = {
    'J4': (6.0, 4.0, 90, 'F', (2.5, 1.0, 0.5, 10.0)),
    'J5': (22.0, 4.0, 90, 'F', (2.5, 1.0, 0.5, 10.0)),
    'J8': (36.0, 4.0, 90, 'F', (2.5, 1.0, 0.5, 10.0)),
    'J9': (46.0, 4.0, 90, 'F', (2.5, 1.0, 0.5, 10.0)),
    'J7': (32.0, 16.0, 90, 'F', (2.5, 0.5, 0.5, 14.5)),
    'J6': (32.0, 38.0, 90, 'F', (2.5, 0.5, 0.5, 11.0)),
    'J1': (13.0, YC - 0.95, 0, 'F', (0, 2.8, 0, 0)),
    'SW1': (2.64, YC + 13.5, 90, 'F', (0, 0, 0, 0)),
    'J3': (XR, Y3, 90, 'B', (0, 0, 0, 0)),
    'J2': (XR, Y2, 90, 'B', (0, 0, 0, 0)),
    'J11': (XR, Y3 - 5.08, 270, 'F', (0, 5.0, 0, 0)),
    'J12': (XR, Y2 + 5.08, 270, 'F', (0, 0, 0, 5.0)),
    'J13': (52.0, PROTO_Y0, 90, 'F', (0, 0, 0, 0)),
}
for _k in range(4):
    ANCHORS['J%d' % (14 + _k)] = (11.0, PROTO_Y0 + _k * 5.08, 90, 'F', (0, 0, 0, 0))

# strefy swobodnego rozmieszczania (x0, y0, x1, y1)
REGION_A = (1.5, 1.5, W - 1.5, A_BOTTOM)
REGION_B = (17.5, Y3 + 2.3, W - 10.0, Y2 - 2.3)
