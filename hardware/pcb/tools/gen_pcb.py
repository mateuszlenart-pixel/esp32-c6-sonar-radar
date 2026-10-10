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

W, H = 66.0, 128.0          # obrys płytki, mm (propozycja do zatwierdzenia)
OX, OY = 100.0, 60.0        # przesunięcie na arkuszu KiCada
mm = pcbnew.FromMM


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
FIXED = {
    # góra: złącza kabli do głowicy (sonar, serwo), buzzer, przycisk zewnętrzny
    'J4': (6, 4, 90, 'F'), 'J5': (20, 4, 90, 'F'), 'J8': (34, 4, 90, 'F'), 'J9': (42, 4, 90, 'F'),
    # sterownik buzzera (pas między złączami a gniazdem OLED)
    'Q2': (47, 9, 0, 'F'), 'R9': (51.5, 8, 0, 'F'), 'R10': (51.5, 10.8, 0, 'F'), 'D2': (57, 9, 0, 'F'),
    # moduły wtykane od frontu (nagłówki u góry modułów)
    'J7': (33 - 3.81, 13, 90, 'F'), 'J6': (33 - 5.08, 55, 90, 'F'),
    # lewy pas: kondensator serwa, przycisk ON/standby
    'C4': (5.5, 50, 0, 'F'), 'SW1': (3.75, 63.75, 0, 'F'), 'C5': (7, 72, 0, 'F'),
    # prawy pas: gniazdo DC (otwór na krawędzi), bezpiecznik, MOSFET, kondensatory, LED
    'J1': (52.3, 66, 180, 'F'), 'D1': (63.5, 74, 90, 'F'), 'R1': (59.5, 73.6, 0, 'F'),
    'C1': (55.5, 80, 0, 'F'), 'C2': (63, 80, 90, 'F'), 'F1': (57.25, 89, 0, 'F'), 'Q1': (63, 89, 90, 'F'),
    'TP1': (59.5, 97, 0, 'F'), 'TP3': (63, 97, 0, 'F'),
    # DevKit od tyłu (poziomo, USB po lewej w widoku od frontu)
    'J3': (55, 94, 90, 'B'), 'J2': (55, 117.3, 90, 'B'),
    # bufor w podstawce między rzędami DevKita, pull-downy przy wejściach, odsprzęganie
    'U1': (21, 108, 90, 'F'), 'C3': (17.5, 104, 90, 'F'),
    'R2': (23.5, 112, 0, 'F'), 'R3': (31.2, 112, 0, 'F'), 'R4': (33.7, 97.4, 0, 'F'), 'R5': (26.1, 97.4, 0, 'F'),
    # dzielnik ECHO i punkt testowy węzła GPIO6
    'R6': (42, 104, 90, 'F'), 'R7': (45.5, 104, 90, 'F'), 'TP5': (49, 104, 0, 'F'),
    # pozostałe punkty testowe, rezystor serwa
    'TP2': (10, 100, 0, 'F'), 'TP4': (10, 104, 0, 'F'), 'R8': (30, 122, 0, 'F'),
}
HOLES = [(3.5, 32), (W - 3.5, 32), (3.5, 88), (W - 3.5, 124)]


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
    for pat in ('+5V', 'GND', 'VIN', 'VIN_F', '+3V3'):
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
        fp.Reference().SetTextSize(pcbnew.VECTOR2I(mm(0.8), mm(0.8)))
        fp.Value().SetVisible(False)

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


if __name__ == '__main__':
    {'place': build, 'finish': finish}[sys.argv[1]]()
