# MR2 Reactive LEDs

ESP32-C3 reactive LED controller for a Toyota MR2 using an MPU6050 accelerometer and WS2812B addressable LEDs.

## Overview

This project is a custom interior lighting controller for a Toyota MR2 SW20.

The system uses an MPU6050 accelerometer to detect acceleration, braking, and cornering. An ESP32-C3 then controls two WS2812B addressable LED strips, one for the left side of the cabin and one for the right side.

The aim is to create subtle reactive lighting that changes based on how the car is moving.

## Planned Features

- Acceleration-reactive LED brightness
- Blue to orange colour fade under acceleration
- Red pulse effect under braking
- Left/right effects during cornering
- Rotary encoder for brightness, mode selection, and software on/off
- Two independent LED outputs for left and right strips
- 3.3V to 5V data level shifting using an SN74AHCT125N
- Fused 12V power from the cigarette lighter circuit
- Hidden control box mounted behind the centre dash

## Hardware

| Component | Purpose |
|---|---|
| ESP32-C3 Super Mini | Main microcontroller |
| MPU6050 / GY-521 | Acceleration and G-force sensing |
| WS2812B LED strip | Addressable interior lighting |
| SN74AHCT125N | 3.3V to 5V LED data level shifter |
| Rotary encoder | Brightness, mode, and on/off control |
| 12V to 5V buck converter | Converts car power to 5V |
| Inline fuse holder | Protects added wiring |
| JST-XH connectors | Connectors for removable wiring |
| 18AWG wire | Power and ground wiring |

## System Behaviour

| Driving State | LED Behaviour |
|---|---|
| Idle / smooth driving | Dim blue ambient glow |
| Acceleration | Fade toward purple/orange and increase brightness |
| Hard acceleration | Bright orange pulse or sweep |
| Braking | Red pulse |
| Cornering | Left/right LED intensity shift |
| Encoder rotation | Adjust brightness |
| Encoder click | Change mode |
| Encoder long press | Software on/off |

## Full Wiring Plan

### Power Input

Power is tapped from the rear of the cigarette lighter circuit.

```text
Cigarette lighter +12V
→ inline 5A fuse
→ 12V to 5V buck converter input +

Cigarette lighter ground
→ 12V to 5V buck converter input -
```

The fuse should be placed as close to the cigarette lighter tap as possible.

### 5V Power Distribution

The buck converter provides 5V for the ESP32-C3, LED strips, and level shifter.

```text
Buck converter 5V output +
├── ESP32-C3 5V / VIN
├── SN74AHCT125N pin 14
├── Left LED strip 5V
└── Right LED strip 5V

Buck converter ground output
├── ESP32-C3 GND
├── SN74AHCT125N pin 7
├── Left LED strip GND
├── Right LED strip GND
├── MPU6050 GND
└── Rotary encoder GND
```

The LED strips are powered directly from the buck converter, not through the ESP32-C3.

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

The LED strip will be cut into two separate sections.

```text
Left LED strip:
5V → buck converter 5V
GND → buck converter GND
DIN → level-shifted data from SN74AHCT125N pin 3

Right LED strip:
5V → buck converter 5V
GND → buck converter GND
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
