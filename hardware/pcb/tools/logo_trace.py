"""Wektoryzuje logo (biały kształt na czarnym tle) do konturów w mm: tools/logo-contours.json.

Marching squares na obrazie w skali szarości (gładkie krawędzie), uproszczenie Douglasa-Peuckera, kontury
zewnętrzne i otwory (np. wnętrza liter) rozpoznawane po zagnieżdżeniu. Dalej: logo_footprint.py.

Użycie:  python3 hardware/pcb/tools/logo_trace.py hardware/pcb/logo/am-logo-source.jfif [szerokość_mm]
"""
import json
import os
import sys

from PIL import Image, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
PX_MM = 0.02          # rozmiar piksela po przeskalowaniu
DILATE = 1            # pogrubienie o piksele (cienkie linie logo muszą mieć >= 0,15 mm na silkscreenie)
SIMPLIFY = 0.45       # tolerancja uproszczenia konturu, piksele


def trace(grid, w, h, thr=128.0):
    """Kontury poziomu thr; zwraca listę zamkniętych pętli punktów (x, y) w pikselach."""
    segs = {}

    def add(a, b):
        segs.setdefault(a, []).append(b)
        segs.setdefault(b, []).append(a)

    for y in range(h - 1):
        r0, r1 = grid[y], grid[y + 1]
        for x in range(w - 1):
            tl, tr, br, bl = r0[x], r0[x + 1], r1[x + 1], r1[x]
            idx = (8 if tl >= thr else 0) | (4 if tr >= thr else 0) | (2 if br >= thr else 0) | (1 if bl >= thr else 0)
            if idx in (0, 15):
                continue
            T, B, L, R = ('h', x, y), ('h', x, y + 1), ('v', x, y), ('v', x + 1, y)
            pairs = {1: [(L, B)], 2: [(B, R)], 3: [(L, R)], 4: [(T, R)], 5: [(T, L), (B, R)], 6: [(T, B)],
                     7: [(T, L)], 8: [(T, L)], 9: [(T, B)], 10: [(T, R), (L, B)], 11: [(T, R)], 12: [(L, R)],
                     13: [(B, R)], 14: [(L, B)]}[idx]
            for a, b in pairs:
                add(a, b)

    def pos(e):
        k, x, y = e
        if k == 'h':
            f1, f2 = grid[y][x], grid[y][x + 1]
            return (x + (thr - f1) / (f2 - f1), float(y))
        f1, f2 = grid[y][x], grid[y + 1][x]
        return (float(x), y + (thr - f1) / (f2 - f1))

    loops, seen = [], set()
    for start in list(segs):
        if start in seen:
            continue
        loop, prev, cur = [], None, start
        while cur not in seen:
            seen.add(cur)
            loop.append(pos(cur))
            nxt = [n for n in segs[cur] if n != prev and n not in seen]
            if not nxt:
                break
            prev, cur = cur, nxt[0]
        if len(loop) >= 4:
            loops.append(loop)
    return loops


def dp(points, eps):
    """Douglas-Peucker dla pętli zamkniętej (dzielimy w najdalszym punkcie)."""
    def rdp(pts):
        if len(pts) < 3:
            return pts
        (x1, y1), (x2, y2) = pts[0], pts[-1]
        dx, dy = x2 - x1, y2 - y1
        n = (dx * dx + dy * dy) ** 0.5 or 1e-9
        best, bi = -1, 0
        for i in range(1, len(pts) - 1):
            d = abs(dy * (pts[i][0] - x1) - dx * (pts[i][1] - y1)) / n
            if d > best:
                best, bi = d, i
        if best > eps:
            return rdp(pts[:bi + 1])[:-1] + rdp(pts[bi:])
        return [pts[0], pts[-1]]
    far = max(range(len(points)), key=lambda i: (points[i][0] - points[0][0]) ** 2 + (points[i][1] - points[0][1]) ** 2)
    a = rdp(points[:far + 1])
    b = rdp(points[far:] + [points[0]])
    return a[:-1] + b[:-1]


def area(p):
    return sum(p[i][0] * p[(i + 1) % len(p)][1] - p[(i + 1) % len(p)][0] * p[i][1] for i in range(len(p))) / 2


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


def main(path, width_mm=22.0):
    im = Image.open(path).convert('L')
    box = im.point(lambda v: 255 if v > 128 else 0).getbbox()
    im = im.crop(box)
    w_px = int(round(width_mm / PX_MM))
    h_px = int(round(im.height * w_px / im.width))
    im = im.resize((w_px, h_px), Image.LANCZOS)
    pad = 3
    canvas = Image.new('L', (w_px + 2 * pad, h_px + 2 * pad), 0)
    canvas.paste(im, (pad, pad))
    for _ in range(DILATE):
        canvas = canvas.filter(ImageFilter.MaxFilter(3))
    W, H = canvas.size
    data = list(canvas.getdata())
    grid = [[float(v) for v in data[y * W:(y + 1) * W]] for y in range(H)]
    loops = [dp(l, SIMPLIFY) for l in trace(grid, W, H)]
    loops = [l for l in loops if len(l) >= 3 and abs(area(l)) > 4]
    # zagnieżdżenie: pętla w nieparzystej liczbie innych to otwór
    info = []
    for i, l in enumerate(loops):
        depth = sum(1 for j, o in enumerate(loops) if j != i and inside(l[0], o))
        info.append(depth)
    out = {'w': round((W - 2 * pad) * PX_MM, 3), 'h': round((H - 2 * pad) * PX_MM, 3), 'loops': []}
    for l, depth in zip(loops, info):
        pts = [[round((x - pad) * PX_MM, 4), round((y - pad) * PX_MM, 4)] for x, y in l]
        out['loops'].append({'hole': depth % 2 == 1, 'pts': pts})
    json.dump(out, open(os.path.join(HERE, 'logo-contours.json'), 'w'))
    print('logo %.1f x %.1f mm: %d konturów (%d otworów), %d punktów' % (
        out['w'], out['h'], len(loops), sum(1 for d in info if d % 2), sum(len(l) for l in loops)))


if __name__ == '__main__':
    main(sys.argv[1], float(sys.argv[2]) if len(sys.argv) > 2 else 22.0)
