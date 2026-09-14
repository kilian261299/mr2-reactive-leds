# Audio Conditioning Breakout

Documents the small conditioning circuit for the audio-reactive LED feature — see [docs/audio-reactive-led-plan.md](../docs/audio-reactive-led-plan.md) for the full build plan this belongs to. This is a new, optional addition layered on top of the completed core project; it does not change the existing PCB, wiring, or firmware.

**Status: component values are placeholders, not yet bench-confirmed.** Everything marked `*` below is expected to change once Phase 2 bench testing (see the build plan) shows what actually works with real audio. Update this file with confirmed values before Phase 3 (permanent circuit) begins.

---

## Purpose

Takes a line-level stereo audio signal (car radio's Front L/R RCA outputs) and converts it into a single, slowly-varying 0–3.3V envelope that an ESP32 ADC pin can read directly — the firmware then smooths and maps that envelope to LED brightness/colour.

The circuit does four things in sequence: sums the two channels to mono, scales the voltage down to a safe range, rectifies it to a one-directional envelope, and smooths + biases it to sit within the ADC's 0–3.3V input range.

---

## Signal Path

```
Front Left RCA  ──┐
Front Right RCA ──┼── mono-summed at the tap point
RCA ground/shield ─┘  (tapped in parallel — never cut the cable in series)
        │
        ▼
Conditioning circuit (this document)
        │
        ▼
ESP32 ADC pin (0–3.3V audio envelope)
```

---

## Testing Circuit (Bench, Spare ESP32-C3 Module)

Used for Phase 2 bench testing only — a separate, spare ESP32-C3 module, not the one installed in the car. Originally planned around a full ESP32 dev board; a spare ESP32-C3 module turned out to be available instead, which is actually simpler, since it's the same chip as production — the audio ADC pin (`GPIO1`) is identical on both, so Phase 2 tuning carries straight into Phase 4 with no pin remapping.

Input is a phone's 3.5mm headphone jack (both L and R channels, mono-summed via R1/R2) rather than the car's RCA outputs, as a convenient stand-in for early tuning — the same summing topology as the production circuit, just with a phone jack instead of RCAs.

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

**⚠ Image below is out of date** — labelled `ESP32 GPIO34 (dev board)`; needs regenerating as `ESP32-C3 GPIO1 (test module)`.

![Testing circuit schematic](../images/audio-circuit/testing_circuit_schematic.png)

| Component | Role |
|---|---|
| R1 / R2 (2.2kΩ) | Series input resistors, sum L+R to mono and limit current from the audio source |
| R3 / R4 (10kΩ / 1kΩ) | Voltage divider, scales the line-level signal down before rectification |
| D1 (1N4007) | Diode rectifier — converts the AC-ish audio signal into a one-directional envelope |
| C1 (2.2µF) | Smoothing capacitor — turns the rectified pulses into a slower-moving envelope |
| R5 / R6 (10kΩ / 10kΩ) | Bias network — centres the resting (silent) voltage within the ADC's readable range |

**On D1's part choice:** a 1N4007 (general-purpose power rectifier) is used here rather than the more typical small-signal choice (e.g. 1N4148), based on what was already on hand. The 1N4007 switches much slower than a dedicated signal diode — normally a mismatch for audio-frequency work, but not a practical problem here, since C1 is deliberately the slow part of this circuit already, turning the rectified signal into a "how loud is the music right now" envelope over hundreds of milliseconds. The diode's speed was never the limiting factor for something changing that slowly.

R3/R4 (divider ratio) and C1 (smoothing) are the values expected to need retuning once real audio is flowing — everything else is a reasonable starting point unlikely to need changing.

### Testing pinout (spare ESP32-C3 module)

| Pin/net | Connects to | Purpose |
|---|---|---|
| `GPIO1` | Conditioning circuit output (Node B) | ADC audio input — ADC1_CH1, same pin production uses |
| `GPIO7` | LED strip `DIN` | LED data output — free, non-strapping pin; confirm against your specific board's silkscreen |
| `3.3V` | Conditioning circuit bias network (R5/R6), LED strip `5V`* | Power |
| `GND` | Conditioning circuit ground, LED strip `GND`, ESP32-C3 `GND` | Common ground — all must share this one reference |

\*Most addressable strips want 5V for reliable operation; running directly off the module's 3.3V is commonly acceptable for a short bench-test wire run, but isn't the final production arrangement. This choice is deliberate, not just "good enough": powering the strip at 3.3V makes its data-logic threshold match the ESP32-C3's 3.3V `GPIO7` output exactly, avoiding the need for a level shifter on the bench. Powering at 5V instead (most ESP32-C3 modules break out a `5V`/`VIN` pin) risks the data line's 3.3V logic not reliably clearing the WS2812B's ~70%-of-VDD "HIGH" threshold without one — exactly why the production board has one (SN74AHCT125N).

---

## Production Circuit (Car, ESP32-C3)

Now that testing uses a spare ESP32-C3 module, this circuit is topologically identical to the testing circuit, down to the same `GPIO1` ADC pin — the only real difference is the input source (a phone's 3.5mm jack for testing vs. the real Front L/R RCA here) and which physical ESP32-C3 module it's wired to.

```
Front Left RCA  → R1 (2.2kΩ*) ──┐
Front Right RCA → R2 (2.2kΩ*) ──┼── Node S (mono sum)
RCA shield/ground → GND
   (shared ground reference for the whole circuit — see Ground
   note below, must be the RCA tap's own shield, not a separate
   chassis point)

Node S → R3 (10kΩ*) → Node A
Node A → R4 (1kΩ*) → GND

Node A → D1 (1N4007) → Node B

Node B → C1 (2.2µF*) → GND

Node B → R5 (10kΩ) → 3.3V
Node B → R6 (10kΩ) → GND

Node B → ESP32-C3 GPIO1 (ADC input, production board)
```

![Production circuit schematic](../images/audio-circuit/production_circuit_schematic.png)

`*` = expected to change once Phase 2 confirms real values. R5/R6 aren't marked — the bias network is unlikely to need tuning.

### Production pinout (ESP32-C3, already installed)

| Pin/net | Connects to | Purpose |
|---|---|---|
| `GPIO1` | Breakout board's conditioning circuit output (Node B) | ADC audio input — **new addition**, currently free |
| `LEFT_LED_PIN` / `RIGHT_LED_PIN` | Existing LED strips | **Unchanged** — reuses the current output path, no new strip or data pin |
| `3.3V` (existing PCB net) | Breakout board's `3.3V` in | Powers the new conditioning circuit |
| `GND` (existing PCB net) | Breakout board's `GND` — **which must trace to the RCA tap's own ground/shield**, not a separate chassis point | Common ground — see Ground note below |

The only genuinely new wiring on the production board is three connections to the breakout: `3.3V`, `GND`, and `GPIO1`. Everything else (LED strips, level shifter, encoder, accelerometer) is completely untouched.

---

## Ground Note

The conditioning circuit's ground must trace back to the RCA tap's own ground/shield connection — **not** a separate chassis ground point used elsewhere in the car (e.g. the 12V system's ground). Referencing two different chassis points that aren't at exactly the same potential is a classic source of audible ground-loop hum in the actual car audio system, not just an LED-behaviour issue.

## Tap Method

Bridge off both signal wires and the ground in parallel at the radio's RCA harness. Never cut into the cable and route the circuit in series — that would interrupt the original signal to the amplifier.

---

## Open Items (Before Phase 3)

- [ ] Confirm R1/R2 (summing resistors), R3/R4 (divider ratio), and C1 (smoothing cap) against real music through the car's actual radio and amp — see the build plan's Phase 2, Stage B
- [ ] Replace the `*`-marked placeholder values above with confirmed ones
- [ ] Confirm D1 (1N4007, chosen for availability rather than being a purpose-picked signal diode — see the note above for why that's expected to be fine, but not yet bench-verified)
