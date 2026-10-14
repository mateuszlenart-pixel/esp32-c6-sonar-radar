"""Buduje płytkę radar-pcb.kicad_pcb z netlisty schematu (KiCad 10, Python pcbnew).

Kroki: wczytanie netlisty XML -> footprinty z bibliotek KiCada -> rozmieszczenie ->
obrys i otwory -> eksport DSN -> (zewnętrznie) Freerouting -> import SES -> wylewki GND.

Użycie (z katalogu repo):
  flatpak run --command=python3 --filesystem=host org.kicad.KiCad hardware/pcb/tools/gen_pcb.py place
  ~/tools/freerouting/freerouting-cli -de hardware/pcb/radar-pcb.dsn -do hardware/pcb/radar-pcb.ses -mp 30
  flatpak run --command=python3 --filesystem=host org.kicad.KiCad hardware/pcb/tools/gen_pcb.py finish
"""
import os
import sys
import xml.etree.ElementTree as ET

import pcbnew

HERE = os.path.dirname(os.path.abspath(__file__))
PCB_DIR = os.path.normpath(os.path.join(HERE, '..'))
NETLIST = os.path.join(PCB_DIR, 'radar-pcb.net')
PCB = os.path.join(PCB_DIR, 'radar-pcb.kicad_pcb')
DSN = os.path.join(PCB_DIR, 'radar-pcb.dsn')
SES = os.path.join(PCB_DIR, 'radar-pcb.ses')
FP_ROOT = '/app/extensions/Library/footprints/'

DY = -37.0                  # wariant kompaktowy: moduły OLED i MAX7219 na kablach, środkowy pas płytki usunięty
W, H = 66.0, 128.0 + DY     # obrys płytki, mm (propozycja do zatwierdzenia)
OX, OY = 100.0, 25.0        # przesunięcie na arkuszu KiCada (A4 poziomo, płytka nad tabelką)
mm = pcbnew.FromMM
SILK_H, SILK_T = 1.0, 0.17   # napisy na silkscreenie: JLCPCB wymaga wysokości >= 1 mm i grubości kreski >= 0,15 mm
SILK_LINE_MIN = 0.15        # minimalna grubość linii obrysu elementów na silkscreenie


def P(x, y):
    return pcbnew.VECTOR2I(mm(OX + x), mm(OY + y))


def load_netlist():
    root = ET.parse(NETLIST).getroot()
    comps = {}
    for c in root.find('components'):
        comps[c.get('ref')] = {'value': c.findtext('value'), 'fp': c.findtext('footprint'),
                               'uuid': (c.findtext('tstamps') or '').split()[0]}
    nets = {}
    for n in root.find('nets'):
        nets[n.get('name')] = [(nd.get('ref'), nd.get('pin')) for nd in n.findall('node')]
    return comps, nets


def load_fp(libname):
    lib, name = libname.split(':')
    fp = pcbnew.FootprintLoad(FP_ROOT + lib + '.pretty', name)
    fp.SetFPID(pcbnew.LIB_ID(lib, name))
    return fp


