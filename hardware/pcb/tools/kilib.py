"""Minimalny czytnik bibliotek symboli KiCad (do generowania schematu skryptem).
Uruchamiać w środowisku Flatpaka: flatpak run --command=python3 --filesystem=host org.kicad.KiCad ...
"""
import re

LIB = '/app/extensions/Library/symbols/'


def _block(t, i):
    d = 0
    j = i
    while True:
        c = t[j]
        if c == '(':
            d += 1
        elif c == ')':
            d -= 1
            if d == 0:
                return t[i:j + 1]
        j += 1


def raw(libfile, name):
    t = open(LIB + libfile + '.kicad_sym').read()
    return _block(t, t.index('(symbol "%s"' % name))


def flat(libfile, name):
    """Zwraca tekst symbolu bez 'extends' (dziedziczone symbole są spłaszczane)."""
    s = raw(libfile, name)
    m = re.search(r'\(extends "([^"]+)"\)', s)
    if not m:
        return s
    base = flat(libfile, m.group(1))
    bname = m.group(1)
    out = base.replace('(symbol "%s"' % bname, '(symbol "%s"' % name, 1)
    out = re.sub(r'\(symbol "%s_(\d+)_(\d+)"' % re.escape(bname), r'(symbol "%s_\1_\2"' % name, out)
    for prop in ('Value', 'Footprint', 'Datasheet', 'Description', 'ki_keywords'):
        pm = re.search(r'\(property "%s" "([^"]*)"' % prop, s)
        if pm:
            out = re.sub(r'(\(property "%s" ")[^"]*(")' % prop,
                         lambda mm: mm.group(1) + pm.group(1) + mm.group(2), out, count=1)
    return out


def pins(sym_text, name):
    """Lista pinów: (numer, nazwa, x, y, kąt, typ, jednostka)."""
    res = []
    for sm in re.finditer(r'\(symbol "%s_(\d+)_(\d+)"' % re.escape(name), sym_text):
        unit = int(sm.group(1))
        blk = _block(sym_text, sm.start())
        for pm in re.finditer(r'\(pin (\w+) (\w+)\s*\(at ([-\d.]+) ([-\d.]+) (\d+)\)', blk):
            pb = _block(blk, pm.start())
            num = re.search(r'\(number "([^"]*)"', pb).group(1)
            nm = re.search(r'\(name "([^"]*)"', pb).group(1)
            res.append((num, nm, float(pm.group(3)), float(pm.group(4)), int(pm.group(5)), pm.group(1), unit))
    return res
