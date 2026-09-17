# Hardware

This folder contains the hardware design files, manufacturing resources, and source files for the MR2 Reactive LEDs controller.

**New, in-progress addition:** an audio-reactive LED mode. Originally planned as a standalone breakout board wired to the existing PCB (see [audio-breakout.md](audio-breakout.md), now superseded); the decision since Phase 2 bench testing is to instead remanufacture the PCB itself with the audio conditioning circuit integrated directly — see [the build plan](../docs/audio-reactive-led-plan.md) for the full reasoning and Phase 3 checklist. This is why PCB files below are now organised by revision (`v1`, `v2`) rather than as a single flat set.

---

## System Bill of Materials (BOM)

The complete project parts list, including components outside of the PCB assembly, is available here:

[System BOM](bom/MR2_Reactive_LEDs_System_BOM.xlsx)

This includes:

- Electronics components
- Wiring
- Power components
- Mechanical parts
- Installation hardware

# PCB

The custom PCB was designed in EasyEDA to replace the breadboard prototype and provide a permanent vehicle-ready control board.

**`v1` (currently installed) is based around the ESP32-C3 Super Mini and integrates:**

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

The PCB was designed as a two-layer FR4 board and verified against the breadboard prototype before manufacture. Full history — including the GPIO4 fault on the first assembled `v1` board and its replacement — is in [docs/pcb-design.md](../docs/pcb-design.md) and [docs/build-log.md](../docs/build-log.md).

**`v2` (planned) adds the audio-reactive circuit** — R1, R2, R6, D1, C1, and a new RCA input connector — directly onto the same board, replacing the standalone-breakout-board approach originally planned. See [docs/audio-reactive-led-plan.md](../docs/audio-reactive-led-plan.md), Phase 3, for the full checklist. Not yet designed.

---

# Folder Structure

PCB files are organised by revision, the same way firmware versions are — each revision gets its own `gerbers/`, `bom/`, and `easyeda/` subfolder:

```text
hardware/
└── pcb/
    ├── v1/  (currently installed)
    │   ├── gerbers/
    │   ├── bom/
    │   └── easyeda/
    └── v2/  (planned — audio circuit integrated, not yet designed)
```

## Gerbers

PCB manufacturing files for the current (`v1`) revision are stored in:

[Gerber Files](pcb/v1/gerbers/)

These files are generated from EasyEDA and are used by the PCB manufacturer to produce the physical circuit board.

---

## PCB Bill of Materials (BOM)

The PCB BOM contains the components required to populate the custom PCB.

[BOM Files](pcb/v1/bom/)

## EasyEDA Source Files

The original PCB design files are stored in:

[EasyEDA Source Files](pcb/v1/easyeda/)

These files allow the PCB schematic and layout to be reviewed or modified in EasyEDA.

---

# Manufacturing

**`v1`: complete.** Designed in EasyEDA (schematic, layout, ERC/DRC), manufactured by JLCPCB, assembled, and installed in the car — see [docs/build-log.md](../docs/build-log.md) for the full history, including the GPIO4 fault on the first assembled board and its replacement.

**`v2`: planned, not yet started.** Will follow the same EasyEDA → JLCPCB process, adding the audio conditioning circuit to the existing design. See [docs/audio-reactive-led-plan.md](../docs/audio-reactive-led-plan.md), Phase 3.

---

# PCB Assembly

**`v1`: complete.** Manufactured, assembled (including a GPIO4 fault investigation and replacement board), installed in the control box, and running in the car. Full detail in [docs/build-log.md](../docs/build-log.md) and [docs/pcb-design.md](../docs/pcb-design.md).

**`v2`: planned, not yet started** — will follow once the board is manufactured, using the same validation approach as `v1` (visual inspection, continuity testing, power-on testing, bench-test before installing).
