"""Generuje schemat radar-pcb.kicad_sch (KiCad 10) z opisu w tym pliku.

Połączenia realizują etykiety globalne na końcach pinów (bez przewodów), a każdy pin
jest albo podłączony do nazwanej sieci, albo oznaczony jako niepodłączony. Skrypt
sprawdza, że żaden pin nie został pominięty.

Uruchomienie z katalogu repo:
  flatpak run --command=python3 --filesystem=host org.kicad.KiCad hardware/pcb/tools/gen_schematic.py
"""
import os
import sys
import uuid

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import kilib

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'radar-pcb.kicad_sch')
ROOT = str(uuid.uuid4())
PROJECT = 'radar-pcb'
LIBS = {}      # lib_id -> tekst symbolu
PINS = {}      # lib_id -> lista pinów
items = []     # elementy schematu (tekst)
errors = []
FOOTPRINTS = set()


def uid():
    return str(uuid.uuid4())


def need(libfile, name):
    libid = '%s:%s' % (libfile, name)
    if libid not in LIBS:
        t = kilib.flat(libfile, name)
        t = t.replace('(symbol "%s"' % name, '(symbol "%s"' % libid, 1)
        LIBS[libid] = t
        PINS[libid] = kilib.pins(t, name)
    return libid


def fmt(v):
    return ('%.2f' % v).rstrip('0').rstrip('.')


def snap(v):
    return round(v / 2.54) * 2.54


def outward(angle):
    return (angle + 180) % 360


def label(name, x, y, angle):
    just = 'right' if angle == 180 else 'left'
    items.append(
        '(global_label "%s" (shape passive) (at %s %s %d) (effects (font (size 1.27 1.27)) (justify %s)) '
        '(uuid "%s") (property "Intersheetrefs" "${INTERSHEET_REFS}" (at %s %s 0) '
        '(effects (font (size 1.27 1.27)) (hide yes))))' % (name, fmt(x), fmt(y), angle, just, uid(), fmt(x), fmt(y)))


def noconn(x, y):
    items.append('(no_connect (at %s %s) (uuid "%s"))' % (fmt(x), fmt(y), uid()))


def place(ref, libfile, sym, value, footprint, x, y, nets, nc=(), unit=1, show_value=True, in_bom=True):
    """Umieszcza element; nets: {numer_pinu: sieć}; nc: numery pinów niepodłączonych."""
    x, y = snap(x), snap(y)
    FOOTPRINTS.add(footprint)
    lid = need(libfile, sym)
    pins = [p for p in PINS[lid] if p[6] in (0, unit)]
    seen = set(nets) | set(nc)
    for p in pins:
        if p[0] not in seen:
            errors.append('%s: pin %s (%s) nie ma sieci ani NC' % (ref, p[0], p[1]))
    for n in seen:
        if n not in [p[0] for p in pins]:
            errors.append('%s: nie ma pinu %s w symbolu %s (jednostka %d)' % (ref, n, sym, unit))
    maxpx = max([p[2] for p in pins] + [1.27])
    fields = (
        '(property "Reference" "%s" (at %s %s 0) (effects (font (size 1.27 1.27)) (justify left)))\n'
        '    (property "Value" "%s" (at %s %s 0) (effects (font (size 1.27 1.27)) (justify left)%s))\n'
        '    (property "Footprint" "%s" (at %s %s 0) (effects (font (size 1.27 1.27)) (hide yes)))\n'
        '    (property "Datasheet" "~" (at %s %s 0) (effects (font (size 1.27 1.27)) (hide yes)))'
        % (ref, fmt(x + maxpx + 2), fmt(y - 1.27), value, fmt(x + maxpx + 2), fmt(y + 1.27),
           '' if show_value else ' (hide yes)', footprint, fmt(x), fmt(y), fmt(x), fmt(y)))
    pin_lines = ' '.join('(pin "%s" (uuid "%s"))' % (p[0], uid()) for p in pins)
    items.append(
        '(symbol (lib_id "%s") (at %s %s 0) (unit %d) (exclude_from_sim no) (in_bom %s) (on_board yes) (dnp no) '
        '(uuid "%s")\n    %s\n    %s\n    (instances (project "%s" (path "/%s" (reference "%s") (unit %d)))))'
        % (lid, fmt(x), fmt(y), unit, 'yes' if in_bom else 'no', uid(), fields, pin_lines, PROJECT, ROOT, ref, unit))
    for p in pins:
        px, py = x + p[2], y - p[3]
        if p[0] in nets:
            label(nets[p[0]], px, py, outward(p[4]))
        elif p[0] in nc:
            noconn(px, py)