# Pozycje wszystkich elementów: ref -> (x, y, obrót, strona); obrót CCW na ekranie, y w dół,
# widok od frontu płytki. Moduły OLED i MAX7219 wtykane od frontu zajmują x 15-51 (pod nimi
# tylko niskie elementy SMD), DevKit od tyłu zajmuje pas y 92-120.
ROW_SPACING = 23.1   # zmierzony rozstaw rzędów pinów DevKita (2026-10-13)
FIXED = {
    # góra: złącza kabli do głowicy (sonar, serwo), buzzer, przycisk zewnętrzny
    'J4': (6, 4, 90, 'F'), 'J5': (20, 4, 90, 'F'), 'J8': (34, 4, 90, 'F'), 'J9': (42, 4, 90, 'F'),
    # sterownik buzzera (pas między złączami a gniazdem OLED)
    'Q2': (47, 9, 0, 'F'), 'R9': (51.5, 8, 0, 'F'), 'R10': (51.5, 10.8, 0, 'F'), 'D2': (57, 9, 0, 'F'),
    # moduły wtykane od frontu (nagłówki u góry modułów)
    'J7': (33 - 3.81, 13, 90, 'F'), 'J6': (33 - 5.08, 55, 90, 'F'),
    # lewy pas: kondensator serwa, przycisk ON/standby
    'C4': (21, 20.5, 0, 'F'), 'FB1': (23.5, 12.4, 0, 'F'), 'C10': (14.5, 19.5, 90, 'F'),
    'R15': (29.2, 7.0, 0, 'F'), 'C11': (44.5, 16, 0, 'F'), 'C12': (51, 16, 0, 'F'), 'SW1': (2.64, 115.25, 90, 'F'), 'C5': (10.2, 115.8, 0, 'F'),
    # prawy pas: gniazdo USB-C zasilania (na krawędzi), CC, TVS, bezpiecznik, kondensatory, LED
    'J1': (3.1, 100, 270, 'F'), 'R16': (10.8, 98.8, 0, 'F'), 'R17': (10.8, 102.2, 0, 'F'), 'D3': (14.4, 105.5, 90, 'F'), 'D1': (7.5, 86.5, 90, 'F'), 'R1': (13.5, 86.9, 0, 'F'),
    'C1': (55.5, 80, 0, 'F'), 'C2': (63, 80, 90, 'F'), 'U2': (13.2, 95.2, 0, 'F'), 'C16': (9.0, 92.6, 0, 'F'), 'R26': (48.5, 77.5, 0, 'F'),
    'TP1': (49.5, 89.5, 0, 'F'), 'TP3': (46, 89.5, 0, 'F'),
    'D4': (25, 121.8, 0, 'F'), 'R18': (10.2, 110.0, 0, 'F'), 'R19': (10.2, 113.0, 0, 'F'), 'J10': (41, 111.5, 90, 'F'),
    'R20': (9.5, 125.3, 0, 'F'), 'R21': (13.5, 125.3, 0, 'F'), 'C13': (17.5, 125.3, 0, 'F'),
    'R24': (46, 121.8, 0, 'F'),
    'R25': (50.5, 121.8, 0, 'F'), 'R22': (37.2, 113.6, 90, 'F'), 'C14': (34.3, 113.6, 90, 'F'),
    'R23': (24.8, 112.2, 90, 'F'), 'C15': (28.0, 114.0, 90, 'F'), 'TP6': (13, 26, 0, 'F'),
    # DevKit od tyłu (poziomo, USB po lewej w widoku od frontu)
    'J3': (55, 94, 90, 'B'), 'J2': (55, 94 + ROW_SPACING, 90, 'B'),
    # bufor SOIC-14 między rzędami DevKita, pull-downy przy wejściach, odsprzęganie
    'U1': (28.6, 104.2, 90, 'F'), 'C3': (17.5, 104, 90, 'F'),
    'R2': (20.2, 110.5, 0, 'F'), 'R3': (31.2, 112, 0, 'F'), 'R4': (33.7, 97.4, 0, 'F'), 'R5': (26.1, 97.4, 0, 'F'),
    # dzielnik ECHO i punkt testowy węzła GPIO6
    'R6': (42, 104, 90, 'F'), 'R7': (45.5, 104, 90, 'F'), 'TP5': (49, 104, 0, 'F'),
    # pozostałe punkty testowe, rezystor serwa
    'TP2': (12.5, 121.0, 0, 'F'), 'TP4': (18.7, 121.0, 0, 'F'), 'R8': (30, 122, 0, 'F'),
    # filtr RC zasilania sonaru przy J4 (lewy górny róg)
    'R11': (16.5, 14, 0, 'F'), 'C6': (4.5, 17.5, 0, 'F'), 'C7': (11, 18, 90, 'F'),
    # rezystory szeregowe DIN/CS/CLK nad gniazdem MAX7219, kondensatory obok modułu
    'R12': (27, 51, 0, 'F'), 'R14': (35.5, 51, 0, 'F'), 'R13': (44, 51, 0, 'F'),
    'C8': (52.2, 55, 0, 'F'), 'C9': (61.5, 55, 90, 'F'),
}
# Wariant kompaktowy: wszystko od y >= 70 (zasilanie, DevKit, bufor) przesunięte w górę o DY,
# gniazdo kabla MAX7219 (J6) i jego elementy tuż pod gniazdem kabla OLED (J7).
for _ref, (_x, _y, _r, _s) in list(FIXED.items()):
    if _y >= 70:
        FIXED[_ref] = (_x, _y + DY, _r, _s)
