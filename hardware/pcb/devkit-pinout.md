# Pinout ESP32-C6-DevKitC-1 (listwy J1 i J3, 2,54 mm)

Źródło: schemat Espressif v1.4 (`docs/hardware/references/esp32-c6-devkitc-1-v1.4-schematic.pdf`,
strona 2), odczytany z rysunku 2026-10-10. Numeracja od końca z anteną.

| Pin | J1 | J3 |
|---|---|---|
| 1 | 3V3 | **GND** |
| 2 | RST (CHIP_PU) | TXD (UART0) |
| 3 | GPIO4 | RXD (UART0) |
| 4 | GPIO5 | GPIO15 |
| 5 | **GPIO6** (ECHO) | GPIO23 |
| 6 | **GPIO7** (MAX7219 DIN) | **GPIO22** (buzzer) |
| 7 | GPIO0 | **GPIO21** (I2C SCL) |
| 8 | GPIO1 | **GPIO20** (I2C SDA) |
| 9 | GPIO8 | **GPIO19** (MAX7219 CS) |
| 10 | **GPIO10** (serwo) | **GPIO18** (MAX7219 CLK) |
| 11 | **GPIO11** (TRIG) | GPIO9 |
| 12 | **GPIO2** (przycisk ON/standby) | **GND** |
| 13 | GPIO3 | GPIO13 (USB D+) |
| 14 | **5V** | GPIO12 (USB D−) |
| 15 | **GND** | **GND** |
| 16 | NC | NC |

Uwagi:
- **GND jest na J1 pin 15 oraz na J3 pinach 1, 12 i 15** (J3 pin 12 i 15 są połączone z masą na
  schemacie). Wcześniejsze stwierdzenie „po jednym GND na listwę” było błędne.
- GPIO12/13 to linie USB (natywne USB-C), nie używać. GPIO4/5/8/9/15 to piny strapping.
- Pin 5V (J1 pin 14) leży obok GND (pin 15): przy kablach łatwo je pomylić. Na PCB pin 5V DevKita
  jest połączony z +5V płytki (zasilanie z USB-C płytki); DevKit ma diodę Schottky od własnego USB.
