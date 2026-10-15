"""Zapisuje wymiary i położenia padów wszystkich footprintów (dla 4 obrotów) do fpinfo.json.

Wejście: radar-pcb.net. Uruchamiać w Flatpaku KiCada (potrzebny pcbnew), z katalogu repo:
  flatpak run --command=python3 --filesystem=host org.kicad.KiCad hardware/pcb/tools/fpinfo.py
"""
import json
import os
import xml.etree.ElementTree as ET

import pcbnew

HERE = os.path.dirname(os.path.abspath(__file__))
PCB_DIR = os.path.normpath(os.path.join(HERE, '..'))
FP_ROOT = '/app/extensions/Library/footprints/'
mm = pcbnew.FromMM


def info(libname):
    lib, name = libname.split(':')
    res = {}
    for rot in (0, 90, 180, 270):
        fp = pcbnew.FootprintLoad(FP_ROOT + lib + '.pretty', name)
        fp.SetPosition(pcbnew.VECTOR2I(0, 0))
        fp.SetOrientationDegrees(rot)
        bb = fp.GetBoundingBox(False)
        pads = {}
        for p in fp.Pads():
            v = p.GetPosition()
            pads.setdefault(p.GetNumber(), []).append([round(pcbnew.ToMM(v.x), 3), round(pcbnew.ToMM(v.y), 3)])
        res[str(rot)] = {'bbox': [round(pcbnew.ToMM(bb.GetLeft()), 3), round(pcbnew.ToMM(bb.GetTop()), 3),
                                  round(pcbnew.ToMM(bb.GetRight()), 3), round(pcbnew.ToMM(bb.GetBottom()), 3)],
                         'pads': pads}
    return res


def main():
    root = ET.parse(os.path.join(PCB_DIR, 'radar-pcb.net')).getroot()
    out = {}
    cache = {}
    for c in root.find('components'):
        ref, fpn, val = c.get('ref'), c.findtext('footprint'), c.findtext('value')
        if fpn not in cache:
            cache[fpn] = info(fpn)
        out[ref] = {'fp': fpn, 'value': val, 'rots': cache[fpn]}
    nets = {}
    for n in root.find('nets'):
        nets[n.get('name')] = [[nd.get('ref'), nd.get('pin')] for nd in n.findall('node')]
    json.dump({'parts': out, 'nets': nets}, open(os.path.join(HERE, 'fpinfo.json'), 'w'))
    print('fpinfo.json:', len(out), 'elementów,', len(nets), 'sieci')


main()