FIXED.update({
    'J6': (33 - 5.08, 36, 90, 'F'),
    'R12': (25, 32, 0, 'F'), 'R14': (32, 32, 0, 'F'), 'R13': (39, 32, 0, 'F'),
    'C8': (48, 35, 0, 'F'), 'C9': (55, 33, 90, 'F'),
})
HY = 30.0   # wiersz górnych otworów montażowych
HOLES = [(3.5, HY), (W - 3.5, HY), (3.5, H - 4), (W - 3.5, H - 4)]


def place_fixed(board, fp, ref):
    x, y, rot, side = FIXED[ref]
    fp.SetPosition(P(x, y))
    fp.SetOrientationDegrees(rot)
    if side == 'B':
        fp.Flip(P(x, y), pcbnew.FLIP_DIRECTION_LEFT_RIGHT)


def pad_xy(fp, pin):
    for p in fp.Pads():
        if p.GetNumber() == str(pin):
            v = p.GetPosition()
            return (pcbnew.ToMM(v.x) - OX, pcbnew.ToMM(v.y) - OY)
    return None


def pack_small(parts, rects):
    """Prosty algorytm półkowy: układa małe elementy w wolnych prostokątach."""
    placed = {}
    for ref, (w, h) in parts:
        done = False
        for r in rects:
            x0, y0, x1, y1, cx, cy, rowh = r['x0'], r['y0'], r['x1'], r['y1'], r['cx'], r['cy'], r['rowh']
            if cx + w > x1:
                cx = x0
                cy += rowh + 0.8
                rowh = 0
            if cy + h <= y1:
                placed[ref] = (cx + w / 2, cy + h / 2)
                r['cx'] = cx + w + 0.8
                r['cy'] = cy
                r['rowh'] = max(rowh, h)
                done = True
                break
            r['cx'], r['cy'], r['rowh'] = cx, cy, rowh
        if not done:
            raise RuntimeError('brak miejsca dla ' + ref)
    return placed