def flag(x, y, net, ref):
    x, y = snap(x), snap(y)
    lid = need('power', 'PWR_FLAG')
    items.append(
        '(symbol (lib_id "%s") (at %s %s 0) (unit 1) (exclude_from_sim no) (in_bom no) (on_board no) (dnp no) '
        '(uuid "%s")\n    (property "Reference" "%s" (at %s %s 0) (effects (font (size 1.27 1.27)) (hide yes)))\n'
        '    (property "Value" "PWR_FLAG" (at %s %s 0) (effects (font (size 1.27 1.27)) (hide yes)))\n'
        '    (pin "1" (uuid "%s"))\n    (instances (project "%s" (path "/%s" (reference "%s") (unit 1)))))'
        % (lid, fmt(x), fmt(y), uid(), ref, fmt(x), fmt(y - 2), fmt(x), fmt(y - 4), uid(), PROJECT, ROOT, ref))
    label(net, x, y, 90)


def text(x, y, s, size=2.0):
    items.append('(text "%s" (at %s %s 0) (effects (font (size %s %s) (bold yes)) (justify left)) (uuid "%s"))'
                 % (s, fmt(x), fmt(y), size, size, uid()))


R = 'Resistor_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P7.62mm_Horizontal'
C = 'Capacitor_THT:C_Disc_D5.0mm_W2.5mm_P5.00mm'
CP8 = 'Capacitor_THT:CP_Radial_D8.0mm_P3.50mm'
CP63 = 'Capacitor_THT:CP_Radial_D6.3mm_P2.50mm'
HDR = 'Connector_PinSocket_2.54mm:PinSocket_1x%02d_P2.54mm_Vertical'   # wszystkie listwy na płytce są żeńskie
HDR2 = 'Connector_PinHeader_2.54mm:PinHeader_2x%02d_P2.54mm_Vertical'
SKT = 'Connector_PinSocket_2.54mm:PinSocket_1x%02d_P2.54mm_Vertical'
TP = 'TestPoint:TestPoint_THTPad_D2.0mm_Drill1.0mm'
DIODE_S = 'Diode_THT:D_DO-35_SOD27_P7.62mm_Horizontal'      # 1N4148
DIODE_P = 'Diode_THT:D_DO-41_SOD81_P10.16mm_Horizontal'     # 1N5819
DIODE_T = 'Diode_THT:D_DO-15_P10.16mm_Horizontal'           # TVS SA5.0A

# ------------------------------------------------------------------ zasilanie
text(30, 30, 'ZASILANIE 5 V: gniazdo DC 5,5/2,1 mm (srodek +), bezpiecznik PTC i dioda TVS (zwiera przy przepieciu i odwroconej polaryzacji, PTC odcina)')
place('J1', 'Connector', 'Barrel_Jack_Switch', 'Gniazdo DC 5,5/2,1 mm, srodek +5 V',
      'Connector_BarrelJack:BarrelJack_Horizontal', 40, 60, {'1': 'VIN', '2': 'GND'}, nc=['3'])
place('F1', 'Device', 'Polyfuse', 'PTC 2 A (hold), rastr 5 mm', 'Capacitor_THT:C_Disc_D7.5mm_W5.0mm_P5.00mm', 75, 60,
      {'1': 'VIN', '2': '+5V'})
