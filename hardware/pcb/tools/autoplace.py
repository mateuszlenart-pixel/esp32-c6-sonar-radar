"""Rozmieszcza elementy płytki (symulowane wyżarzanie): minimalna długość połączeń, bez nakładania się.

Wejście: tools/fpinfo.json (fpinfo.py). Kotwice i strefy: tools/geom.py. Wyjście: tools/layout.json
(położenie, obrót i miejsce opisu każdego swobodnego elementu). Wynik zależy od ziarna (--seed).
  python3 hardware/pcb/tools/autoplace.py [--seed N] [--iters N]
"""
import argparse
import json
import math
import os
import random
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import geom as G

GRID = 0.5
MARGIN = 1.0        # minimalny odstęp między prostokątami elementów (kanał dla ścieżek)
CHAR_W, TEXT_H = 0.9, 1.0
SKIP_NETS = {'GND'}
NET_W = {'+5V': 0.2, '+3V3': 0.2}
# dodatkowe przyciąganie (kondensatory odsprzęgające blisko układów): (ref1, pin1, ref2, pin2, waga)
PAIRS = [('C3', '1', 'U1', '14', 3), ('C3', '2', 'U1', '7', 1), ('C11', '1', 'Q2', '3', 1.5), ('C1', '1', 'F1', '2', 2),
         ('C2', '1', 'F1', '2', 2), ('D3', '1', 'F1', '2', 2), ('C9', '1', 'J6', '1', 2), ('C8', '1', 'J6', '1', 2),
         ('C7', '1', 'R11', '2', 1), ('C6', '1', 'R11', '2', 1), ('C10', '1', 'FB1', '2', 2), ('C4', '1', 'FB1', '2', 2),
         ('C13', '1', 'R21', '1', 2), ('C5', '1', 'R18', '2', 1)]


def short_value(ref, v):
    v = v.split(' (')[0].strip() if ref not in ('R24', 'R25', 'C12') else v
    if ref in ('R24', 'R25'):
        return '4k7 DNP'
    if ref == 'C12':
        return '100n DNP'
    if ref == 'FB1':
        return '0R'
    if ref.startswith('R'):
        v = v.replace(' ', '').replace(',', '.')
        return v if v.endswith('k') else v + 'R'
    if ref.startswith('C'):
        if '/' in v:
            a, b = v.split('/')
            return a.strip().replace(' uF', 'u').replace(' ', '') + ' ' + b.strip().replace(' ', '')
        return v.replace(' nF', 'n').replace(' uF', 'u').replace(' ', '')
    return {'D1': 'LED', 'D2': '1N4148', 'D3': 'SA5.0A', 'D4': '1N5819', 'Q2': '2N3904', 'U1': '74AHCT125',
            'F1': 'PTC 2A'}.get(ref, v)


def tp_name(v):
    return v.replace('TP ', '').replace('+3V3', '3V3').replace('SERVO_5V', 'SERVO5V').replace('TRIG_5V', 'TRIG').replace('ECHO_3V3', 'ECHO')


def label_text(ref, value):
    if ref.startswith('TP'):
        return tp_name(value)
    if ref == 'J10':
        return 'J10 BOOT GND RST'
    return '%s %s' % (ref, short_value(ref, value))


def load():
    d = json.load(open(os.path.join(HERE, 'fpinfo.json')))
    return d['parts'], d['nets']


def part_rect(info, label, x, y, rot):
    r = info['rots'][str(rot)]
    bx0, by0, bx1, by1 = r['bbox']
    lw = CHAR_W * len(label)
    if rot in (0, 180):
        cx = (bx0 + bx1) / 2
        lr = (cx - lw / 2, by0 - 0.3 - TEXT_H, cx + lw / 2, by0 - 0.3)
        lab = (x + cx, y + by0 - 0.3 - TEXT_H / 2, 0)
    else:
        cy = (by0 + by1) / 2
        lr = (bx0 - 0.3 - TEXT_H, cy - lw / 2, bx0 - 0.3, cy + lw / 2)
        lab = (x + bx0 - 0.3 - TEXT_H / 2, y + cy, 90)
    rect = (x + min(bx0, lr[0]), y + min(by0, lr[1]), x + max(bx1, lr[2]), y + max(by1, lr[3]))
    return rect, lab


