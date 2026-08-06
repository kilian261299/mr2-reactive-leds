## Full Wiring Plan

### Power Input

Power is tapped from the rear of the cigarette lighter circuit.

```text
Cigarette lighter +12V
→ inline 15A fuse
→ USB charger module input +

Cigarette lighter ground
→ USB charger module input -
```

The fuse should be placed as close to the cigarette lighter tap as possible.

**Backfeed issue, accepted rather than fixed:** the lighter and radio circuits share a common fuse/node downstream of the ignition switch. The USB charger module (see below) has no reverse-current blocking diode on its input, so connecting the board via USB-C (laptop power) while the key is off currently backfeeds enough current to power the car radio on. This doesn't cross the ignition switch or reach the battery, so it isn't a safety or drain concern, but it confirms the USB-power and vehicle-power paths aren't isolated from each other — reinforcing that both must never be connected to the board at the same time. A Schottky diode (low forward voltage drop) placed in-line here, oriented to block reverse current, would fix this — but given the limited real-world impact, it was deliberately not installed.

### Master Power Switch — Not Fitted

The PCB includes a master power switch connector, originally intended for a Gebildet metal toggle switch. This was not fitted in the final build — insufficient mounting space was available at the intended location — so the connector is shorted instead.

The system is therefore live whenever it has power, with no separate physical on/off switch. Power is controlled entirely by whatever supplies the board — currently the fused 12V vehicle supply via the cigarette lighter circuit; USB-C is used separately for bench/upload purposes, never at the same time as vehicle power (see Backfeed issue above).

### 5V Power Distribution

Power is provided by a repurposed USB charger module — see "Power Module" below for why this replaced the originally planned buck converter.

```text
5V output +
├── ESP32-C3 5V / VIN
├── SN74AHCT125N pin 14
├── Left LED strip 5V
└── Right LED strip 5V

Ground output
├── ESP32-C3 GND
├── SN74AHCT125N pin 7
├── Left LED strip GND
├── Right LED strip GND
├── MPU6050 GND
└── Rotary encoder GND
```

The LED strips are powered directly from the power module, not through the ESP32-C3.

### Power Module — Buck Converter Replaced with a USB Charger Module

The originally specified 12V to 5V buck converter caused an intermittent cold-boot failure once wired into the car — the startup sweep would often not run, and sometimes not even the calibration flash would appear, though a manual power replug reliably fixed it once running. Traced to the buck converter's power-up characteristics (likely a slow or unclean voltage rise on cold start), rather than the wiring, fuse, or splices.

Replaced with a small USB phone-charger module, disassembled from its housing and mounted inside the control box in the buck converter's place, using the same 12V input and 5V output wiring. Confirmed via bench testing to output a clean, stable ~5.2V with no boot issues across repeated cold cycles. See the project build log for the full troubleshooting history.

### MPU6050 Accelerometer

```text
MPU6050 VCC → ESP32-C3 3.3V
MPU6050 GND → ESP32-C3 GND
MPU6050 SDA → ESP32-C3 GPIO 8
MPU6050 SCL → ESP32-C3 GPIO 9
```

### Rotary Encoder

```text
Rotary encoder VCC → ESP32-C3 3.3V
Rotary encoder GND → ESP32-C3 GND
Rotary encoder CLK/A → ESP32-C3 GPIO 4
Rotary encoder DT/B → ESP32-C3 GPIO 5
Rotary encoder SW → ESP32-C3 GPIO 6
```

### LED Data Level Shifting

The ESP32-C3 outputs 3.3V logic, so an SN74AHCT125N is used to shift the LED data signal to 5V.

```text
SN74AHCT125N pin 14 → 5V
SN74AHCT125N pin 7 → GND
```

Left LED data:

```text
ESP32-C3 GPIO 2
→ SN74AHCT125N pin 2

SN74AHCT125N pin 3
→ 330Ω resistor
→ Left LED strip DIN

SN74AHCT125N pin 1
→ GND
```

Right LED data:

```text
ESP32-C3 GPIO 3
→ SN74AHCT125N pin 5

SN74AHCT125N pin 6
→ 330Ω resistor
→ Right LED strip DIN

SN74AHCT125N pin 4
→ GND
```

Unused enable pins:

```text
SN74AHCT125N pin 10 → GND
SN74AHCT125N pin 13 → GND
```

### LED Strip Outputs

The LED strip was cut into two separate sections (approximately 0.5m / 80 LEDs each) to fit final mounting.

```text
Left LED strip:
5V → power module 5V
GND → power module GND
DIN → level-shifted data from SN74AHCT125N pin 3

Right LED strip:
5V → power module 5V
GND → power module GND
DIN → level-shifted data from SN74AHCT125N pin 6
```

### Recommended Box Connectors

```text
2-pin power input:
Pin 1 = fused 12V
Pin 2 = ground

3-pin left LED output:
Pin 1 = 5V
Pin 2 = data
Pin 3 = ground

3-pin right LED output:
Pin 1 = 5V
Pin 2 = data
Pin 3 = ground

5-pin rotary encoder:
Pin 1 = 3.3V
Pin 2 = ground
Pin 3 = CLK/A
Pin 4 = DT/B
Pin 5 = SW
```
