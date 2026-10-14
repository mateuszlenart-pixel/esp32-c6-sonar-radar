#!/bin/bash
# Rendery 3D i PDF z wymiarami do weryfikacji (wynik w hardware/pcb/renders/, katalog ignorowany).
# Uruchamiać z katalogu repo po build_all.sh.
set -e
PCB=$(pwd)/hardware/pcb
O=$PCB/renders
mkdir -p $O
K="flatpak run --command=kicad-cli --filesystem=host org.kicad.KiCad"
cd $PCB
for side in top bottom; do
  $K pcb render --side $side --quality high --width 1400 --height 2000 --background opaque \
    --zoom 0.9 --output $O/plytka-$side.png radar-pcb.kicad_pcb > /dev/null
done
$K pcb export pdf --mode-single --include-border-title --ev --sketch-pads-on-fab-layers \
  --layers "F.Fab,F.Silkscreen,Edge.Cuts,Dwgs.User" --output $O/p1-przod.pdf radar-pcb.kicad_pcb > /dev/null
$K pcb export pdf --mode-single --include-border-title --ev --mirror --sketch-pads-on-fab-layers \
  --layers "B.Fab,B.Silkscreen,Edge.Cuts,Cmts.User" --output $O/p2-tyl.pdf radar-pcb.kicad_pcb > /dev/null
python3 - "$O" <<'PY'
import sys
from PIL import Image, ImageDraw, ImageFont
O = sys.argv[1] + '/'
W, H = 1754, 1240
pg = Image.new('RGB', (W, H), 'white')
d = ImageDraw.Draw(pg)
try:
    f = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf', 26)
    f2 = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf', 20)
except Exception:
    f = f2 = None
d.text((60, 40), 'Radar ESP32-C6 – płytka v0.1 kompaktowa – podgląd 3D (bez modułów i DevKita)', fill='black', font=f)
d.text((60, 80), 'Moduły OLED i MAX7219 na kablach do gniazd J7 i J6, DevKit w J2/J3 od tyłu. '
       'Do pomiarów użyj stron 1–2 (skala 1:1).', fill='black', font=f2)
for i, (fn, cap) in enumerate([('plytka-top.png', 'PRZÓD'), ('plytka-bottom.png', 'TYŁ (widok od tyłu)')]):
    im = Image.open(O + fn).convert('RGB')
    im = im.crop((40, 60, im.width - 40, im.height - 40))
    sc = (H - 200) / im.height
    im = im.resize((int(im.width * sc), int(im.height * sc)))
    x = 120 + i * (W // 2)
    pg.paste(im, (x, 150))
    d.text((x, H - 45), cap, fill='black', font=f)
pg.save(O + 'p3-3d.pdf', resolution=150)
PY
pdfunite $O/p1-przod.pdf $O/p2-tyl.pdf $O/p3-3d.pdf $O/radar-pcb-v0.1-wymiary-do-weryfikacji.pdf
echo "PDF: $O/radar-pcb-v0.1-wymiary-do-weryfikacji.pdf"