def anchor_rect(parts, ref):
    x, y, rot, side, ext = G.ANCHORS[ref]
    bx0, by0, bx1, by1 = parts[ref]['rots'][str(rot)]['bbox']
    if side == 'B':
        bx0, bx1 = -bx1, -bx0
    return (x + bx0 - ext[0], y + by0 - ext[1], x + bx1 + ext[2], y + by1 + ext[3])


def overlap(a, b):
    ox = min(a[2], b[2]) - max(a[0], b[0]) + MARGIN
    oy = min(a[3], b[3]) - max(a[1], b[1]) + MARGIN
    return ox * oy if ox > 0 and oy > 0 else 0.0


def overflow(rect):
    best = None
    for reg in (G.REGION_A, G.REGION_B):
        o = max(0, reg[0] - rect[0]) + max(0, reg[1] - rect[1]) + max(0, rect[2] - reg[2]) + max(0, rect[3] - reg[3])
        best = o if best is None else min(best, o)
    return best


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--seed', type=int, default=1)
    ap.add_argument('--iters', type=int, default=500000)
    args = ap.parse_args()
    parts, nets = load()
    free = [r for r in sorted(parts) if r not in G.ANCHORS]
    labels = {r: label_text(r, parts[r]['value']) for r in free}

    obst = [anchor_rect(parts, r) for r in G.ANCHORS]
    obst += [(hx - 3.7, hy - 3.7, hx + 3.7, hy + 3.7) for hx, hy in G.HOLES]
    obst.append(G.ANT)

    # pozycje padów kotwic
    anchor_pads = {}
    for r, (x, y, rot, side, ext) in G.ANCHORS.items():
        for num, lst in parts[r]['rots'][str(rot)]['pads'].items():
            anchor_pads[(r, num)] = [(x + (-px if side == 'B' else px), y + py) for px, py in lst]

    # sieci: lista (waga, [(ref, pin)...]) tylko te, w których jest swobodny element
    netlist = []
    for name, nodes in nets.items():
        if name in SKIP_NETS:
            continue
        refs = [(r, p) for r, p in nodes]
        if any(r in free for r, _ in refs) and len(refs) >= 2:
            netlist.append((NET_W.get(name, 1.0), refs))
    for r1, p1, r2, p2, w in PAIRS:
        netlist.append((w, [(r1, p1), (r2, p2)]))
    part_nets = {r: [] for r in free}
    for i, (w, refs) in enumerate(netlist):
        for r in {x for x, _ in refs}:
            if r in part_nets:
                part_nets[r].append(i)

    rng = random.Random(args.seed)
    state = {}
    for r in free:
        reg = G.REGION_A
        state[r] = [round(rng.uniform(reg[0] + 5, reg[2] - 5) / GRID) * GRID, round(rng.uniform(reg[1] + 5, reg[3] - 5) / GRID) * GRID, rng.choice([0, 90, 180, 270])]

    cache = {}

    def rect_of(r, st):
        k = (r, st[0], st[1], st[2])
        if k not in cache:
            if len(cache) > 400000:
                cache.clear()
            cache[k] = part_rect(parts[r], labels[r], st[0], st[1], st[2])
        return cache[k]

    def pad_xy(r, pin, st):
        if r in state:
            o = parts[r]['rots'][str(st[r][2])]['pads'][pin]
            return [(st[r][0] + px, st[r][1] + py) for px, py in o]
        return anchor_pads[(r, pin)]

    def net_cost(i, st):
        w, refs = netlist[i]
        xs, ys = [], []
        for r, p in refs:
            for x, y in pad_xy(r, p, st):
                xs.append(x)
                ys.append(y)
        return w * ((max(xs) - min(xs)) + (max(ys) - min(ys)))

    def part_cost(r, st):
        c = sum(net_cost(i, st) for i in part_nets[r])
        rect = rect_of(r, st[r])[0]
        pen = overflow(rect) * 50.0
        for o in obst:
            pen += overlap(rect, o) * 30.0
        for q in free:
            if q != r:
                pen += overlap(rect, rect_of(q, st[q])[0]) * 30.0
        return c + pen

    def total(st):
        t = 0.0
        for i in range(len(netlist)):
            t += net_cost(i, st)
        pen = 0.0
        for r in free:
            rect = rect_of(r, st[r])[0]
            pen += overflow(rect) * 50.0
            for o in obst:
                pen += overlap(rect, o) * 30.0
        for a in range(len(free)):
            for b in range(a + 1, len(free)):
                pen += overlap(rect_of(free[a], st[free[a]])[0], rect_of(free[b], st[free[b]])[0]) * 30.0
        return t, pen

    T0, T1 = 8.0, 0.05
    for it in range(args.iters):
        T = T0 * (T1 / T0) ** (it / args.iters)
        r = rng.choice(free)
        old = list(state[r])
        before = part_cost(r, state)
        m = rng.random()
        if m < 0.55:
            s = max(1.0, 12.0 * (1 - it / args.iters))
            state[r][0] = round((old[0] + rng.gauss(0, s)) / GRID) * GRID
            state[r][1] = round((old[1] + rng.gauss(0, s)) / GRID) * GRID
        elif m < 0.8:
            state[r][2] = rng.choice([0, 90, 180, 270])
        else:
            reg = rng.choice([G.REGION_A, G.REGION_A, G.REGION_B])
            state[r][0] = round(rng.uniform(reg[0], reg[2]) / GRID) * GRID
            state[r][1] = round(rng.uniform(reg[1], reg[3]) / GRID) * GRID
        after = part_cost(r, state)
        d = after - before
        if d > 0 and rng.random() >= math.exp(-d / T):
            state[r] = old
        if it % 100000 == 0:
            t, pen = total(state)
            print('iter %d  T=%.2f  długość=%.0f  kary=%.1f' % (it, T, t, pen), flush=True)
    t, pen = total(state)
    print('koniec: długość=%.0f  kary=%.2f' % (t, pen))

    # lista konfliktów
    bad = []
    for r in free:
        rect = rect_of(r, state[r])[0]
        if overflow(rect) > 0.01:
            bad.append((r, 'poza strefą'))
        for k, o in enumerate(obst):
            if overlap(rect, o) > 0.01:
                bad.append((r, 'kotwica %d' % k))
        for q in free:
            if q > r and overlap(rect, rect_of(q, state[q])[0]) > 0.01:
                bad.append((r, q))
    print('konflikty:', bad)
    out = {}
    for r in free:
        rect, lab = part_rect(parts[r], labels[r], *state[r])
        out[r] = {'x': state[r][0], 'y': state[r][1], 'rot': state[r][2], 'label': labels[r],
                  'lx': lab[0], 'ly': lab[1], 'lang': lab[2]}
    json.dump({'seed': args.seed, 'conflicts': len(bad), 'parts': out}, open(os.path.join(HERE, 'layout.json'), 'w'), indent=1)
    free_area = sum((r[2] - r[0]) * (r[3] - r[1]) for r in (G.REGION_A, G.REGION_B))
    used = sum((rect_of(r, state[r])[0][2] - rect_of(r, state[r])[0][0]) * (rect_of(r, state[r])[0][3] - rect_of(r, state[r])[0][1]) for r in free)
    print('powierzchnia elementów %.0f mm2 z %.0f mm2 stref (z kotwicami niedostępnymi)' % (used, free_area))


main()