# ------------------------------------------------------------------ opisy na płytce
PIN_NAMES = {
    'J4': ('HC-SR04', ['VCC', 'TRIG', 'ECHO', 'GND']),
    'J5': ('SERWO', ['GND', '+5V', 'SIG']),
    'J8': ('BUZZ', ['+', '-']),
    'J9': ('PRZYC.', ['IO2', 'GND']),
    'J10': ('J10', ['IO9', 'GND', 'RST']),
    'J7': ('OLED SH1106', ['VCC', 'GND', 'SCK', 'SDA']),
    'J6': ('MAX7219', ['VCC', 'GND', 'DIN', 'CS', 'CLK']),
}
DEVKIT = {
    'J3': ('DevKit J3', ['GND', 'TX', 'RX', 'IO15', 'IO23', 'IO22', 'IO21', 'IO20', 'IO19', 'IO18', 'IO9',
                         'GND', 'IO13', 'IO12', 'GND', 'NC']),
    'J2': ('DevKit J1', ['3V3', 'RST', 'IO4', 'IO5', 'IO6', 'IO7', 'IO0', 'IO1', 'IO8', 'IO10', 'IO11',
                         'IO2', 'IO3', '5V', 'GND', 'NC']),
}
# oznaczenie + wartość na opisie: ref -> (tekst, dx, dy, wyrównanie, obrót) względem środka elementu.
# Własne oznaczenie elementu (pole Reference) jest ukrywane, żeby napisy się nie dublowały.
VALUES = {
    'R1': ('R1 1k', 0, -1.7, 'center', 0), 'D1': ('D1 PWR', 0, -2.4, 'center', 0),
    'C1': ('C1 470uF 16V', 1.75, 5.4, 'center', 0), 'C2': ('C2 100nF', 1.9, 0, 'center', 90),
    'U2': ('AP22802', 0.8, -3.0, 'center', 0), 'R26': ('R26 10k', 0, 1.7, 'center', 0), 'C16': ('C16 1uF', -1.0, -1.6, 'center', 0),
    'R16': ('5k1', 2.0, 0, 'left', 0), 'R17': ('5k1', 2.0, -0.5, 'left', 0), 'D3': ('D3 TVS', -2.7, 0, 'center', 90),
    'D4': ('D4 5V->DevKit', 0, 2.2, 'center', 0), 'R18': ('R18 1k', 2.0, 0, 'left', 0), 'R19': ('R19 10k', 2.0, 0, 'left', 0),
    'R20': ('R20 100k', -1.0, 1.7, 'center', 0), 'R21': ('R21 47k', 0, -1.5, 'center', 0), 'C13': ('C13 100n', 0.5, 1.7, 'center', 0),
    'R22': ('R22', 0.5, -2.4, 'center', 0), 'R23': ('R23', -2.0, 0, 'right', 0),
    'C14': ('C14', 0, -2.2, 'center', 0), 'C15': ('C15', 1.8, 0, 'left', 0), 'R24': ('R24', 0, -1.7, 'center', 0), 'R25': ('R25', 0, -1.7, 'center', 0),
    'C4': ('C4 220uF 10V', -0.5, 4.6, 'center', 0), 'FB1': ('FB1 0R', 0, 1.7, 'center', 0),
    'C10': ('C10 100n', -1.2, 3.2, 'center', 0), 'R15': ('R15 10k', -2.0, 2.0, 'center', 0),
    'C11': ('C11 100nF', 0, 1.7, 'center', 0), 'C12': ('C12 100n opc.', 0, -1.7, 'center', 0), 'SW1': ('SW1 STBY', 0, 5.0, 'left', 0),
    'C5': ('100n', 1.9, 0, 'left', 0),
    'Q2': ('Q2 3904', -2.2, 3.0, 'center', 0), 'R9': ('R9 1k', 0, -1.7, 'center', 0),
    'R10': ('R10 100k', -0.5, 2.1, 'left', 0), 'D2': ('D2 1N4148', -0.6, 2.3, 'left', 0),
    'C3': ('C3 100n', 2.5, -2.5, 'center', 0),
    'R2': ('R2 47k', 0, -1.7, 'center', 0), 'R3': ('R3 47k', -1.0, -1.7, 'center', 0),
    'R4': ('R4 47k', 1.6, 0, 'left', 0), 'R5': ('R5 47k', -1.6, 0, 'right', 0),
    'R6': ('R6 10k', 0, -2.6, 'center', 0), 'R7': ('R7 15k', 0, 2.5, 'center', 0),
    'R8': ('R8 330', 1.6, 0, 'left', 0), 'U1': ('U1 74AHCT125', 2.5, 4.4, 'center', 0),
    'J1': ('J1 USB-C', -1.4, 6.7, 'left', 0),
    'R11': ('R11 10R', 1.4, 1.7, 'center', 0), 'C6': ('C6 100uF', 1.25, 4.4, 'center', 0),
    'C7': ('C7 100nF', 1.9, 0, 'center', 90),
    'R12': ('R12 100R', 0, -1.7, 'center', 0), 'R14': ('R14 100R', 0, -3.0, 'center', 0),
    'R13': ('R13 100R', 0, -1.7, 'center', 0), 'C8': ('C8 100uF', 1.25, -4.3, 'center', 0),
    'C9': ('C9 100nF', 1.9, 0, 'center', 90),
}
FREE_TEXT = [
    # (tekst, x, y, wysokość, obrót, warstwa)
    ('+5V', 49.5, 87.6, 0.8, 0, 'F'), ('GND', 46, 87.6, 0.8, 0, 'F'), ('SERVO 5V', 13, 27.8, 0.8, 0, 'F'),
    ('3V3', 12.5, 119.3, 0.8, 0, 'F'), ('TRIG', 18.7, 119.3, 0.8, 0, 'F'), ('ECHO', 50.9, 104, 0.8, 0, 'F', 'left'),
    ('DevKit od tylu  <- USB-C', 30, 90.2, 0.9, 0, 'F'),
    ('R24, R25 4k7: nie montowac', 48, 124.6, 1.0, 0, 'F'),
    ('R22,R23 100k  C14,C15 100n', 33, 119.4, 1.0, 0, 'F'),
    ('Radar C6 v0.1 compact', 36, 126.4, 0.9, 0, 'F'),
    ('USB-C ->', 30, 126.6, 1.0, 0, 'B'),
    ('DevKit J3 (pin 1 = GND)', 36, 86.2, 0.9, 0, 'B'),
    ('DevKit J1 (pin 1 = 3V3)', 36, 124.6, 0.9, 0, 'B'),
    ('Radar ESP32-C6 - tyl', 33, 66, 1.2, 0, 'B'),
]

FREE_TEXT = [(t[0], t[1], t[2] + DY if t[2] >= 60 else t[2]) + tuple(t[3:]) for t in FREE_TEXT]


