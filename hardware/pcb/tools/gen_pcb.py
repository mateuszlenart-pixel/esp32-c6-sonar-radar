"""Buduje płytkę radar-pcb.kicad_pcb z netlisty schematu (KiCad 10, Python pcbnew), wersja przewlekana.

Kroki: wczytanie netlisty XML -> footprinty z bibliotek KiCada -> rozmieszczenie (kotwice z geom.py,
reszta z layout.json, wynik autoplace.py) -> obrys i otwory -> eksport DSN -> (zewnętrznie) Freerouting
-> import SES -> wylewki GND.

Użycie (z katalogu repo):
  flatpak run --command=python3 --filesystem=host org.kicad.KiCad hardware/pcb/tools/gen_pcb.py place
  java -jar ~/tools/freerouting/freerouting-2.5.0.jar -de hardware/pcb/radar-pcb.dsn -do hardware/pcb/radar-pcb.ses -mp 40
  flatpak run --command=python3 --filesystem=host org.kicad.KiCad hardware/pcb/tools/gen_pcb.py finish
"""
import json
import os
import sys
import xml.etree.ElementTree as ET

import pcbnew

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import geom as G

PCB_DIR = os.path.normpath(os.path.join(HERE, '..'))
NETLIST = os.path.join(PCB_DIR, 'radar-pcb.net')
PCB = os.path.join(PCB_DIR, 'radar-pcb.kicad_pcb')
DSN = os.path.join(PCB_DIR, 'radar-pcb.dsn')
SES = os.path.join(PCB_DIR, 'radar-pcb.ses')
LAYOUT = os.path.join(HERE, 'layout.json')
FP_ROOT = '/app/extensions/Library/footprints/'

W, H = G.W, G.H             # obrys płytki, mm
OX, OY = 100.0, 25.0        # przesunięcie na arkuszu KiCada
mm = pcbnew.FromMM
SILK_H, SILK_T = 1.0, 0.17   # napisy na silkscreenie: JLCPCB wymaga wysokości >= 1 mm i grubości kreski >= 0,15 mm
SILK_LINE_MIN = 0.15        # minimalna grubość linii obrysu elementów na silkscreenie


def set_origin(board):
    """Początek pomocniczy (Gerbery, wiercenia) w lewym dolnym rogu płytki."""
    board.GetDesignSettings().SetAuxOrigin(P(0, H))


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


def load_layout():
    """ref -> (x, y, obrót, strona, opis) dla wszystkich elementów (kotwice + wynik autoplace.py)."""
    lay = {}
    for ref, (x, y, rot, side, _ext) in G.ANCHORS.items():
        lay[ref] = (x, y, rot, side, None)
    auto = json.load(open(LAYOUT))['parts']
    for ref, a in auto.items():
        lay[ref] = (a['x'], a['y'], a['rot'], 'F', (a['label'], a['lx'], a['ly'], a['lang']))
    return lay


def place_fixed(board, fp, lay):
    x, y, rot, side, _ = lay
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


# ------------------------------------------------------------------ opisy na płytce
PIN_NAMES = {
    'J4': ('J4 HC-SR04', ['VCC', 'TRIG', 'ECHO', 'GND']),
    'J5': ('J5 SERWO', ['GND', '+5V', 'SIG']),
    'J8': ('J8 BUZZ', ['+', '-']),
    'J9': ('J9 PRZYC.', ['IO2', 'GND']),
    'J7': ('J7 OLED SH1106', ['VCC', 'GND', 'SCK', 'SDA']),
    'J6': ('J6 MAX7219', ['VCC', 'GND', 'DIN', 'CS', 'CLK']),
}
DEVKIT = {
    'J3': ('DevKit J3', ['GND', 'TX', 'RX', 'IO15', 'IO23', 'IO22', 'IO21', 'IO20', 'IO19', 'IO18', 'IO9',
                         'GND', 'IO13', 'IO12', 'GND', 'NC']),
    'J2': ('DevKit J1', ['3V3', 'RST', 'IO4', 'IO5', 'IO6', 'IO7', 'IO0', 'IO1', 'IO8', 'IO10', 'IO11',
                         'IO2', 'IO3', '5V', 'GND', 'NC']),
}
# listwy z kopią pinów DevKita (front): J11 = J3, J12 = J2 (pin 14 = szyna +5V płytki)
COPY = {'J11': ('J3', 'above'), 'J12': ('J2', 'below')}
RAIL_NAMES = ['GND', '3V3', '5V', 'GND', 'GND', '3V3', '5V', 'GND']
PROTO = {'J14', 'J15', 'J16', 'J17'}


