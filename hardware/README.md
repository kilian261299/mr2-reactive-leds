# Hardware

This folder contains the hardware design files, manufacturing resources, and source files for the MR2 Reactive LEDs controller.

**New addition, Phase 3 in progress — board ordered, in fabrication/shipping:** an audio-reactive LED mode. Originally planned as a standalone breakout board wired to the existing PCB (see [audio-breakout.md](audio-breakout.md), now superseded); the decision since Phase 2 bench testing is to instead remanufacture the PCB itself with the audio conditioning circuit integrated directly — see [the build plan](../docs/audio-reactive-led-plan.md) for the full reasoning and Phase 3 checklist. This is why PCB files below are now organised by revision (`v1`, `v2`) rather than as a single flat set. `v2`'s EasyEDA schematic/layout is designed and verified (DRC clean, connectivity checked against the exported netlist), design files are committed under `hardware/pcb/v2/`, all remaining parts (C4, D1, J7) have been ordered, and Gerbers have been submitted to JLCPCB (Global Standard Direct Line, 8–12 business days quoted). The `v4.0` firmware has been drafted in parallel (see [firmware/README.md](../firmware/README.md)) and compiles clean, but isn't bench-tested against real audio yet, since the conditioning circuit parts are with the board.

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

The workbook has two tabs: `Sheet1` for the original core-project parts, and `Audio Addition` for the new audio-reactive circuit's parts (R3–R6, C3, C4, D1, J7), kept separate to show what was added after the core system was already built.

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
- Toggle switch connector (`J6_SWITCH` — never actually populated, just shorted instead; removed for `v2`, see below)
- Decoupling capacitors
- Mounting holes

The PCB was designed as a two-layer FR4 board and verified against the breadboard prototype before manufacture. Full history — including the GPIO4 fault on the first assembled `v1` board and its replacement — is in [docs/pcb-design.md](../docs/pcb-design.md) and [docs/build-log.md](../docs/build-log.md).

**`v2` (in progress, ordered from JLCPCB) adds the audio-reactive circuit** — R3, R4, R5, R6, C3, C4, D1 (BAT85), and a new `J7` RCA input connector — directly onto the same board, replacing the standalone-breakout-board approach originally planned, and dropping the never-populated `J6_SWITCH`. Reference designators continue on from `v1`'s existing `R1`/`R2`/`C1`/`C2` (already used for other components) rather than reusing them — see [docs/audio-reactive-led-plan.md](../docs/audio-reactive-led-plan.md), Phase 3, for the full checklist. Design complete: cloned from `v1`, DRC clean, schematic connectivity verified, `J7` wired with Front Left → R3 / Front Right → R4 matching the documented convention.

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
    └── v2/  (audio circuit integrated, ordered from JLCPCB — in fabrication/shipping)
```

## Gerbers

PCB manufacturing files for the currently-installed (`v1`) revision are stored in:

[Gerber Files (v1)](pcb/v1/gerbers/)

The `v2` design's Gerbers (audio circuit integrated, about to be submitted to JLCPCB) are in:

[Gerber Files (v2)](pcb/v2/gerbers/)

These files are generated from EasyEDA and are used by the PCB manufacturer to produce the physical circuit board.

---

## PCB Bill of Materials (BOM)

The PCB BOM contains the components required to populate the custom PCB.

[BOM Files (v1)](pcb/v1/bom/) · [BOM Files (v2)](pcb/v2/bom/)

## EasyEDA Source Files

The PCB design files (schematic + layout) are stored per revision:

[EasyEDA Source Files (v1)](pcb/v1/easyeda/) · [EasyEDA Source Files (v2)](pcb/v2/easyeda/)

These files allow the PCB schematic and layout to be reviewed or modified in EasyEDA.

---

# Manufacturing

**`v1`: complete.** Designed in EasyEDA (schematic, layout, ERC/DRC), manufactured by JLCPCB, assembled, and installed in the car — see [docs/build-log.md](../docs/build-log.md) for the full history, including the GPIO4 fault on the first assembled board and its replacement.

**`v2`: designed in EasyEDA, ordered from JLCPCB — in fabrication/shipping.** Cloned from the `v1` project rather than editing it in place, adding the audio conditioning circuit (R3–R6, C3, C4, D1, J7). DRC clean (0 errors); EasyEDA Standard has no separate ERC report, so schematic connectivity was verified directly against the exported netlist instead. All parts (C4, D1, J7) ordered. Gerbers submitted via JLCPCB's Global Standard Direct Line shipping, 8–12 business days quoted. See [docs/audio-reactive-led-plan.md](../docs/audio-reactive-led-plan.md), Phase 3, for the full checklist.

---

# PCB Assembly

**`v1`: complete.** Manufactured, assembled (including a GPIO4 fault investigation and replacement board), installed in the control box, and running in the car. Full detail in [docs/build-log.md](../docs/build-log.md) and [docs/pcb-design.md](../docs/pcb-design.md).

**`v2`: planned, not yet started** — will follow once the board is manufactured, using the same validation approach as `v1` (visual inspection, continuity testing, power-on testing, bench-test before installing).