place('D3', 'Device', 'D_Zener', 'TVS SA5.0A (DO-15, katoda do +5V)', DIODE_T, 100, 85, {'1': '+5V', '2': 'GND'})
flag(115, 50, 'VIN', '#FLG04')
flag(130, 50, '+5V', '#FLG05')
place('C1', 'Device', 'C_Polarized', '470 uF / 16 V', CP8, 150, 60, {'1': '+5V', '2': 'GND'})
place('C2', 'Device', 'C', '100 nF', C, 175, 60, {'1': '+5V', '2': 'GND'})
place('R1', 'Device', 'R', '1 k', R, 200, 60, {'1': '+5V', '2': 'LED_A'})
place('D1', 'Device', 'LED', 'LED zasilania 3 mm', 'LED_THT:LED_D3.0mm', 225, 75, {'1': 'GND', '2': 'LED_A'})
flag(262, 50, 'GND', '#FLG02')
flag(274, 50, '+3V3', '#FLG03')
for i, (ref, net) in enumerate([('TP1', '+5V'), ('TP2', '+3V3'), ('TP3', 'GND'), ('TP4', 'TRIG_5V'), ('TP5', 'ECHO_3V3')]):
    place(ref, 'Connector', 'TestPoint', 'TP ' + net, TP, 40 + i * 22, 100, {'1': net}, show_value=False, in_bom=False)

# ------------------------------------------------------------------ bufor AHCT125
text(30, 130, 'BUFOR SN74AHCT125N (DIP-14 w podstawce): 3,3 V z ESP32 -> 5 V; wejscia A z pull-downami')
U = ('74xx', '74AHCT125', 'SN74AHCT125N (DIP-14)', 'Package_DIP:DIP-14_W7.62mm_Socket')
gates = [(1, 80, 160, '1', '2', '3', 'TRIG'), (2, 80, 205, '4', '5', '6', 'DIN'),
         (3, 150, 160, '10', '9', '8', 'CLK'), (4, 150, 205, '13', '12', '11', 'CS')]
for unit, x, y, oe, a, yy, sig in gates:
    place('U1', U[0], U[1], U[2], U[3], x, y, {oe: 'GND', a: sig + '_3V3', yy: sig + '_5V'}, unit=unit)
