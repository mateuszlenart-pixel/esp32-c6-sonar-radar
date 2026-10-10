#!/bin/bash
# Pełna regeneracja: schemat -> ERC -> netlista -> płytka -> Freerouting -> wylewki -> DRC.
# Uruchamiać z katalogu repo. Wymaga: Flatpak org.kicad.KiCad, ~/tools/java25 i ~/tools/freerouting.
set -e
REPO=$(pwd)
PCB=$REPO/hardware/pcb
KPY="flatpak run --command=python3 --filesystem=host org.kicad.KiCad"
KCLI="flatpak run --command=kicad-cli --filesystem=host org.kicad.KiCad"
JAVA=$(ls -d ~/tools/java25/*/bin)/java
$KPY $PCB/tools/gen_schematic.py
(cd $PCB && $KCLI sch erc --severity-error --exit-code-violations --output erc.rpt radar-pcb.kicad_sch)
(cd $PCB && $KCLI sch export netlist --format kicadxml --output radar-pcb.net radar-pcb.kicad_sch)
$KPY $PCB/tools/gen_pcb.py place
$JAVA -Djava.awt.headless=true -jar ~/tools/freerouting/freerouting-2.5.0.jar \
  -de $PCB/radar-pcb.dsn -do $PCB/radar-pcb.ses -mp 40 > $PCB/freerouting.log 2>&1
grep -E "Auto-routing stage completed" $PCB/freerouting.log | sed 's/.*completed/routing completed/'
$KPY $PCB/tools/gen_pcb.py finish
(cd $PCB && $KCLI pcb drc --severity-all --schematic-parity --format report --output drc.rpt radar-pcb.kicad_pcb)