def add_text(board, s, x, y, h=0.8, angle=0, side='F', just='center'):
    t = pcbnew.PCB_TEXT(board)
    t.SetText(s)
    t.SetLayer(pcbnew.F_SilkS if side == 'F' else pcbnew.B_SilkS)
    h = max(h, SILK_H)
    t.SetTextSize(pcbnew.VECTOR2I(mm(h), mm(h)))
    t.SetTextThickness(mm(SILK_T))
    t.SetTextAngleDegrees(angle)
    t.SetHorizJustify({'center': pcbnew.GR_TEXT_H_ALIGN_CENTER, 'left': pcbnew.GR_TEXT_H_ALIGN_LEFT,
                       'right': pcbnew.GR_TEXT_H_ALIGN_RIGHT}[just])
    if side == 'B':
        t.SetMirrored(True)
    t.SetPosition(P(x, y))
    board.Add(t)


def add_labels(board, fps):
    # złącza na froncie: nazwa pinu pionowo pod każdym pinem, nazwa modułu po lewej
    for ref, (title, names) in PIN_NAMES.items():
        fp = fps[ref]
        for i, n in enumerate(names):
            x, y = pad_xy(fp, i + 1)
            add_text(board, n, x, y + 1.5, 0.8, 90, 'F', 'right')
        x1, y1 = pad_xy(fp, 1)
        add_text(board, title, x1 - 1.8, y1 + 1.5, 0.8, 90, 'F', 'right')
    # DevKit od tyłu: nazwy pinów nad J3 i pod J2 (warstwa B, lustrzane)
    for ref, (title, names) in DEVKIT.items():
        fp = fps[ref]
        above = ref == 'J3'
        for i, n in enumerate(names):
            x, y = pad_xy(fp, i + 1)
            if above:
                add_text(board, n, x, y - 1.5, 0.8, 90, 'B', 'right')
            else:
                add_text(board, n, x, y + 1.5, 0.8, 90, 'B', 'left')
    # wartości elementów
    for ref, (txt, dx, dy, just, ang) in VALUES.items():
        fps[ref].Reference().SetVisible(False)
        c = fps[ref].GetPosition()
        add_text(board, txt, pcbnew.ToMM(c.x) - OX + dx, pcbnew.ToMM(c.y) - OY + dy, 0.8, ang, 'F', just)
    # polaryzacja: + przy kondensatorach elektrolitycznych, K przy katodach, 1 przy pinie 1 U1
    for ref in ('C1', 'C4', 'C6', 'C8'):
        x, y = pad_xy(fps[ref], 1)
        add_text(board, '+', x - (3.6 if ref == 'C1' else 2.6), y - (1.4 if ref == 'C1' else 2.4), 1.2, 0, 'F')
    for ref in ('D1', 'D2'):
        x, y = pad_xy(fps[ref], 1)
        dx, dy = (0, 0)
        add_text(board, 'K', x + (1.4 if ref == 'D1' else -1.6), y + (0 if ref == 'D1' else -1.4), 0.8, 0, 'F')
    x, y = pad_xy(fps['U1'], 1)
    add_text(board, '1', x - 1.6, y + 1.2, 0.8, 0, 'F')
    for item in FREE_TEXT:
        add_text(board, *item)



# ------------------------------------------------------------------ wymiary do weryfikacji
# (nr, start, koniec, odsunięcie linii wymiaru, strona, opis do legendy)
DIMS = [
    ('W1', (0, 0), (W, 0), 5, 'F', 'szerokosc plytki: czy miesci sie we wnetrzu kolumny obudowy'),
    ('W2', (0, H), (0, 0), 9, 'F', 'wysokosc plytki (wariant kompaktowy, moduly na kablach)'),
    ('W3', (0, 13), (33 - 3.81, 13), -3, 'F', 'gniazdo kabla OLED (J7): od lewej krawedzi do pinu 1'),
    ('W4', (0, 36), (33 - 5.08, 36), -3, 'F', 'gniazdo kabla MAX7219 (J6): od lewej krawedzi do pinu 1'),
    ('W6', (0, 0), (0, 100 + DY), -7, 'F', 'gniazdo USB-C zasilania: od gornej krawedzi do osi wtyku (otwor w scianie obudowy)'),
    ('W7', (0, 0), (0, 113.0 + DY), 4, 'F', 'przycisk boczny: od gornej krawedzi do osi popychacza (otwor w scianie)'),
    ('W8', (3.5, HY), (W - 3.5, HY), 4, 'F', 'otwory montazowe M3: rozstaw w poziomie'),
    ('W9', (3.5, HY), (3.5, H - 4), -4, 'F', 'otwory montazowe M3: rozstaw w pionie'),
    ('W10', (55, 94 + DY), (55, 94 + ROW_SPACING + DY), -6, 'B', 'DevKit: rozstaw rzedow pinow (srodek-srodek), ZMIERZONE 23,1 mm'),
    ('W11', (55, 94 + ROW_SPACING + DY), (55 - 38.1, 94 + ROW_SPACING + DY), -5, 'B', 'DevKit: pin 1 - pin 16 jednej listwy (15 x 2,54 mm), ZMIERZONE 38,1 mm'),
    ('W12', (55, 94 + DY), (W, 94 + DY), 4, 'B', 'DevKit: od prawej krawedzi (widok od frontu) do pinu 1, ZMIERZONE 11,0 mm'),
]


