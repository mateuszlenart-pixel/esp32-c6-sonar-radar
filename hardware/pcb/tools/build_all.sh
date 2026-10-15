#!/bin/bash
# Pełna regeneracja: schemat -> ERC -> netlista -> rozmieszczenie -> płytka -> Freerouting -> wylewki -> DRC.
# Uruchamiać z katalogu repo. Wymaga: Flatpak org.kicad.KiCad, ~/tools/java25 i ~/tools/freerouting.
# Rozmieszczenie (autoplace.py) trwa kilka minut; SKIP_PLACE=1 używa zapisanego tools/layout.json.
set -e
REPO=$(pwd)
PCB=$REPO/hardware/pcb
KPY="flatpak run --command=python3 --filesystem=host org.kicad.KiCad"
KCLI="flatpak run --command=kicad-cli --filesystem=host org.kicad.KiCad"
JAVA=$(ls -d ~/tools/java25/*/bin)/java
$KPY $PCB/tools/gen_schematic.py
(cd $PCB && $KCLI sch erc --severity-error --exit-code-violations --output erc.rpt radar-pcb.kicad_sch)
(cd $PCB && $KCLI sch export netlist --format kicadxml --output radar-pcb.net radar-pcb.kicad_sch)
# logo (silkscreen): wektoryzacja logo/am-logo-source.jfif i footprint logo/am-logo.kicad_mod (bez pliku: bez logo)
if [ -f $PCB/logo/am-logo-source.jfif ]; then
  python3 $PCB/tools/logo_trace.py $PCB/logo/am-logo-source.jfif 22
  $KPY $PCB/tools/logo_footprint.py
fi
if [ -z "$SKIP_PLACE" ]; then
  $KPY $PCB/tools/fpinfo.py
  python3 $PCB/tools/autoplace.py --seed ${SEED:-3} --iters ${ITERS:-500000}
else
  $KPY $PCB/tools/fpinfo.py
fi
$KPY $PCB/tools/gen_pcb.py place
$JAVA -Djava.awt.headless=true -jar ~/tools/freerouting/freerouting-2.5.0.jar \
  -de $PCB/radar-pcb.dsn -do $PCB/radar-pcb.ses -mp 40 > $PCB/freerouting.log 2>&1
grep -E "Auto-routing stage completed" $PCB/freerouting.log | sed 's/.*completed/routing completed/'
$KPY $PCB/tools/gen_pcb.py finish
(cd $PCB && $KCLI pcb drc --severity-all --schematic-parity --format report --output drc.rpt radar-pcb.kicad_pcb)