def free_text():
    ycen = G.YC
    return [
        # (tekst, x, y, wysokość, obrót, warstwa[, wyrównanie])
        ('J1 DC 5V (+ srodek)', 1.5, ycen - 0.95 - 4.86 - 1.6, 1.0, 0, 'F', 'left'),
        ('Radar C6 v0.2 przewlekana', 30.0, G.H - 1.9, 1.0, 0, 'F'),
        ('POLE PROTOTYPOWE (rastr 2,54)', 28.0, G.PROTO_Y0 - 5.6, 1.0, 0, 'F'),
        ('Radar ESP32-C6 - tyl, DevKit USB-C ->', 40.0, G.H - 1.9, 1.0, 0, 'B'),
    ]


def add_text(board, s, x, y, h=1.0, angle=0, side='F', just='center'):
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


def add_labels(board, fps, lay):
    # złącza na froncie: nazwa pinu pionowo pod każdym pinem, nazwa modułu po lewej
    for ref, (title, names) in PIN_NAMES.items():
        fp = fps[ref]
        for i, n in enumerate(names):
            x, y = pad_xy(fp, i + 1)
            add_text(board, n, x, y + 1.6, 1.0, 90, 'F', 'right')
        x1, y1 = pad_xy(fp, 1)
        add_text(board, title, x1 - 1.9, y1 + 1.6, 1.0, 90, 'F', 'right')
    # kopie pinów DevKita na froncie: opisy nad J11 (do góry) i pod J12 (w dół)
    for ref, (src, where) in COPY.items():
        names = DEVKIT[src][1]
        for i, n in enumerate(names):
            x, y = pad_xy(fps[ref], i + 1)
            if where == 'above':
                add_text(board, n, x, y - 1.7, 1.0, 90, 'F', 'left')
            else:
                add_text(board, n, x, y + 1.7, 1.0, 90, 'F', 'right')
    for i, n in enumerate(RAIL_NAMES):
        x, y = pad_xy(fps['J13'], i + 1)
        add_text(board, n, x, y + 1.7, 1.0, 90, 'F', 'right')
    # pozostałe elementy: opis z layout.json (oznaczenie + wartość)
    for ref, (x, y, rot, side, lab) in lay.items():
        if lab is None:
            continue
        text, lx, ly, lang = lab
        add_text(board, text, lx, ly, 1.0, lang, 'F', 'center')
    for item in free_text():
        add_text(board, *item)


# ------------------------------------------------------------------ wymiary do weryfikacji
# (nr, start, koniec, odsunięcie linii wymiaru, strona, opis do legendy)
DIMS = [
    ('W1', (0, 0), (W, 0), 5, 'F', 'szerokosc plytki: czy miesci sie we wnetrzu obudowy'),
    ('W2', (0, H), (0, 0), 9, 'F', 'wysokosc plytki (wersja przewlekana, z polem prototypowym)'),
    ('W6', (0, 0), (0, G.YC), -7, 'F', 'gniazdo DC: od gornej krawedzi do osi wtyku (otwor w scianie obudowy)'),
    ('W7', (0, 0), (0, G.YC + 11.25), 4, 'F', 'przycisk boczny: od gornej krawedzi do osi popychacza (otwor w scianie)'),
    ('W8', (G.HOLES[0][0], G.HOLES[0][1]), (G.HOLES[1][0], G.HOLES[1][1]), 4, 'F', 'otwory montazowe M3: rozstaw w poziomie'),
    ('W9', (G.HOLES[0][0], G.HOLES[0][1]), (G.HOLES[2][0], G.HOLES[2][1]), -4, 'F', 'otwory montazowe M3: rozstaw w pionie'),
    ('W10', (G.XR, G.Y3), (G.XR, G.Y2), -6, 'B', 'DevKit: rozstaw rzedow pinow (srodek-srodek), ZMIERZONE 23,1 mm'),
    ('W11', (G.XR, G.Y2), (G.XR - 38.1, G.Y2), -5, 'B', 'DevKit: pin 1 - pin 16 jednej listwy (15 x 2,54 mm), ZMIERZONE 38,1 mm'),
    ('W12', (G.XR, G.Y3), (W, G.Y3), 4, 'B', 'DevKit: od prawej krawedzi (widok od frontu) do pinu 1, ZMIERZONE 11,0 mm'),
]

