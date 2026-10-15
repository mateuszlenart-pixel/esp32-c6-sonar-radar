"""Zamienia kontury logo (logo-contours.json z logo_trace.py) na footprint logo/am-logo.kicad_mod na F.SilkS.

Wynik jest równoważny temu, co daje KiCad Image Converter (wielokąty wypełnione, otwory przecięte mostkiem).
Uruchamiać w Flatpaku KiCada (pcbnew do łączenia otworów):
  flatpak run --command=python3 --filesystem=host org.kicad.KiCad hardware/pcb/tools/logo_footprint.py
"""
import json
import os
import uuid

import pcbnew

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, '..', 'logo', 'am-logo.kicad_mod')
NM = 1e6


def area(p):
    return abs(sum(p[i][0] * p[(i + 1) % len(p)][1] - p[(i + 1) % len(p)][0] * p[i][1] for i in range(len(p))) / 2)


def inside(pt, poly):
    x, y = pt
    c = False
    j = len(poly) - 1
    for i in range(len(poly)):
        xi, yi = poly[i]
        xj, yj = poly[j]
        if (yi > y) != (yj > y) and x < (xj - xi) * (y - yi) / (yj - yi) + xi:
            c = not c
        j = i
    return c


def main():
    d = json.load(open(os.path.join(HERE, 'logo-contours.json')))
    outers = [l['pts'] for l in d['loops'] if not l['hole']]
    holes = [l['pts'] for l in d['loops'] if l['hole']]
    owner = {}
    for hi, h in enumerate(holes):
        cands = [oi for oi, o in enumerate(outers) if inside(h[0], o)]
        owner[hi] = min(cands, key=lambda oi: area(outers[oi])) if cands else None
    cx, cy = d['w'] / 2, d['h'] / 2
    ps = pcbnew.SHAPE_POLY_SET()
    for oi, o in enumerate(outers):
        idx = ps.NewOutline()
        for x, y in o:
            ps.Append(int((x - cx) * NM), int((y - cy) * NM), idx)
        for hi, h in enumerate(holes):
            if owner[hi] == oi:
                hidx = ps.NewHole(idx)
                for x, y in h:
                    ps.Append(int((x - cx) * NM), int((y - cy) * NM), idx, hidx)
    ps.Fracture()
    polys = []
    for i in range(ps.OutlineCount()):
        ch = ps.Outline(i)
        polys.append([(ch.CPoint(k).x / NM, ch.CPoint(k).y / NM) for k in range(ch.PointCount())])
    lines = ['(footprint "am-logo"', '\t(version 20260206)', '\t(generator "radar_gen")', '\t(layer "F.Cu")',
             '\t(descr "Logo Adam & Mateusz, silkscreen, %.1f x %.1f mm")' % (d['w'], d['h']),
             '\t(property "Reference" "LOGO"\n\t\t(at 0 0 0)\n\t\t(layer "F.SilkS")\n\t\t(hide yes)\n\t\t(effects\n\t\t\t(font\n\t\t\t\t(size 1 1)\n\t\t\t\t(thickness 0.15)\n\t\t\t)\n\t\t)\n\t)',
             '\t(property "Value" "am-logo"\n\t\t(at 0 0 0)\n\t\t(layer "F.Fab")\n\t\t(hide yes)\n\t\t(effects\n\t\t\t(font\n\t\t\t\t(size 1 1)\n\t\t\t\t(thickness 0.15)\n\t\t\t)\n\t\t)\n\t)',
             '\t(attr exclude_from_pos_files exclude_from_bom allow_missing_courtyard)']
    for p in polys:
        pts = ' '.join('(xy %.4f %.4f)' % (x, y) for x, y in p)
        lines.append('\t(fp_poly\n\t\t(pts\n\t\t\t%s\n\t\t)\n\t\t(stroke\n\t\t\t(width 0)\n\t\t\t(type solid)\n\t\t)\n\t\t(fill yes)\n\t\t(layer "F.SilkS")\n\t\t(uuid "%s")\n\t)' % (pts, uuid.uuid4()))
    lines.append(')')
    open(OUT, 'w').write('\n'.join(lines) + '\n')
    print('am-logo.kicad_mod: %d wielokątów, %d punktów' % (len(polys), sum(len(p) for p in polys)))


main()