MEASURED = {'W1', 'W10', 'W11', 'W12'}   # wariant kompaktowy: reszta zmieniła położenie, do ponownego zatwierdzenia


def add_dimensions(board):
    for (nr, a, b, h, side, _) in DIMS:
        d = pcbnew.PCB_DIM_ALIGNED(board)
        d.SetLayer(pcbnew.Dwgs_User if side == 'F' else pcbnew.Cmts_User)
        d.SetStart(P(*a))
        d.SetEnd(P(*b))
        d.SetHeight(mm(h))
        d.SetUnitsMode(pcbnew.DIM_UNITS_MODE_MM)
        d.SetUnitsFormat(pcbnew.DIM_UNITS_FORMAT_BARE_SUFFIX)
        d.SetPrecision(pcbnew.DIM_PRECISION_X_XX)
        d.SetPrefix(nr + ': ')
        d.SetSuffix(' OK' if nr in MEASURED else ' ?')
        d.SetTextSize(pcbnew.VECTOR2I(mm(1.1), mm(1.1)))
        if side == 'B':
            d.SetMirrored(True)
        board.Add(d)
        d.Update()
    # legenda obok płytki
    for side, layer in (('F', pcbnew.Dwgs_User), ('B', pcbnew.Cmts_User)):
        lines = ['WYMIARY DO WERYFIKACJI  (? = do potwierdzenia, OK = zmierzone lub zatwierdzone 2026-10-13)',
                 'Wydruk w skali 1:1 (100%, bez dopasowania do strony).', '']
        lines += ['%s  %s' % (nr, opis) for (nr, _, _, _, sd, opis) in DIMS if sd == side]
        lines += ['', 'Strona: ' + ('PRZOD (widok od frontu)' if side == 'F' else
                                    'TYL (widok od tylu, lustro) - DevKit wtykany od tej strony'),
                  'Projekt roboczy v0.1 - NIE ZAMAWIAC przed weryfikacja wymiarow.']
        for i, line in enumerate(lines):
            t = pcbnew.PCB_TEXT(board)
            t.SetText(line)
            t.SetLayer(layer)
            t.SetTextSize(pcbnew.VECTOR2I(mm(1.6 if i == 0 else 1.25), mm(1.6 if i == 0 else 1.25)))
            t.SetTextThickness(mm(0.2 if i == 0 else 0.16))
            t.SetHorizJustify(pcbnew.GR_TEXT_H_ALIGN_LEFT)
            if side == 'B':
                # po odbiciu lustrzanym na wydruku tekst ląduje po prawej stronie płytki
                t.SetMirrored(True)
                t.SetPosition(P(-8, 4 + i * 3.0))
            else:
                t.SetPosition(P(W + 14, 4 + i * 3.0))
            board.Add(t)
    tb = board.GetTitleBlock()
    tb.SetTitle('Radar ESP32-C6 - plytka glowna - WYMIARY DO WERYFIKACJI')
    tb.SetRevision('0.1')
    tb.SetDate('2026-10-10')
    tb.SetComment(0, 'Projekt roboczy. Wymiary oznaczone ? sprawdzic na elementach przed zamowieniem.')


