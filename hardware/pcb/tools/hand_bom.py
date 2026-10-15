"""Lista zakupów elementów do ręcznego lutowania, z netlisty (radar-pcb.net).

Użycie (z katalogu repo):  python3 hardware/pcb/tools/hand_bom.py hardware/pcb/production
Zapisuje lista-zakupow.csv: ilość, oznaczenia, co kupić, uwagi. Elementy opcjonalne (nie montować)
są wypisane osobno.
"""
import csv
import os
import sys
import xml.etree.ElementTree as ET
from collections import OrderedDict

HERE = os.path.dirname(os.path.abspath(__file__))
NETLIST = os.path.join(HERE, '..', 'radar-pcb.net')

OPTIONAL = {'C12', 'R24', 'R25'}          # miejsca na elementy, których domyślnie nie montujemy
NO_BUY = {'TP1', 'TP2', 'TP3', 'TP4', 'TP5', 'TP6', 'J14', 'J15', 'J16', 'J17'}   # pady/otwory, nic nie kupujemy


def describe(ref, value, fp):
    name = fp.split(':')[1]
    if name.startswith('R_Axial'):
        v = value.replace(' (nie montowac)', '').replace(',', '.').split(' (')[0]
        if ref == 'FB1':
            return ('Zworka z drutu 0 R (albo koralik ferrytowy osiowy >= 1 A)', 'rezystor osiowy 0 R, rozstaw 7,62 mm')
        return ('Rezystor 1/4 W 1 %% %s' % v.replace(' k', 'k').replace(' ', ''), 'osiowy, rozstaw otworów 7,62 mm')
    if name.startswith('C_Disc_D7.5'):
        return ('Bezpiecznik polimerowy PTC 2 A (hold)', 'radialny, rozstaw 5,0 mm')
    if name.startswith('C_Disc'):
        return ('Kondensator ceramiczny %s' % value.replace(' (opcja)', ''), 'rozstaw wyprowadzeń 5,0 mm (MLCC radialny 50 V)')
    if name.startswith('CP_Radial'):
        return ('Kondensator elektrolityczny %s' % value, 'radialny, ' + name.replace('CP_Radial_', ''))
    if name == 'LED_D3.0mm':
        return ('LED 3 mm czerwona (+ rezystor R1)', 'przewlekana')
    if name.startswith('D_DO-35'):
        return ('Dioda 1N4148', 'DO-35, rozstaw 7,62 mm')
    if name.startswith('D_DO-41'):
        return ('Dioda Schottky 1N5819', 'DO-41, rozstaw 10,16 mm')
    if name.startswith('D_DO-15'):
        return ('Dioda TVS SA5.0A (jednokierunkowa)', 'DO-15, rozstaw 10,16 mm; pasek = katoda')
    if name == 'TO-92_Inline':
        return ('Tranzystor NPN 2N3904', 'TO-92, kolejnosc E B C przy plaskiej stronie do siebie')
    if name.startswith('DIP-14'):
        return ('Podstawka DIP-14 oraz uklad SN74AHCT125N (DIP-14)', 'wersja przewlekana 74AHCT125')
    if name == 'BarrelJack_Horizontal':
        return ('Gniazdo zasilania DC 5,5/2,1 mm na PCB (plug srodek +)', 'pasuje CUI PJ-002A i zamienniki o rastrze pinow 0 / -6 / -3 x 4,7 mm')
    if name.startswith('SW_Tactile'):
        return ('Przycisk kątowy C&K PTS645VL58-2LFS (lub zgodny)', 'tact switch kątowy, 4 piny THT')
    if name.startswith('PinSocket_1x'):
        n = int(name.split('1x')[1][:2])
        note = 'prosta, rastr 2,54 mm' + (', wysoka (8,5 mm) dla DevKita' if ref in ('J2', 'J3') else '')
        return ('Listwa gniazdo żeńskie 1x%d' % n, note)
    return (value, name)


def main(outdir):
    comps = OrderedDict()
    for c in ET.parse(NETLIST).getroot().find('components'):
        comps[c.get('ref')] = (c.findtext('value'), c.findtext('footprint'))

    def key(ref):
        return (ref.rstrip('0123456789'), int(ref.lstrip('ABCDEFGHIJKLMNOPQRSTUVWXYZ') or 0))

    groups = OrderedDict()
    optional = []
    for ref in sorted(comps, key=key):
        if ref in NO_BUY:
            continue
        val, fp = comps[ref]
        what, note = describe(ref, val, fp)
        if ref in OPTIONAL:
            optional.append((ref, what, note))
            continue
        groups.setdefault((what, note), []).append(ref)
    os.makedirs(outdir, exist_ok=True)
    with open(os.path.join(outdir, 'lista-zakupow.csv'), 'w', newline='', encoding='utf-8') as f:
        w = csv.writer(f)
        w.writerow(['Ilość', 'Oznaczenia', 'Co kupić', 'Uwagi'])
        for (what, note), refs in groups.items():
            w.writerow([len(refs), ','.join(refs), what, note])
        for ref, what, note in optional:
            w.writerow([0, ref, what + ' (OPCJA, nie montować)', note])
    print('lista-zakupow.csv:', len(groups), 'pozycji,', sum(len(r) for r in groups.values()), 'elementów')


if __name__ == '__main__':
    main(sys.argv[1])
