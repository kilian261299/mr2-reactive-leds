# MR2 Reactive LEDs

ESP32-C3 reactive LED controller for a Toyota MR2 using an MPU6050 accelerometer and WS2812B addressable LEDs.

## Overview

This project is a custom interior lighting controller for my 1992 Toyota MR2 SW20.

The system uses a chassis-mounted MPU6050 accelerometer to detect vehicle acceleration, braking, and cornering. An ESP32-C3 then controls two independent WS2812B LED strips, one in each footwell, creating smooth ambient lighting that reacts naturally to the car's movement.

The controller is built around a custom PCB and includes adjustable brightness, startup sequence, and different modes.

## Project Status

**Current Stage:** Fully installed in the MR2 — control box, accelerometer, rotary encoder, and LED strips are all mounted in their final positions, running on actual vehicle power (12V from the cigarette lighter circuit). The originally specified buck converter was replaced with a repurposed USB charger module after reliability issues on cold boot — see [Build Log](docs/build-log.md) for details. The only remaining work is firmware tuning, based on real driving data.

Completed:

- Hardware selection
- Breadboard prototype
- Firmware development (currently tuning v2.x — see [firmware/README.md](firmware/README.md) for the version history, including v3.0's gyroscope-based approach, currently parked pending further testing)
- Custom PCB design
- PCB manufacturing
- PCB assembly (including a GPIO fault investigation and replacement board)
- Control box construction
- Control box permanently installed in the MR2
- MPU6050 accelerometer permanently mounted, under the shift boot leather
- Rotary encoder permanently installed, with a hole drilled into the dash trim to fit it
- LED strips permanently installed in the footwells
- Vehicle 12V power connected (buck converter replaced with a USB charger module)
- Several real-world test drives, firmware tuning based on results
- Hardware documentation

Upcoming:

- Further test drives to confirm the latest firmware tuning (v2.2 and beyond) — this is the only remaining project work

## Features

- Reactive LED brightness based on vehicle acceleration
- Smooth blue ↔ orange colour blending
- Hill compensation, so genuine acceleration/braking is distinguished from the vehicle simply pitching on a slope (accelerometer-only baseline approach — a gyroscope-based version exists and is being evaluated, see firmware docs)
- Independent left and right LED outputs
- Cornering effects using lateral acceleration
- User-adjustable maximum brightness
- Rotary encoder controlled modes and calibration
- 3.3V to 5V logic level shifting using an SN74AHCT125N
- Custom EasyEDA PCB with JST-XH connectors
- Powered from the cigarette lighter circuit via a fused 5V supply module

## Hardware

The system is built around an ESP32-C3 microcontroller with custom PCB hardware designed for integration into the Toyota MR2.

The table below covers the major functional components. For the complete parts list — including fasteners, passives, connectors, and sourcing/supplier notes — see the [System BOM](hardware/bom/MR2_Reactive_LEDs_System_BOM.xlsx), which is the definitive source for exact quantities and part numbers.

| Component | Purpose |
|---|---|
| ESP32-C3 Super Mini | Main microcontroller running the reactive lighting firmware |
| MPU6050 / GY-521 | Accelerometer used for vehicle motion detection |
| WS2812B LED strip | Addressable interior lighting output |
| SN74AHCT125N | 3.3V to 5V logic level shifter for reliable LED data transmission |
| Rotary encoder | Brightness adjustment and lighting mode selection |
| USB charger module (repurposed) | Converts vehicle electrical supply to regulated 5V — replaces the originally specified buck converter; see Build Log |
| Inline fuse holder | Protects the additional vehicle wiring |
| Custom PCB | Dedicated controller board integrating the system hardware |
| JST-XH connectors | Removable connections between PCB and external components |
| 18AWG wire | Main power wiring |
| 24AWG wire | Low-current signal wiring |

Detailed hardware documentation, PCB design information, and manufacturing files:

- [System BOM](hardware/bom/MR2_Reactive_LEDs_System_BOM.xlsx) — complete parts list
- [PCB Design Documentation](docs/pcb-design.md)
- [Hardware Files](hardware/README.md)

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

All modes share the same underlying sensor processing — including hill compensation — so a steady hill is correctly ignored in every mode's brightness, not just Mode 0's colour. The rotary encoder sets a **maximum** brightness ceiling in every mode too — actual output stays at or below that, reacting to vehicle movement and cornering, and gently breathing when the car is calm. The only real difference between modes is which colour(s) are shown.

| Mode | Colour behaviour |
|---|---|
| **Mode 0** | Main driving mode. Colour reacts to what the car is doing: blue at rest, shifting toward orange under acceleration, red under braking. |
| **Mode 1** | Fixed purple. Brightness still reacts to movement/cornering, but colour doesn't change with acceleration vs braking the way Mode 0's does. |
| **Mode 2** | Fixed red. Same brightness behaviour as Mode 1. |
| **Mode 3** | Fixed blue. Same brightness behaviour as Mode 1. |
| **Mode 4** | Fixed green. Same brightness behaviour as Mode 1. |

## System Behaviour

| Driving State | LED Behaviour |
|---|---|
| Stationary / steady driving | Blue ambient lighting at the selected brightness, gently breathing |
| Acceleration | LEDs smoothly shift towards orange while increasing brightness |
| Braking | LEDs glow red, stronger braking = brighter red |
| Steady hill or gradient | LEDs settle back to calm blue once speed is steady, rather than staying reactive for the length of the hill |
| Left corner | Left strip brightens, right strip dims |
| Right corner | Right strip brightens, left strip dims |
| Rotary encoder rotation | Adjust maximum LED brightness |
| Rotary encoder short press | Cycle through firmware modes |
| Rotary encoder long press | Recalibrate the accelerometer baseline. Flashes LEDs green to confirm |
| System power | No separate switch — the system is live whenever it has power (fused 12V vehicle supply, via the cigarette lighter circuit) |

## Wiring Summary

Power is taken from the rear of the cigarette lighter circuit, protected by an inline fuse, converted to 5V using a repurposed USB charger module (replacing the originally specified buck converter — see Build Log for why), then routed to the controller PCB.

The PCB distributes power to the ESP32-C3, MPU6050, SN74AHCT125N level shifter and both LED strips. The PCB has a master power switch connector; the physical Gebildet toggle switch originally planned for this wasn't fitted due to insufficient mounting space at the intended location, so this connector is currently shorted instead. The system is live whenever it has power, with no separate physical on/off switch.

**Known issue, accepted:** the charger module has no reverse-current blocking diode on its input, and the lighter/radio circuits share a common fuse downstream of the ignition switch — connecting the board via USB (laptop power) with the key off currently backfeeds enough current to power the car radio on. Doesn't cross the ignition switch or reach the battery, so not a safety/drain concern, but confirms USB power and vehicle power must never be connected to the board simultaneously. A blocking diode would fix this, but given the limited real-world impact, it's a deliberate trade-off rather than a planned fix — see Build Log.

The MPU6050 is mounted separately on the vehicle chassis to provide accurate acceleration measurements independent of the controller enclosure.

Full wiring details are available in [`docs/wiring-plan.md`](docs/wiring-plan.md).

## Documentation

- [Wiring Plan](docs/wiring-plan.md) - Complete wiring diagram, connector pinouts and ESP32 pin assignments.
- [Build Log](docs/build-log.md) - Project progress, PCB revisions, testing and installation notes.
- [Firmware Notes](firmware/README.md) - Test sketches, firmware behaviour and version history.
- [PCB Design Documentation](docs/pcb-design.md)
- [Hardware Files](hardware/README.md)