def build():
    comps, nets = load_netlist()
    board = pcbnew.BOARD()

    # --- netclassy
    ds = board.GetDesignSettings()
    ds.m_TrackMinWidth = mm(0.15)
    ds.m_CopperEdgeClearance = mm(0.3)
    ns = ds.m_NetSettings
    dflt = ns.GetDefaultNetclass()
    dflt.SetClearance(mm(0.2))
    dflt.SetTrackWidth(mm(0.3))
    dflt.SetViaDiameter(mm(0.7))
    dflt.SetViaDrill(mm(0.35))
    power = pcbnew.NETCLASS('Power')
    power.SetClearance(mm(0.25))
    power.SetTrackWidth(mm(0.8))
    power.SetViaDiameter(mm(0.9))
    power.SetViaDrill(mm(0.45))
    ns.SetNetclass('Power', power)
    vin = pcbnew.NETCLASS('Vin')
    vin.SetClearance(mm(0.2))
    vin.SetTrackWidth(mm(0.5))
    vin.SetViaDiameter(mm(0.8))
    vin.SetViaDrill(mm(0.4))
    ns.SetNetclass('Vin', vin)
    ns.SetNetclassPatternAssignment('VIN', 'Vin')
    for pat in ('+5V', 'GND', '+3V3', 'SR04_VCC', 'SERVO_5V'):
        ns.SetNetclassPatternAssignment(pat, 'Power')

    # --- sieci
    netinfo = {}
    for name in nets:
        ni = pcbnew.NETINFO_ITEM(board, name)
        board.Add(ni)
        netinfo[name] = ni
    pin_net = {}
    for name, nodes in nets.items():
        for ref, pin in nodes:
            pin_net[(ref, pin)] = name

    # --- footprinty
    fps = {}
    for ref, c in comps.items():
        fp = load_fp(c['fp'])
        fp.SetReference(ref)
        fp.SetPath(pcbnew.KIID_PATH('/' + c['uuid']))
        fp.SetValue(c['value'])
        fps[ref] = fp
        board.Add(fp)
        for pad in fp.Pads():
            nm = pin_net.get((ref, pad.GetNumber()))
            if nm:
                pad.SetNet(netinfo[nm])
        fp.Reference().SetTextSize(pcbnew.VECTOR2I(mm(SILK_H), mm(SILK_H)))
        fp.Reference().SetTextThickness(mm(SILK_T))
        for g in fp.GraphicalItems():
            if g.GetLayer() in (pcbnew.F_SilkS, pcbnew.B_SilkS) and g.GetWidth() < mm(SILK_LINE_MIN):
                g.SetWidth(mm(SILK_LINE_MIN))
        fp.Value().SetVisible(False)
        if ref.startswith('TP') or ref == 'J10':
            fp.Reference().SetVisible(False)

    for ref in FIXED:
        place_fixed(board, fps[ref], ref)

    missing = [r for r in fps if r not in FIXED]
    if missing:
        raise RuntimeError('brak pozycji dla: ' + ', '.join(missing))

    # --- obrys i otwory
    for (x0, y0, x1, y1) in [(0, 0, W, 0), (W, 0, W, H), (W, H, 0, H), (0, H, 0, 0)]:
        seg = pcbnew.PCB_SHAPE(board)
        seg.SetShape(pcbnew.SHAPE_T_SEGMENT)
        seg.SetLayer(pcbnew.Edge_Cuts)
        seg.SetWidth(mm(0.1))
        seg.SetStart(P(x0, y0))
        seg.SetEnd(P(x1, y1))
        board.Add(seg)
    for i, (hx, hy) in enumerate(HOLES):
        h = load_fp('MountingHole:MountingHole_3.2mm_M3')
        h.SetReference('H%d' % (i + 1))
        h.Reference().SetVisible(False)
        h.Value().SetVisible(False)
        h.SetBoardOnly(True)
        h.SetPosition(P(hx, hy))
        board.Add(h)

    e = 0.9   # pas przy krawędzi bez ścieżek i przelotek (4 prostokątne strefy zakazu)
    for (x0, y0, x1, y1) in [(0, 0, W, e), (0, H - e, W, H), (0, 0, e, H), (W - e, 0, W, H)]:
        ka = pcbnew.ZONE(board)
        ka.SetIsRuleArea(True)
        ka.SetDoNotAllowTracks(True)
        ka.SetDoNotAllowVias(True)
        ka.SetDoNotAllowPads(False)
        ka.SetDoNotAllowFootprints(False)
        ka.SetDoNotAllowZoneFills(False)
        ka.SetLayerSet(pcbnew.LSET.AllCuMask())
        o = ka.Outline()
        o.NewOutline()
        for (x, y) in [(x0, y0), (x1, y0), (x1, y1), (x0, y1)]:
            o.Append(mm(OX + x), mm(OY + y))
        board.Add(ka)
    # pod anteną DevKita (koniec z pinem 1, x >= 57): bez miedzi na obu warstwach
    ka = pcbnew.ZONE(board)
    ka.SetIsRuleArea(True)
    ka.SetDoNotAllowTracks(True)
    ka.SetDoNotAllowVias(True)
    ka.SetDoNotAllowPads(False)
    ka.SetDoNotAllowFootprints(False)
    ka.SetDoNotAllowZoneFills(True)
    ka.SetLayerSet(pcbnew.LSET.AllCuMask())
    o = ka.Outline()
    o.NewOutline()
    for (x, y) in [(57, 91 + DY), (W, 91 + DY), (W, 121 + DY), (57, 121 + DY)]:
        o.Append(mm(OX + x), mm(OY + y))
    board.Add(ka)
    add_labels(board, fps)
    add_dimensions(board)
    board.Save(PCB)
    pcbnew.ExportSpecctraDSN(board, DSN)
    # kontrola położeń rzędów DevKita i modułów
    for ref, pin in [('J3', 1), ('J3', 16), ('J2', 1), ('J2', 16), ('J7', 1), ('J7', 4), ('J6', 1), ('J6', 5),
                     ('J4', 1), ('J4', 4), ('U1', 1), ('U1', 14), ('J1', 1)]:
        print(ref, pin, pad_xy(fps[ref], pin))
    print('Zapisano', PCB, 'oraz', DSN, '| elementów:', len(fps))


