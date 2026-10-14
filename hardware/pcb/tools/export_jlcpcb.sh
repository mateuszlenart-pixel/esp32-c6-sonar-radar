#!/bin/bash
# Pliki produkcyjne dla JLCPCB: Gerbery + wiercenia (zip), BOM i CPL w hardware/pcb/production/.
# Uruchamiać z katalogu repo po build_all.sh. Wymaga Flatpak org.kicad.KiCad.
set -e
PCB=$(pwd)/hardware/pcb
O=$PCB/production
mkdir -p $O/gerber
K="flatpak run --command=kicad-cli --filesystem=host org.kicad.KiCad"
cd $PCB
$K pcb export gerbers --use-drill-file-origin --no-x2 --subtract-soldermask \
  --layers "F.Cu,B.Cu,F.Paste,B.Paste,F.Silkscreen,B.Silkscreen,F.Mask,B.Mask,Edge.Cuts" \
  --output $O/gerber/ radar-pcb.kicad_pcb > /dev/null
$K pcb export drill --drill-origin plot --excellon-separate-th --format excellon --excellon-units mm \
  --generate-map --map-format gerberx2 --output $O/gerber/ radar-pcb.kicad_pcb > /dev/null
$K pcb export pos --use-drill-file-origin --format csv --units mm --side front --exclude-dnp \
  --output $O/pos-kicad.csv radar-pcb.kicad_pcb > /dev/null
python3 $PCB/tools/jlcpcb_bom.py $O/pos-kicad.csv $O
(cd $O/gerber && python3 - <<'PY'
import glob, zipfile
with zipfile.ZipFile('../gerber-jlcpcb.zip', 'w', zipfile.ZIP_DEFLATED) as z:
    for f in sorted(glob.glob('*')):
        z.write(f)
        print(' ', f)
PY
)
echo "Gotowe: $O/gerber-jlcpcb.zip, $O/bom-jlcpcb.csv, $O/cpl-jlcpcb.csv"
