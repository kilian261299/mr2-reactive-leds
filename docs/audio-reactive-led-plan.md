# Audio-Reactive LED Feature — Build Plan

A new, optional mode for the MR2 Reactive LEDs project: LEDs react to music from the car radio, in addition to the existing acceleration/braking/cornering reactivity. This is an addition layered on top of the completed core project — it does not replace or modify the existing v2.x firmware line.

---

## Circuit Design

**Note on D1 (the rectifier diode):** using a 1N4007 rather than the more typical small-signal choice (e.g. 1N4148) for this role, based on what was already on hand. The 1N4007 is a general-purpose power rectifier with much slower switching than a dedicated signal diode — normally a mismatch for audio-frequency work, but not a practical problem here, since the smoothing capacitor (C1) is deliberately the slow part of this circuit already, turning the rectified signal into a "how loud is the music right now" envelope over hundreds of milliseconds. The diode's speed was never going to be the limiting factor for something changing that slowly.

### Signal path overview

```
Front Left RCA  ──┐
Front Right RCA ──┼── mono-summed at the tap point
RCA ground/shield ─┘  (tapped in parallel — never cut the cable in series)
        │
        ▼
Conditioning circuit (see component list below)
        │
        ▼
ESP32 ADC pin (reads a 0–3.3V audio envelope)
        │
        ▼
Firmware: smooth, map to brightness/colour
        │
        ▼
WS2812B LED strips (existing LEFT_LED_PIN / RIGHT_LED_PIN)
```

### Testing setup — full pipeline

**⚠ Image below is out of date** — it shows the original full-ESP32-dev-board setup (`GPIO34`/`GPIO5`). Testing now uses a spare ESP32-C3 module instead (see "Testing board update" note below); the image needs regenerating with `GPIO1`/`GPIO7` before it's accurate again.

![Testing setup overview](../images/audio-circuit/testing_setup_overview.png)

This is the complete bench-test signal path: iPhone headphone output (both L and R channels), summed and conditioned, into a spare ESP32-C3 module, driving the addressable LED strip. This is the testing configuration only — the production install uses the car's actual RCA tap and the ESP32-C3 installed in the car, not an iPhone or this spare board.

