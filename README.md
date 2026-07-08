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

## Tools / Equipment

Basic tools already available:

| Tool | Use |
|---|---|
| Soldering iron | Soldering wires, connectors, and perfboard |
| Solder | Electrical joints |
| Wire strippers | Preparing wires |
| Multimeter | Checking voltage, continuity, and converter output |
| Heat shrink | Insulating soldered joints |
| Electrical tape | Temporary insulation and cable wrapping |
| Small screwdriver set | Opening trim and securing parts |
| USB cable | Programming and testing the ESP32-C3 |

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

## Wiring Summary

Power is tapped from the rear of the cigarette lighter circuit, fused, then stepped down to 5V for the ESP32-C3, LED strips, level shifter, and sensors.

The two LED strips are wired as independent left and right outputs so they can react separately during acceleration, braking, and cornering.

Full wiring details are in [`docs/wiring-plan.md`](docs/wiring-plan.md).