MEASURED = {'W10', 'W11', 'W12'}   # wersja przewlekana: pozostałe do zatwierdzenia


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
        lines = ['WYMIARY DO WERYFIKACJI  (? = do potwierdzenia, OK = zmierzone)',
                 'Wydruk w skali 1:1 (100%, bez dopasowania do strony).', '']
        lines += ['%s  %s' % (nr, opis) for (nr, _, _, _, sd, opis) in DIMS if sd == side]
        lines += ['', 'Strona: ' + ('PRZOD (widok od frontu)' if side == 'F' else
                                    'TYL (widok od tylu, lustro) - DevKit wtykany od tej strony'),
                  'Projekt roboczy v0.2 (przewlekany) - NIE ZAMAWIAC przed weryfikacja wymiarow.']
        for i, line in enumerate(lines):
            t = pcbnew.PCB_TEXT(board)
            t.SetText(line)
            t.SetLayer(layer)
            t.SetTextSize(pcbnew.VECTOR2I(mm(1.6 if i == 0 else 1.25), mm(1.6 if i == 0 else 1.25)))
            t.SetTextThickness(mm(0.2 if i == 0 else 0.16))
            t.SetHorizJustify(pcbnew.GR_TEXT_H_ALIGN_LEFT)
            if side == 'B':
                t.SetMirrored(True)
                t.SetPosition(P(-8, 4 + i * 3.0))
            else:
                t.SetPosition(P(W + 14, 4 + i * 3.0))
            board.Add(t)
    tb = board.GetTitleBlock()
    tb.SetTitle('Radar ESP32-C6 - plytka glowna (wersja przewlekana)')
    tb.SetRevision('0.2')
    tb.SetDate('2026-10-15')
    tb.SetComment(0, 'Projekt roboczy. Wymiary oznaczone ? sprawdzic na elementach przed zamowieniem.')


LOGO_DIR = os.path.join(PCB_DIR, 'logo')


def add_logo(board):
    """Logo z KiCad Image Converter: pierwszy plik logo/*.kicad_mod, wyśrodkowany w obszarze G.LOGO.

    Plik robi się w KiCadzie: Image Converter -> import jako Footprint -> rozmiar w mm (szerokość <= 22 mm)
    -> warstwa F.SilkS -> eksport .kicad_mod do hardware/pcb/logo/. Brak pliku = brak logo.
    """
    import glob
    import shutil
    files = sorted(glob.glob(os.path.join(LOGO_DIR, '*.kicad_mod')))
    if not files:
        return
    pretty = os.path.join(HERE, '_logo.pretty')
    os.makedirs(pretty, exist_ok=True)
    name = os.path.splitext(os.path.basename(files[0]))[0]
    shutil.copy(files[0], os.path.join(pretty, name + '.kicad_mod'))
    fp = pcbnew.FootprintLoad(pretty, name)
    fp.SetReference('LOGO')
    fp.Reference().SetVisible(False)
    fp.Value().SetVisible(False)
    fp.SetBoardOnly(True)
    board.Add(fp)
    fp.SetPosition(pcbnew.VECTOR2I(0, 0))
    bb = fp.GetBoundingBox(False)
    x0, y0, x1, y1 = G.LOGO
    cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
    fp.SetPosition(pcbnew.VECTOR2I(mm(OX + cx) - bb.GetCenter().x, mm(OY + cy) - bb.GetCenter().y))
    w, h = pcbnew.ToMM(bb.GetWidth()), pcbnew.ToMM(bb.GetHeight())
    print('logo: %.1f x %.1f mm (miejsce %.1f x %.1f mm)' % (w, h, x1 - x0, y1 - y0))


def zone_rule(board, x0, y0, x1, y1, tracks=True, fills=False, pads=False):
    ka = pcbnew.ZONE(board)
    ka.SetIsRuleArea(True)
    ka.SetDoNotAllowTracks(tracks)
    ka.SetDoNotAllowVias(tracks)
    ka.SetDoNotAllowPads(pads)
    ka.SetDoNotAllowFootprints(False)
    ka.SetDoNotAllowZoneFills(fills)
    ka.SetLayerSet(pcbnew.LSET.AllCuMask())
    o = ka.Outline()
    o.NewOutline()
    for (x, y) in [(x0, y0), (x1, y0), (x1, y1), (x0, y1)]:
        o.Append(mm(OX + x), mm(OY + y))
    board.Add(ka)