**Testing board update:** originally planned around a full ESP32 dev board (hence the `GPIO34` references elsewhere in this doc's history). A spare ESP32-C3 module turned out to be available instead, which is actually simpler: it's the same chip as production, so the audio ADC pin (`GPIO1`) is identical on both, and Phase 2 tuning carries straight into Phase 4 with no pin remapping. See the updated pinout table below.

### Testing circuit — schematic

**⚠ Image below is out of date** — labelled `ESP32 GPIO34 (dev board)`; needs regenerating as `ESP32-C3 GPIO1 (test module)`.

![Testing circuit schematic](../images/audio-circuit/testing_circuit_schematic.png)

### Testing circuit — component list

```
Phone 3.5mm tip (L)    → R1 (2.2kΩ) ──┐
Phone 3.5mm ring (R)   → R2 (2.2kΩ) ──┼── Node S (mono sum)
Phone 3.5mm sleeve (ground) → GND
   (shared ground reference for the whole circuit)

Node S → R3 (10kΩ) → Node A
Node A → R4 (1kΩ) → GND

Node A → D1 (1N4007) → Node B

Node B → C1 (2.2µF) → GND

Node B → R5 (10kΩ) → 3.3V
Node B → R6 (10kΩ) → GND

Node B → ESP32-C3 (spare test module) GPIO1 (ADC input)
```

Values are starting points for Phase 2 — expect to retune R3/R4 (divider ratio) and C1 (smoothing) once you're actually watching the LED respond to real music.

### Production setup — full pipeline

![Production setup overview](../images/audio-circuit/production_setup_overview.png)

Same Y-split concept as the testing pipeline, now with the real components: the radio's Front L/R RCA (plus shield/ground) splits to the amplifier, completely unchanged, and separately to the new conditioning circuit → ESP32-C3 → the existing LED strips. No new strip or amp wiring — everything downstream of the ESP32-C3 already exists.

### Production circuit — schematic

![Production circuit schematic](../images/audio-circuit/production_circuit_schematic.png)

### Production circuit — component list

```
Front Left RCA  → R1 (2.2kΩ*) ──┐
Front Right RCA → R2 (2.2kΩ*) ──┼── Node S (mono sum)
RCA shield/ground → GND
   (shared ground reference for the whole circuit — see Ground
   note below, this must be the RCA tap's own shield, not a
   separate chassis point)

Node S → R3 (10kΩ*) → Node A
Node A → R4 (1kΩ*) → GND

Node A → D1 (1N4007) → Node B

Node B → C1 (2.2µF*) → GND

Node B → R5 (10kΩ) → 3.3V
Node B → R6 (10kΩ) → GND

Node B → ESP32-C3 GPIO1 (ADC input, production board)
```

`*` marks values expected to change once Phase 2 bench testing confirms what actually works — currently the same as the testing circuit's starting values. **Update both this schematic and this list with confirmed values before Phase 3 begins.** R5/R6 (bias network) aren't marked, since they're unlikely to need tuning.

Now that testing uses a spare ESP32-C3 module (see "Testing board update" above), this circuit and the testing circuit are topologically identical, down to the same `GPIO1` ADC pin — the only real difference is the input source (a phone's 3.5mm jack for testing vs. the real Front L/R RCA here) and which physical ESP32-C3 module it's wired to.

**Ground note:** the conditioning circuit's ground must trace back to the RCA tap's own ground/shield connection — not a separate chassis ground point used elsewhere in the car (e.g. the 12V system's ground). Referencing two different chassis points that aren't at exactly the same potential is a classic source of audible ground-loop hum in the actual audio system, not just an LED-behaviour issue.

**Tap method:** bridge off both signal wires and the ground in parallel at the radio's RCA harness. Never cut into the cable and route the circuit in series — that would interrupt the original signal to the amplifier.

### Pinout — testing board (spare ESP32-C3 module)

Used for bench testing only — a separate, spare ESP32-C3 module, not the one installed in the car.

| Pin/net | Connects to | Purpose |
|---|---|---|
| `GPIO1` | Conditioning circuit output (Node B) | ADC audio input — ADC1_CH1, same pin the production board uses |
| `GPIO7` | LED strip `DIN` | LED data output — free, non-strapping pin; confirm against your specific board's silkscreen |
| `3.3V` | Conditioning circuit bias network (R5/R6), LED strip `5V`* | Power |
| `GND` | Conditioning circuit ground, LED strip `GND`, ESP32-C3 `GND` | Common ground — all must share this one reference |

*Most addressable strips want 5V for reliable operation; running directly off the module's 3.3V is commonly acceptable for a short bench-test wire run, but isn't the final production arrangement — the real install uses proper level shifting (below).

The 3.3V choice here is deliberate, not just "good enough": powering the strip at 3.3V makes its data-logic threshold match the ESP32-C3's 3.3V `GPIO7` output exactly, avoiding the need for a level shifter on the bench. Power the strip at 5V instead (also an option — most ESP32-C3 modules break out a `5V`/`VIN` pin) and the data line's 3.3V logic may not reliably clear the WS2812B's ~70%-of-VDD "HIGH" threshold without one — exactly why the production board has one (SN74AHCT125N). Trade-off either way: 3.3V power slightly under-drives the LED chips (usually just dimmer/less accurate colour, not broken); 5V power without a level shifter risks flicker or no response, more likely as the strip/wire gets longer.

### Pinout — production board (ESP32-C3, already installed)

| Pin/net | Connects to | Purpose |
|---|---|---|
| `GPIO1` | Breakout board's conditioning circuit output (Node B) | ADC audio input — **new addition**, currently free |
| `LEFT_LED_PIN` / `RIGHT_LED_PIN` | Existing LED strips | **Unchanged** — reuses the current output path, no new strip or data pin |
| `3.3V` (existing PCB net) | Breakout board's `3.3V` in | Powers the new conditioning circuit |
| `GND` (existing PCB net) | Breakout board's `GND` — **which must trace to the RCA tap's own ground/shield**, not a separate chassis point | Common ground — see Ground note above |

The only genuinely new wiring on the production board is three connections to the breakout: `3.3V`, `GND`, and `GPIO1`. Everything else (LED strips, level shifter, encoder, accelerometer) is completely untouched.

---

## Phase 1: GitHub setup

Prompt for Claude Code:

```
I want to add a new audio-reactive LED feature to this project.
Read docs/build-log.md and firmware/README.md first for context
on how this repo is organized.

First, add this plan document to the repo at
docs/audio-reactive-plan.md, including its images at
images/audio-circuit/testing_setup_overview.png,
images/audio-circuit/testing_circuit_schematic.png,
images/audio-circuit/production_setup_overview.png, and
images/audio-circuit/production_circuit_schematic.png. Commit it.

Then set up:
1. firmware/tests/04_audio_reactive_test/ — a new bench-test
   sketch that reads an audio envelope on an ADC pin, smooths it,
   and drives a real addressable LED strip using the same
   Adafruit_NeoPixel approach as the main firmware. This test
   runs on a full ESP32 dev board, not the ESP32-C3 the main
   controller uses (that board is installed in the car) — note
   this clearly in the sketch so pin numbers aren't confused with
   the production board later.
2. hardware/audio-breakout.md — a new file documenting the audio
   conditioning circuit: mono-summed front L/R RCA tap, resistor
   divider, diode rectifier, smoothing capacitor, bias network.
   I'll fill in exact component values once bench-tested.
3. A new section in docs/build-log.md, clearly separated from the
   existing stages — this is a new experimental addition on top
   of the completed core project, not a continuation of it.

Commit each piece separately with a clear message, so the history
stays readable.

Don't touch the main firmware versions (v2.2 etc) — this is
bench-test-first, same as how the encoder and accelerometer were
originally validated before joining the real firmware.
```

---

## Phase 2: Build and bench-test

Using a spare ESP32-C3 module and the real addressable LED strip found for testing — **the ESP32-C3 installed in the car is not touched during this phase**, avoiding any risk to the already-working, installed v2.4.1 firmware. (Originally planned around a full ESP32 dev board — see the "Testing board update" note above for why a spare ESP32-C3 module is used instead.)

**Stage A — initial tuning, phone audio:**
- [ ] Breadboard the conditioning circuit using the component list above
- [ ] Wire audio output to `GPIO1`, LED data to `GPIO7` (or confirmed equivalents)
- [ ] Flash the Phase 1 test sketch
- [ ] Feed it phone headphone audio (line-level output, good first stand-in for the radio)
- [ ] Tune R3/R4 (divider ratio) and C1 (smoothing) by watching how the LED actually responds

**Stage B — validate against the real car radio, still on the spare test module:**
- [ ] With the breadboard circuit still on the spare ESP32-C3 module (not the one installed in the car), temporarily clip onto the car's actual Front L/R RCA connectors and ground — alligator clip leads, not a permanent splice yet
- [ ] Re-check the tuning with real music through the car's actual radio and amp — car head units can output different line-level voltages than a phone, and real music behaves differently than a phone test track, so this can reveal a need for further adjustment
- [ ] **Record the final working component values** — carries directly into Phase 3
- [ ] Update the production schematic image and component list above with confirmed values (replace the `*`-marked placeholders) and remove the asterisks

---

## Phase 3: Build the permanent circuit

- [ ] Confirm the RGB/addressable LED and conditioning circuit values from Phase 2
- [ ] Solder the confirmed values onto a small perfboard/breakout board
- [ ] Tap Front Left + Front Right RCA, plus ground/shield, in parallel at the radio harness (see Ground note above)
- [ ] Connect breakout GND to the existing PCB's ground net (same single ground path — see note above)
- [ ] Connect breakout 3.3V and signal output to the existing PCB's 3.3V and `GPIO1`

---

## Phase 4: Install and integrate the firmware

- [ ] Mount the breakout board in the control box
- [ ] Route the RCA tap wires to the radio harness
- [ ] Add a new mode to the real firmware (new version, e.g. v2.5, branching from the current final v2.4.1), using Phase 2's tuned values, driving the actual strips through the existing `setStrip()` / NeoPixel functions
- [ ] Test in the car with real music — expect some retuning against real strips, cabin acoustics, and road noise
- [ ] Update `docs/build-log.md` and the firmware changelog with results
