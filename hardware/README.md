# Hardware

This folder contains the hardware design files, manufacturing resources, and source files for the MR2 Reactive LEDs controller.

---

## System Bill of Materials (BOM)

The complete project parts list, including components outside of the PCB assembly, is available here:

[System BOM](bom)

This includes:

- Electronics components
- Wiring
- Power components
- Mechanical parts
- Installation hardware

# PCB

The custom PCB was designed in EasyEDA to replace the breadboard prototype and provide a permanent vehicle-ready control board.

The PCB is based around the ESP32-C3 Super Mini and integrates the following components:

- ESP32-C3 Super Mini
- SN74AHCT125N 3.3V to 5V level shifter
- MPU6050 accelerometer connector
- Rotary encoder connector
- Left and right WS2812B LED outputs
- JST-XH connectors
- 12V power input connector
- Toggle switch connector
- Decoupling capacitors
- Mounting holes

The PCB was designed as a two-layer FR4 board and verified against the breadboard prototype before manufacture.

---

# Folder Structure

## Gerbers

PCB manufacturing files are stored in:

[Gerber Files](pcb/gerbers/)

These files are generated from EasyEDA and are used by the PCB manufacturer to produce the physical circuit board.

---

## PCB Bill of Materials (BOM)

The PCB BOM contains the components required to populate the custom PCB.

The PCB was ordered as a bare manufactured board. Components will be manually assembled and tested after delivery.

[BOM Files](pcb/bom/)

## EasyEDA Source Files

The original PCB design files are stored in:

[EasyEDA Source Files](pcb/easyeda/)

These files allow the PCB schematic and layout to be reviewed or modified in EasyEDA.

---

# Manufacturing

PCB manufacturing files were generated after completing:

- Schematic design
- PCB layout
- Electrical Rule Check (ERC)
- Design Rule Check (DRC)

The PCB was submitted to JLCPCB for manufacture.

Current status:

**PCB design complete. Awaiting manufactured PCB delivery.**

---