def build():
    comps, nets = load_netlist()
    lay = load_layout()
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
    for pat in ('VIN', '+5V', 'GND', '+3V3', 'SR04_VCC', 'SERVO_5V', 'DEVKIT_5V'):
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
        fp.Reference().SetVisible(False)     # oznaczenia rysujemy sami (add_labels), z wartością
        fp.Value().SetVisible(False)
        for g in list(fp.GraphicalItems()):
            if ref in PROTO and g.GetLayer() in (pcbnew.F_SilkS, pcbnew.B_SilkS, pcbnew.F_CrtYd, pcbnew.B_CrtYd):
                fp.RemoveNative(g)      # pole prototypowe: same otwory, bez obrysów (bloki stykają się rastrem)
                continue
            if ref == 'J1' and g.GetLayer() == pcbnew.F_SilkS and min(pcbnew.ToMM(g.GetStart().x), pcbnew.ToMM(g.GetEnd().x)) - OX < 1.2:
                fp.RemoveNative(g)      # obrys gniazda DC przy krawędzi płytki
                continue
            if g.Type() == pcbnew.PCB_SHAPE_T and g.GetLayer() in (pcbnew.F_SilkS, pcbnew.B_SilkS) and g.GetWidth() < mm(SILK_LINE_MIN):
                g.SetWidth(mm(SILK_LINE_MIN))

    missing = [r for r in fps if r not in lay]
    if missing:
        raise RuntimeError('brak pozycji dla: ' + ', '.join(missing))
    for ref in lay:
        place_fixed(board, fps[ref], lay[ref])

    # --- obrys i otwory
    for (x0, y0, x1, y1) in [(0, 0, W, 0), (W, 0, W, H), (W, H, 0, H), (0, H, 0, 0)]:
        seg = pcbnew.PCB_SHAPE(board)
        seg.SetShape(pcbnew.SHAPE_T_SEGMENT)
        seg.SetLayer(pcbnew.Edge_Cuts)
        seg.SetWidth(mm(0.1))
        seg.SetStart(P(x0, y0))
        seg.SetEnd(P(x1, y1))
        board.Add(seg)
    for i, (hx, hy) in enumerate(G.HOLES):
        h = load_fp('MountingHole:MountingHole_3.2mm_M3')
        h.SetReference('H%d' % (i + 1))
        h.Reference().SetVisible(False)
        h.Value().SetVisible(False)
        h.SetBoardOnly(True)
        h.SetPosition(P(hx, hy))
        board.Add(h)

    e = 0.9   # pas przy krawędzi bez ścieżek i przelotek
    for (x0, y0, x1, y1) in [(0, 0, W, e), (0, H - e, W, H), (0, 0, e, H), (W - e, 0, W, H)]:
        zone_rule(board, x0, y0, x1, y1)
    # pod anteną DevKita: bez miedzi na obu warstwach
    zone_rule(board, G.ANT[0], G.ANT[1], G.ANT[2], G.ANT[3], tracks=True, fills=True)
    # ramka pola prototypowego (silkscreen)
    x0, y0 = 11.0 - 1.4, G.PROTO_Y0 - 2.54 - 1.4
    x1, y1 = 11.0 + 13 * 2.54 + 1.4, G.PROTO_Y0 + 3 * 5.08 + 1.4
    for (ax, ay, bx, by) in [(x0, y0, x1, y0), (x1, y0, x1, y1), (x1, y1, x0, y1), (x0, y1, x0, y0)]:
        seg = pcbnew.PCB_SHAPE(board)
        seg.SetShape(pcbnew.SHAPE_T_SEGMENT)
        seg.SetLayer(pcbnew.F_SilkS)
        seg.SetWidth(mm(SILK_LINE_MIN))
        seg.SetStart(P(ax, ay))
        seg.SetEnd(P(bx, by))
        board.Add(seg)
    add_labels(board, fps, lay)
    add_logo(board)
    add_dimensions(board)
    set_origin(board)
    board.Save(PCB)
    pcbnew.ExportSpecctraDSN(board, DSN)
    for ref, pin in [('J3', 1), ('J3', 16), ('J2', 1), ('J11', 1), ('J11', 16), ('J12', 1), ('J1', 1), ('J1', 2),
                     ('U1', 1), ('SW1', 1)]:
        print(ref, pin, pad_xy(fps[ref], pin))
    print('Zapisano', PCB, 'oraz', DSN, '| elementów:', len(fps), '| płytka %.1f x %.1f mm' % (W, H))


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
    set_origin(board)
    board.Save(PCB)
    print('zapisano z wylewkami GND')


def relabel():
    """Szybka poprawka opisów: usuwa napisy z silkscreenu i dodaje je ponownie na gotowej płytce
    (bez ponownego rozmieszczania i trasowania). Po zmianach w opisach/layout.json."""
    board = pcbnew.LoadBoard(PCB)
    for t in list(board.GetDrawings()):
        if t.Type() == pcbnew.PCB_TEXT_T and t.GetLayer() in (pcbnew.F_SilkS, pcbnew.B_SilkS):
            board.RemoveNative(t)
    fps = {fp.GetReference(): fp for fp in board.GetFootprints()}
    add_labels(board, fps, load_layout())
    set_origin(board)
    board.Save(PCB)
    print('opisy przeliczone')


if __name__ == '__main__':
    {'place': build, 'finish': finish, 'relabel': relabel}[sys.argv[1]]()