place('U1', U[0], U[1], U[2], U[3], 215, 190, {'7': 'GND', '14': '+5V'}, unit=5)
place('C3', 'Device', 'C', '100 nF', C, 245, 190, {'1': '+5V', '2': 'GND'})
for i, sig in enumerate(['TRIG', 'DIN', 'CLK', 'CS']):
    # CS (spoczynkowo wysoki w firmware) ma pull-up do 3,3 V, pozostale linie pull-down do masy
    place('R%d' % (2 + i), 'Device', 'R', '47 k', R, 40 + (i % 2) * 70, 160 + (i // 2) * 45,
          {'1': sig + '_3V3', '2': '+3V3' if sig == 'CS' else 'GND'})
text(260, 150, 'DZIELNIK ECHO 5 V -> 3,1 V')
place('R6', 'Device', 'R', '10 k', R, 270, 175, {'1': 'ECHO_5V', '2': 'ECHO_3V3'})
place('R7', 'Device', 'R', '15 k', R, 300, 175, {'1': 'ECHO_3V3', '2': 'GND'})

# ------------------------------------------------------------------ ESP32 DevKitC-1
text(30, 226, 'ESP32-C6-DevKitC-1 (gniazda zenskie 1x16). Wszystkie piny sa wyprowadzone na listwy J11 (lustro J3) i J12 (lustro J2). Pin 5V DevKita zasilany z +5V przez D4.')
J2_NETS = {'1': '+3V3', '2': 'EN', '3': 'IO4', '4': 'IO5', '5': 'ECHO_3V3', '6': 'DIN_3V3', '7': 'V5_SENSE', '8': 'IO1',
           '9': 'IO8', '10': 'SERVO_3V3', '11': 'TRIG_3V3', '12': 'BTN_ESP', '13': 'IO3', '14': 'DEVKIT_5V', '15': 'GND'}
J3_NETS = {'1': 'GND', '2': 'TX', '3': 'RX', '4': 'IO15', '5': 'IO23', '6': 'BUZ_CTRL', '7': 'SCL', '8': 'SDA',
           '9': 'CS_3V3', '10': 'CLK_3V3', '11': 'BOOT', '12': 'GND', '13': 'IO13', '14': 'IO12', '15': 'GND'}
place('J2', 'Connector_Generic', 'Conn_01x16', 'DevKit J1', SKT % 16, 40, 262, J2_NETS, nc=['16'])
place('J3', 'Connector_Generic', 'Conn_01x16', 'DevKit J3', SKT % 16, 120, 262, J3_NETS, nc=['16'])
# listwy z kopia wszystkich sygnalow DevKita (do dalszych modyfikacji); pin 14 listwy J12 = szyna +5V plytki
J12_NETS = dict(J2_NETS)
J12_NETS['14'] = '+5V'
place('J12', 'Connector_Generic', 'Conn_01x16', 'Piny DevKit J1 (kopia J2)', HDR % 16, 200, 262, J12_NETS, nc=['16'])
place('J11', 'Connector_Generic', 'Conn_01x16', 'Piny DevKit J3 (kopia J3)', HDR % 16, 260, 262, dict(J3_NETS), nc=['16'])

# ------------------------------------------------------------------ moduly i zlacza
text(330, 30, 'MODULY I ZLACZA')
place('J4', 'Connector_Generic', 'Conn_01x04', 'HC-SR04 (kabel): VCC TRIG ECHO GND', HDR % 4, 340, 55,
      {'1': 'SR04_VCC', '2': 'TRIG_5V', '3': 'ECHO_5V', '4': 'GND'})
# filtr RC zasilania sonaru: oddziela odbiornik HC-SR04 od skoków prądu matrycy MAX7219
place('R11', 'Device', 'R', '10', R, 370, 40, {'1': '+5V', '2': 'SR04_VCC'})
place('C6', 'Device', 'C_Polarized', '100 uF / 16 V', CP63, 385, 45, {'1': 'SR04_VCC', '2': 'GND'})
place('C7', 'Device', 'C', '100 nF', C, 400, 45, {'1': 'SR04_VCC', '2': 'GND'})
place('J5', 'Connector_Generic', 'Conn_01x03', 'Serwo SG90 (kabel): GND +5V SIG', HDR % 3, 340, 95,
      {'1': 'GND', '2': 'SERVO_5V', '3': 'SERVO_SIG'})
# zasilanie serwa przez miejsce na ferryt (montowac zworke 0 R albo ferryt >= 1 A): silnik nie zasmieca
# szyny 5 V sonaru, bufora i matrycy; C4 + C10 przy zlaczu zasilaja impulsy pradu serwa
place('FB1', 'Device', 'R', '0 R (zworka z drutu; opcja: ferryt >= 1 A)', R, 300, 75,
      {'1': '+5V', '2': 'SERVO_5V'})
place('C10', 'Device', 'C', '100 nF', C, 320, 75, {'1': 'SERVO_5V', '2': 'GND'})
# rezystor ciagnie sygnal serwa do masy, gdy GPIO10 jest nieukonczone (reset, start): brak drgan serwa
place('R15', 'Device', 'R', '10 k', R, 320, 105, {'1': 'SERVO_SIG', '2': 'GND'})
place('R8', 'Device', 'R', '330', R, 300, 95, {'1': 'SERVO_3V3', '2': 'SERVO_SIG'})
place('C4', 'Device', 'C_Polarized', '220 uF / 10 V', CP63, 280, 95, {'1': 'SERVO_5V', '2': 'GND'})
place('J6', 'Connector_Generic', 'Conn_01x05', 'MAX7219 Elegoo: VCC GND DIN CS CLK', SKT % 5, 340, 130,
      {'1': '+5V', '2': 'GND', '3': 'DIN_J', '4': 'CS_J', '5': 'CLK_J'})
# rezystory szeregowe tłumiące odbicia na liniach do matrycy, kondensatory przy module
place('R12', 'Device', 'R', '100', R, 370, 115, {'1': 'DIN_5V', '2': 'DIN_J'})
place('R13', 'Device', 'R', '100', R, 385, 115, {'1': 'CLK_5V', '2': 'CLK_J'})
place('R14', 'Device', 'R', '100', R, 400, 115, {'1': 'CS_5V', '2': 'CS_J'})
place('C8', 'Device', 'C_Polarized', '100 uF / 16 V', CP63, 370, 140, {'1': '+5V', '2': 'GND'})
place('C9', 'Device', 'C', '100 nF', C, 385, 140, {'1': '+5V', '2': 'GND'})
place('J7', 'Connector_Generic', 'Conn_01x04', 'OLED SH1106: VCC GND SCK SDA', SKT % 4, 340, 165,
      {'1': '+3V3', '2': 'GND', '3': 'SCL', '4': 'SDA'})
text(260, 200, 'BUZZER (aktywny lub pasywny, 5 V): tranzystor 2N3904 + dioda')
place('R9', 'Device', 'R', '1 k', R, 270, 225, {'1': 'BUZ_CTRL', '2': 'BUZ_BASE'})
place('R10', 'Device', 'R', '100 k', R, 295, 225, {'1': 'BUZ_BASE', '2': 'GND'})
place('Q2', 'Transistor_BJT', '2N3904', '2N3904 (TO-92, E B C)', 'Package_TO_SOT_THT:TO-92_Inline', 330, 225,
      {'1': 'GND', '2': 'BUZ_BASE', '3': 'BUZ_NEG'})
place('D2', 'Device', 'D', '1N4148', DIODE_S, 365, 225, {'1': '+5V', '2': 'BUZ_NEG'})
place('J8', 'Connector_Generic', 'Conn_01x02', 'Buzzer: + -', HDR % 2, 395, 215, {'1': '+5V', '2': 'BUZ_NEG'})
# odsprzezenie zasilania sterownika buzzera: przelaczanie 2 kHz nie moze wchodzic w szyne 5 V
place('C11', 'Device', 'C', '100 nF', C, 420, 215, {'1': '+5V', '2': 'GND'})
# miejsce na kondensator rownolegle do buzzera (tlumienie iskrzenia/EMI), opcjonalny
place('C12', 'Device', 'C', '100 nF (opcja)', C, 440, 215, {'1': '+5V', '2': 'BUZ_NEG'})
text(180, 240, 'WYLACZNIK ON/STANDBY (GPIO2): przycisk do GND, pull-up w ESP32')
place('SW1', 'Switch', 'SW_Push', 'Przycisk ON/standby (katowy)', 'Button_Switch_THT:SW_Tactile_SPST_Angled_PTS645Vx58-2LFS', 195, 265,
      {'1': 'BTN', '2': 'GND'})
place('C5', 'Device', 'C', '100 nF', C, 235, 265, {'1': 'BTN_ESP', '2': 'GND'})
place('J9', 'Connector_Generic', 'Conn_01x02', 'Przycisk zewnetrzny', HDR % 2, 270, 262, {'1': 'BTN', '2': 'GND'})
# kabel do przycisku na panelu: rezystor szeregowy (ESD) i zewnetrzny pull-up po stronie kabla
place('R18', 'Device', 'R', '1 k', R, 300, 262, {'1': 'BTN', '2': 'BTN_ESP'})
place('R19', 'Device', 'R', '10 k', R, 320, 262, {'1': 'BTN', '2': '+3V3'})

# --- dodatki z audytu (2026-10-11)
text(30, 300, 'DODATKI: dioda 5V DevKita, pomiar napiecia 5 V (ADC), BOOT/RST, pull-upy I2C (opcja), listwa zasilania, pole prototypowe')
place('D4', 'Device', 'D_Schottky', '1N5819', DIODE_P, 40, 320, {'1': 'DEVKIT_5V', '2': '+5V'})
place('R20', 'Device', 'R', '100 k', R, 70, 320, {'1': '+5V', '2': 'V5_SENSE'})
place('R21', 'Device', 'R', '47 k', R, 90, 320, {'1': 'V5_SENSE', '2': 'GND'})
place('C13', 'Device', 'C', '100 nF', C, 110, 320, {'1': 'V5_SENSE', '2': 'GND'})
place('J10', 'Connector_Generic', 'Conn_01x03', 'Awaryjne wgrywanie: BOOT GND RST (zworka na sasiednie piny)', HDR % 3, 220, 320,
      {'1': 'BOOT', '2': 'GND', '3': 'EN'})
place('R24', 'Device', 'R', '4,7 k (nie montowac)', R, 250, 320, {'1': 'SCL', '2': '+3V3'})
place('R25', 'Device', 'R', '4,7 k (nie montowac)', R, 270, 320, {'1': 'SDA', '2': '+3V3'})
place('TP6', 'Connector', 'TestPoint', 'TP SERVO_5V', TP, 290, 320, {'1': 'SERVO_5V'}, show_value=False, in_bom=False)
# listwa zasilania do wlasnych dodatkow: masa, 3,3 V, 5 V
place('J13', 'Connector_Generic', 'Conn_01x08', 'Listwa zasilania: GND 3V3 5V', HDR % 8, 320, 320,
      {'1': 'GND', '2': '+3V3', '3': '+5V', '4': 'GND', '5': 'GND', '6': '+3V3', '7': '+5V', '8': 'GND'})
# pole prototypowe: 4 bloki otworow 2x14 (rastr 2,54 mm), niepodlaczone
for k in range(4):
    place('J%d' % (14 + k), 'Connector_Generic', 'Conn_02x14_Odd_Even', 'Pole prototypowe %d (2x14, niepodlaczone)' % (k + 1),
          HDR2 % 14, 360 + k * 30, 320, {}, nc=[str(i) for i in range(1, 29)], show_value=False)

if errors:
    print('BLEDY OPISU:')
    print('\n'.join(errors))
    sys.exit(1)

lib_text = '\n'.join('    ' + t.replace('\n', '\n    ') for t in LIBS.values())
sheet = (
    '(kicad_sch (version 20250114) (generator "radar_gen") (generator_version "1.0")\n'
    '  (uuid "%s")\n  (paper "A3")\n'
    '  (title_block (title "Radar sonarowy ESP32-C6 - plytka glowna, wersja przewlekana") (date "2026-10-15") (rev "0.2")\n'
    '    (comment 1 "Wersja robocza do przegladu. Nie zamawiac przed testem prototypu."))\n'
    '  (lib_symbols\n%s\n  )\n  %s\n  (sheet_instances (path "/" (page "1")))\n  (embedded_fonts no)\n)\n'
    % (ROOT, lib_text, '\n  '.join(items)))
open(OUT, 'w').write(sheet)

# Tablice bibliotek projektu (ścieżki przez zmienne KiCada, działają na Dellu i na Asusie)
d = os.path.dirname(os.path.abspath(OUT))
sym_libs = sorted(set(k.split(':')[0] for k in LIBS))
fp_libs = sorted(set(f.split(':')[0] for f in FOOTPRINTS) | {'MountingHole'})
open(os.path.join(d, 'sym-lib-table'), 'w').write(
    '(sym_lib_table\n  (version 7)\n' + ''.join(
        '  (lib (name "%s")(type "KiCad")(uri "${KICAD10_SYMBOL_DIR}/%s.kicad_sym")(options "")(descr ""))\n' % (n, n)
        for n in sym_libs) + ')\n')
open(os.path.join(d, 'fp-lib-table'), 'w').write(
    '(fp_lib_table\n  (version 7)\n' + ''.join(
        '  (lib (name "%s")(type "KiCad")(uri "${KICAD10_FOOTPRINT_DIR}/%s.pretty")(options "")(descr ""))\n' % (n, n)
        for n in fp_libs) + ')\n')
print('OK: zapisano', os.path.abspath(OUT), '| elementow:', len(items), '| symboli bibliotecznych:', len(LIBS))
