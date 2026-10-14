"""Buduje BOM i CPL w formacie JLCPCB z netlisty i pliku pozycji KiCada.

Użycie (z katalogu hardware/pcb):
  python3 tools/jlcpcb_bom.py <plik_pozycji_kicad.csv> <katalog_wyjściowy>
Do BOM trafiają tylko elementy SMD montowane przez JLCPCB (strona przednia). Elementy przewlekane
(gniazda, listwy, SW1, elektrolity) i niemontowane (DNP) są pominięte, a lista pominiętych jest wypisana.
"""
import csv
import os
import sys
import xml.etree.ElementTree as ET
from collections import OrderedDict

HERE = os.path.dirname(os.path.abspath(__file__))
NETLIST = os.path.join(HERE, '..', 'radar-pcb.net')

# (komentarz, numer LCSC, typ w bibliotece JLCPCB) wg wartości z netlisty; numery sprawdzone na stronach
# jlcpcb.com/partdetail/<numer> 2026-10-14 (nie sprawdzono stanów magazynowych).
PARTS = OrderedDict([
    ('1 k', ('1kΩ ±1% 0805', 'C17513', 'Basic')),
    ('10 k', ('10kΩ ±1% 0805', 'C17414', 'Basic')),
    ('100 k', ('100kΩ ±1% 0805', 'C17407', 'Basic')),
    ('47 k', ('47kΩ ±1% 0805', 'C17713', 'Basic')),
    ('100', ('100Ω ±1% 0805', 'C17408', 'Basic')),
    ('10', ('10Ω ±1% 0805', 'C17415', 'sprawdzić')),
    ('330', ('330Ω ±1% 0805', 'C17630', 'Basic')),
    ('5,1 k', ('5.1kΩ ±1% 0805', 'C27834', 'Basic')),
    ('15 k', ('15kΩ ±1% 0805', 'C17475', 'Basic')),
    ('0 R (zworka; opcja: ferryt >= 1 A)', ('0Ω zworka 0805 (FB1)', 'C17477', 'Basic')),
    ('100 nF', ('100nF 50V X7R 0805', 'C49678', 'Basic')),
    ('1 uF', ('1uF 50V X7R 0805', 'C28323', 'Basic')),
    ('LED zasilania', ('LED czerwona 0805', 'C84256', 'Basic')),
    ('1N4148W', ('1N4148W SOD-123', 'C81598', 'Basic')),
    ('1N5819HW', ('1N5819HW-7-F SOD-123', 'C82544', 'sprawdzić')),
    ('SMAJ5.0A (TVS 5 V, jednokierunkowy)', ('SMAJ5.0A SMA (MDD)', 'C113952', 'sprawdzić')),
    ('MMBT3904', ('MMBT3904 SOT-23', 'C20526', 'Basic')),
    ('SN74AHCT125D', ('SN74AHCT125DR SOIC-14', 'C155176', 'sprawdzić')),
    ('AP22802AW5-7 (przelacznik 5 V, LCSC C211404, limit ok. 2,2-3,2 A)', ('AP22802AW5-7 SOT-25', 'C211404', 'sprawdzić')),
    ('USB-C zasilanie 5 V (GCT USB4105)', ('USB4105-GF-A USB-C', 'C3020560', 'sprawdzić')),
])
DNP = {'C12', 'R24', 'R25'}  # opcjonalne, niemontowane (zob. order-notes.md)


def main(pos_csv, outdir):
    comps = {}
    for c in ET.parse(NETLIST).getroot().find('components'):
        comps[c.get('ref')] = (c.findtext('value'), c.findtext('footprint'))
    on_front = {}
    with open(pos_csv, newline='') as f:
        for row in csv.DictReader(f):
            on_front[row['Ref']] = row
    groups = OrderedDict()
    skipped = []
    for ref, (val, fp) in sorted(comps.items(), key=lambda kv: (kv[0].rstrip('0123456789'), int(kv[0].lstrip('ABCDEFGHIJKLMNOPQRSTUVWXYZ') or 0))):
        if ref in DNP or ref.startswith(('TP', 'H')):
            skipped.append((ref, 'DNP / punkt testowy'))
            continue
        if 'THT' in fp or 'PinHeader' in fp or 'PinSocket' in fp:
            skipped.append((ref, 'przewlekany, lutuje użytkownik'))
            continue
        if val not in PARTS:
            raise SystemExit('brak numeru LCSC dla %s (%s)' % (ref, val))
        groups.setdefault(val, []).append(ref)
    os.makedirs(outdir, exist_ok=True)
    smd_refs = [r for refs in groups.values() for r in refs]
    with open(os.path.join(outdir, 'bom-jlcpcb.csv'), 'w', newline='', encoding='utf-8') as f:
        w = csv.writer(f)
        w.writerow(['Comment', 'Designator', 'Footprint', 'LCSC Part #'])
        for val, refs in groups.items():
            comment, lcsc, _ = PARTS[val]
            fp = comps[refs[0]][1].split(':')[-1]
            w.writerow([comment, ','.join(refs), fp, lcsc])
    with open(os.path.join(outdir, 'cpl-jlcpcb.csv'), 'w', newline='', encoding='utf-8') as f:
        w = csv.writer(f)
        w.writerow(['Designator', 'Mid X', 'Mid Y', 'Layer', 'Rotation'])
        for ref in smd_refs:
            row = on_front[ref]
            if row['Side'].lower() != 'top':
                raise SystemExit('%s nie jest na stronie przedniej' % ref)
            w.writerow([ref, '%smm' % row['PosX'], '%smm' % row['PosY'], 'Top', row['Rot']])
    print('BOM: %d pozycji, %d elementów; pominięte: %s' % (len(groups), len(smd_refs),
          ', '.join('%s (%s)' % s for s in skipped)))


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
