# MR2 Reactive LEDs

ESP32-C3 reactive LED controller for a Toyota MR2 using an MPU6050 accelerometer and WS2812B addressable LEDs.

## Overview

This project is a custom interior lighting controller for my 1992 Toyota MR2 SW20.

The system uses a chassis-mounted MPU6050 accelerometer to detect vehicle acceleration, braking, and cornering. An ESP32-C3 then controls two independent WS2812B LED strips, one in each footwell, creating smooth ambient lighting that reacts naturally to the car's movement.

The controller is built around a custom PCB and includes adjustable brightness, startup sequence, different modes, and a physical power switch for complete system shutdown.

## Features

- Reactive LED brightness based on vehicle acceleration
- Smooth blue ↔ orange colour blending
- Independent left and right LED outputs
- Cornering effects using lateral acceleration
- User-adjustable maximum brightness
- Rotary encoder controlled modes and calibration
- 3.3V to 5V logic level shifting using an SN74AHCT125N
- Custom EasyEDA PCB with JST-XH connectors
- Physical master power switch
- Powered from the cigarette lighter circuit via a fused 12V to 5V buck converter

## Hardware

| Component | Purpose |
|---|---|
| ESP32-C3 Super Mini | Main microcontroller |
| MPU6050 / GY-521 | Acceleration sensing |
| WS2812B LED strip | Addressable interior lighting |
| SN74AHCT125N | 3.3V to 5V LED data level shifter |
| Rotary encoder | Brightness adjustment and test mode selection |
| Gebildet Metal Toggle Switch | Master power switch |
| 12V to 5V buck converter | Converts vehicle power to 5V |
| Inline fuse holder | Protects added wiring |
| Custom PCB | Main controller board |
| JST-XH connectors | Removable wiring connectors |
| 18AWG wire | Main power wiring |
| 24AWG wire | Signal wiring |

## Tools / Equipment

| Tool | Use |
|---|---|
| Soldering iron | Soldering PCB and connectors |
| Solder | Electrical joints |
| Wire strippers | Preparing wiring |
| Multimeter | Voltage and continuity testing |
| Heat shrink | Insulating solder joints |
| Electrical tape | Temporary insulation and cable management |
| Small screwdriver set | Vehicle trim removal and assembly |
| USB-C cable | Programming the ESP32-C3 |

## Firmware Modes

| Mode | Purpose |
|---|---|
| **Mode 0** | Final driving mode. Blue ↔ orange colour blending with acceleration, braking and cornering effects. Idle breathing effect. |
| **Mode 1** | Purple colour mode. With acceleration, braking and cornering effects. user controls overall brightness. |
| **Mode 2** | Red colour mode. With acceleration, braking and cornering effects. user controls overall brightness. |
| **Mode 3** | Blue colour mode. With acceleration, braking and cornering effects. user controls overall brightness. |
| **Mode 4** | Green colour mode. With acceleration, braking and cornering effects. user controls overall brightness. |

## System Behaviour

| Driving State | LED Behaviour |
|---|---|
| Stationary / steady driving | Blue ambient lighting at the selected brightness |
| Acceleration | LEDs smoothly shift towards orange while increasing brightness |
| Braking | LEDs glow red, stronger braking = brighter red |
| Left corner | Left strip brightens, right strip dims |
| Right corner | Right strip brightens, left strip dims |
| Rotary encoder rotation | Adjust maximum LED brightness |
| Rotary encoder short press | Cycle through firmware modes |
| Rotary encoder long press | Recalibrate the accelerometer baseline. Flashes LEDs green to confrim |
| Gebildet toggle switch | Completely powers the controller on or off |

## Wiring Summary

Power is taken from the rear of the cigarette lighter circuit, protected by an inline fuse, converted to 5V using a buck converter, then routed to the controller PCB.

The PCB distributes power to the ESP32-C3, MPU6050, SN74AHCT125N level shifter and both LED strips. It also has a master power switch connector where a switch can be connected, I am using a Gebildet metal toggle switch.

The MPU6050 is mounted separately on the vehicle chassis to provide accurate acceleration measurements independent of the controller enclosure.

Full wiring details are available in [`docs/wiring-plan.md`](docs/wiring-plan.md).

## Documentation

- [Wiring Plan](docs/wiring-plan.md) - Complete wiring diagram, connector pinouts and ESP32 pin assignments.
- [Build Log](docs/build-log.md) - Project progress, PCB revisions, testing and installation notes.
- [Firmware Notes](firmware/README.md) - Test sketches, firmware behaviour and final Arduino implementation.
