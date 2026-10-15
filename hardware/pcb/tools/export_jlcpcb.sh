#!/bin/bash
# Pliki do zamówienia gołych płytek (JLCPCB): Gerbery + wiercenia (zip) i lista zakupów elementów
# do ręcznego lutowania, w hardware/pcb/production/. Uruchamiać z katalogu repo po build_all.sh.
set -e
PCB=$(pwd)/hardware/pcb
O=$PCB/production
mkdir -p $O/gerber
K="flatpak run --command=kicad-cli --filesystem=host org.kicad.KiCad"
cd $PCB
$K pcb export gerbers --use-drill-file-origin --no-x2 --subtract-soldermask \
  --layers "F.Cu,B.Cu,F.Silkscreen,B.Silkscreen,F.Mask,B.Mask,Edge.Cuts" \
  --output $O/gerber/ radar-pcb.kicad_pcb > /dev/null
$K pcb export drill --drill-origin plot --excellon-separate-th --format excellon --excellon-units mm \
  --generate-map --map-format gerberx2 --output $O/gerber/ radar-pcb.kicad_pcb > /dev/null
python3 $PCB/tools/hand_bom.py $O
(cd $O/gerber && python3 - <<'PY'
import glob, zipfile
with zipfile.ZipFile('../gerber-jlcpcb.zip', 'w', zipfile.ZIP_DEFLATED) as z:
    for f in sorted(glob.glob('*')):
        if 'Paste' in f:   # wersja przewlekana nie ma montażu SMD, pasta niepotrzebna
            continue
        z.write(f)
        print(' ', f)
PY
)
echo "Gotowe: $O/gerber-jlcpcb.zip i $O/lista-zakupow.csv"
