# PCB Design

## Schematic

![Schematic](../images/pcb/PCB_schematic.png)

The PCB was designed in EasyEDA around the ESP32-C3 Super Mini.

The design integrates:

- ESP32-C3 Super Mini
- SN74AHCT125N 3.3V to 5V level shifter
- MPU6050 accelerometer connector
- Rotary encoder connector
- Left and right WS2812B LED connectors
- JST-XH connectors
- 5V power input
- Toggle switch connector
- Decoupling capacitors
- Mounting holes

---

## PCB Layout

![PCB Layout](../images/pcb/PCB_LAYOUT.png)

Two-layer FR4 PCB design with separate 5V power routing.

The PCB layout was verified against the breadboard prototype before manufacture.

---

## 2D Preview

![PCB 2D Top Render](../images/pcb/PCB_RENDER_TOP.png)

![PCB 2D Bottom Render](../images/pcb/PCB_RENDER_BOTTOM.png)

---

## 3D Preview

![PCB 3D Render](../images/pcb/PCB_RENDER_TOP_3D.png)

3D render of the PCB before manufacture.

---

# Design Files

The PCB manufacturing files and editable design files are available below.

---

## Gerber Files

Manufacturing files generated from EasyEDA:

[Gerber Files](../hardware/pcb/gerbers/)

These files were submitted to the PCB manufacturer to produce the physical circuit board.

---

## PCB Bill of Materials (BOM)

The PCB BOM contains the components and reference designators used during PCB design.

The PCB was ordered as a bare manufactured board. Components will be manually sourced and soldered during PCB assembly.

[PCB BOM Files](../hardware/pcb/bom/)

---

## System Bill of Materials (BOM)

The complete project BOM, including components outside of the PCB assembly such as wiring, power components, LEDs, and installation hardware, is available here:

[System BOM](../hardware/bom/MR2_Reactive_LEDs_System_BOM.xlsx)

---

## EasyEDA Source

Original editable PCB design source:

[EasyEDA Source Files](../hardware/pcb/easyeda/)

The EasyEDA source files allow the schematic and PCB layout to be reviewed or modified.

---

# Manufacturing

**Status:** PCB ordered — awaiting delivery.

The PCB manufacturing files were generated after completing:

- Schematic design
- PCB layout
- Electrical Rule Check (ERC)
- Design Rule Check (DRC)

Gerber files were submitted to JLCPCB for manufacture.

The order consists of bare manufactured PCBs only. No components were assembled by the manufacturer.

---

# PCB Assembly

After receiving the manufactured PCB, components will be manually assembled and tested.

Planned assembly steps:

- Inspect manufactured PCB
- Solder PCB components
- Inspect solder joints
- Perform continuity testing
- Power-on testing
- Verify ESP32-C3 programming
- Test LED, encoder and accelerometer connections

---

# Assembled PCB

Pending.