def finish():
    board = pcbnew.LoadBoard(PCB)
    ok = pcbnew.ImportSpecctraSES(board, SES)
    print('import SES:', ok)
    # przelotki, które kończą się ślepo (ścieżka tylko po jednej warstwie): usuwamy je przed wylewkami
    conn = board.GetConnectivity()
    conn.RecalculateRatsnest()
    dead = []
    for t in board.GetTracks():
        if t.Type() == pcbnew.PCB_VIA_T:
            layers = {x.GetLayer() for x in conn.GetConnectedTracks(t) if x.Type() != pcbnew.PCB_VIA_T}
            if len(layers) < 2 and not conn.GetConnectedPads(t):
                dead.append(t)
    for t in dead:
        board.Remove(t)
    print('usunięte ślepe przelotki:', len(dead))
    # wylewki GND na obu warstwach
    gnd = board.FindNet('GND')
    for layer in (pcbnew.F_Cu, pcbnew.B_Cu):
        z = pcbnew.ZONE(board)
        z.SetLayer(layer)
        z.SetNet(gnd)
        z.SetPadConnection(pcbnew.ZONE_CONNECTION_FULL)
        z.SetMinThickness(mm(0.25))
        z.SetThermalReliefGap(mm(0.3))
        z.SetThermalReliefSpokeWidth(mm(0.4))
        out = z.Outline()
        out.NewOutline()
        for (x, y) in [(0.5, 0.5), (W - 0.5, 0.5), (W - 0.5, H - 0.5), (0.5, H - 0.5)]:
            out.Append(mm(OX + x), mm(OY + y))
        board.Add(z)
    pcbnew.ZONE_FILLER(board).Fill(board.Zones())
    board.Save(PCB)
    print('zapisano z wylewkami GND')


def relabel():
    """Szybka poprawka opisów: usuwa napisy z silkscreenu i dodaje je ponownie na gotowej płytce
    (bez ponownego rozmieszczania i trasowania). Po zmianach w VALUES/FREE_TEXT/PIN_NAMES."""
    board = pcbnew.LoadBoard(PCB)
    for t in list(board.GetDrawings()):
        if t.Type() == pcbnew.PCB_TEXT_T and t.GetLayer() in (pcbnew.F_SilkS, pcbnew.B_SilkS):
            board.RemoveNative(t)
    fps = {fp.GetReference(): fp for fp in board.GetFootprints()}
    for ref, fp in fps.items():
        if ref.startswith('H'):
            fp.Reference().SetVisible(False)
            fp.Value().SetVisible(False)
    add_labels(board, fps)
    board.Save(PCB)
    print('opisy przeliczone')


if __name__ == '__main__':
    {'place': build, 'finish': finish, 'relabel': relabel}[sys.argv[1]]()
