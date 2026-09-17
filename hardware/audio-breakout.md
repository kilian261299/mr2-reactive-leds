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

Input is a 3.5mm breakout cable (jack end into a phone or PC, stripped end wired to the circuit) rather than the car's RCA outputs, as a convenient stand-in for early tuning — same topology as the production circuit, just with a phone/PC headphone output instead of RCAs. Currently mono (red/R2 only) — see the white-wire note below for why.

```
Red wire (signal) → R2 (470Ω) ──┐
White wire (ground — see note) ─┤
Shield wire (ground) ───────────┴── GND

Node S → C2 (coupling cap, non-polarized, ~2.2–4.7µF) → Node A
Node A → R7 (100kΩ) → GND

Node A → D1 (1N4007) → Node B

Node B → C1 (2.2µF) → GND

Node B → R6 (10kΩ) → GND

Node B → ESP32-C3 (spare test module) GPIO1 (ADC input)

(R1, 470Ω: present for a future true left-channel input,
 currently unconnected — see white-wire note below)
```

A small monitoring speaker taps directly across the red wire and shield, *before* R2 — a separate parallel branch, not part of the conditioning circuit's signal path.

![Testing circuit schematic](../images/audio-circuit/testing_circuit_schematic.png)

**⚠ Out of date:** this image shows R5, R3/R4, and the old R1/R2 value (2.2kΩ), and predates C2/R7 entirely. Needs regenerating to match the component list above.

| Component | Role |
|---|---|
| R1 / R2 (470Ω) | Series input resistors, isolate L+R during summing and protect the source from a dead short — not for signal attenuation (see notes below) |
| C2 (~2.2–4.7µF, non-polarized) | Coupling capacitor — blocks any DC bias from the source, passes the real audio content through |
| R7 (100kΩ) | Reference resistor for C2 — gives the post-cap node a defined path to ground instead of leaving it floating |
| D1 (1N4007) | Diode rectifier — converts the AC-ish audio signal into a one-directional envelope |
| C1 (2.2µF) | Smoothing capacitor — turns the rectified pulses into a slower-moving envelope |
| R6 (10kΩ) | Discharge/bias resistor to GND — sets both the resting (silent) voltage (~0V) and, together with C1, the envelope's decay rate |

**White wire note:** on this particular breakout cable, "white" turned out to have continuity with the shield — a second ground/drain wire, not a real left-channel conductor (cheap 3.5mm pigtail cables don't reliably follow RCA's white=L/red=R convention). It ties to the same GND node as the shield. R1 stays in the circuit for a future genuine stereo source; its input is unconnected for now, so testing runs mono through R2 only.

**On D1's part choice:** a 1N4007 (general-purpose power rectifier) is used here rather than the more typical small-signal choice (e.g. 1N4148), based on what was already on hand. The 1N4007 switches much slower than a dedicated signal diode — normally a mismatch for audio-frequency work, but not a practical problem here, since C1 is deliberately the slow part of this circuit already, turning the rectified signal into a "how loud is the music right now" envelope over hundreds of milliseconds. The diode's speed was never the limiting factor for something changing that slowly.

**On dropping R5 (design correction):** earlier versions of this circuit paired R6 with a second resistor, R5, pulling Node B up to 3.3V — a two-resistor bias network centring the resting voltage at their midpoint, 1.65V. That's incompatible with D1 ever conducting at realistic signal levels: D1 only conducts once Node A exceeds Node B by its own forward-voltage drop, so a 1.65V resting point demands roughly 2V+ at Node A just to register anything. Removing R5 and keeping R6 alone as the only path to ground brings the resting point down to ~0V, so D1 only needs to clear its own ~0.3–0.6V forward drop — the standard single-resistor diode-envelope-detector topology. Secondary effect: R6 is now the sole discharge path (previously R5 || R6), so the decay time constant is slower than before.

**R3/R4 removed entirely, R1/R2 reduced 2.2kΩ → 470Ω (further correction, same root cause):** R3/R4 started at 10kΩ:1kΩ, then 1kΩ:1kΩ, each time sized as ADC-protection headroom against an unconfirmed car radio signal. Bench testing with the 1kΩ:1kΩ divider still in place showed no response to music at all — the combined attenuation through R2 (2.2kΩ) plus the R3/R4 divider left too little signal for D1 to ever clear its own conduction threshold. Removing R3/R4 entirely and reducing R1/R2 to 470Ω (still enough for channel isolation and short-circuit protection, without adding unnecessary attenuation) restored a real response on the bench.

**Car radio preout voltage — now confirmed, not estimated:** the Kenwood DPX-07MD's own service manual (`仕様一覧` / specifications page) lists `プリアウトレベル (FM): 1.8V/10kΩ` — a rated preout of 1.8V RMS, which works out to roughly 2.55V peak (1.8V × √2), replacing the earlier unconfirmed "~4V RMS / ~5.6V peak" guess. With R3/R4 gone, Node A sees close to that full peak directly (470Ω is too small to meaningfully attenuate it) — but C1 only ever charges to `peak − D1's forward drop`, landing around ~1.95–2.25V at Node B, still comfortably under the 3.3V ADC ceiling. The ADC-protection question is no longer a guess, and no dedicated divider is needed to provide it.

**C2/R7 — DC-blocking coupling capacitor, adopted for the `v2` PCB.** Bench testing showed a large, non-audio baseline jump the instant the laptop's cable was connected — most likely a DC bias specific to that laptop's headphone output (ground loops and a cable fault were both tested and ruled out). C2 blocks that steady bias while passing real audio through essentially untouched; R7 gives the post-cap node a defined reference instead of leaving it floating. Standard practice for audio inputs generally — while a proper automotive RCA preout is usually already internally AC-coupled (so the real radio may not have needed this), it's cheap insurance on a board being re-fabbed anyway.

**Component choice matters: a first attempt using two polarized electrolytics wired back-to-back failed and stayed failed, even after reseating.** That trick only works for signals that are purely symmetric AC with no sustained DC offset — the opposite of what's needed here, since blocking a *sustained* bias puts one of the two caps under continuous reverse bias rather than the brief, symmetric reverse-bias the trick tolerates. Sustained reverse bias degrades an electrolytic's leakage behaviour over time, which likely explains the reproducible failure — a wrong component choice, not a wiring fault. **C2 must be genuinely non-polarized** (ceramic or film) — no reverse-bias concern at all. Not yet re-tested on the bench with a proper non-polarized part; the `v2` PCB is the next validation point, alongside the real radio.

R1/R2 and C1 (smoothing, jointly setting decay rate with R6) are the values expected to need retuning once real audio is flowing.

### Testing pinout (spare ESP32-C3 module)

| Pin/net | Connects to | Purpose |
|---|---|---|
| `GPIO1` | Conditioning circuit output (Node B) | ADC audio input — ADC1_CH1, same pin production uses |
| `GPIO7` | LED strip `DIN` | LED data output — free, non-strapping pin; confirm against your specific board's silkscreen |
| `3.3V` | LED strip `5V`* (conditioning circuit no longer draws from 3.3V — R6 alone returns to GND) | Power |
| `GND` | Conditioning circuit ground, LED strip `GND`, ESP32-C3 `GND` | Common ground — all must share this one reference |

\*Most addressable strips want 5V for reliable operation; running directly off the module's 3.3V is commonly acceptable for a short bench-test wire run, but isn't the final production arrangement. This choice is deliberate, not just "good enough": powering the strip at 3.3V makes its data-logic threshold match the ESP32-C3's 3.3V `GPIO7` output exactly, avoiding the need for a level shifter on the bench. Powering at 5V instead (most ESP32-C3 modules break out a `5V`/`VIN` pin) risks the data line's 3.3V logic not reliably clearing the WS2812B's ~70%-of-VDD "HIGH" threshold without one — exactly why the production board has one (SN74AHCT125N).

---

## Production Circuit (Car, ESP32-C3)

Now that testing uses a spare ESP32-C3 module, this circuit is topologically identical to the testing circuit, down to the same `GPIO1` ADC pin — the only real difference is the input source (a phone's 3.5mm jack for testing vs. the real Front L/R RCA here) and which physical ESP32-C3 module it's wired to.

```
Front Left RCA  → R1 (470Ω*) ──┐
Front Right RCA → R2 (470Ω*) ──┼── Node S
RCA shield/ground → GND
   (shared ground reference for the whole circuit — see Ground
   note below, must be the RCA tap's own shield, not a separate
   chassis point)

Node S → C2 (coupling cap, non-polarized, ~2.2–4.7µF) → Node A
Node A → R7 (100kΩ) → GND

Node A → D1 (1N4007) → Node B

Node B → C1 (2.2µF*) → GND

Node B → R6 (10kΩ) → GND

Node B → ESP32-C3 GPIO1 (ADC input, production board)
```

![Production circuit schematic](../images/audio-circuit/production_circuit_schematic.png)

**⚠ Out of date:** this image shows R5, R3/R4, and the old R1/R2 value (2.2kΩ), and predates C2/R7 entirely. Needs regenerating to match the component list above.

`*` = expected to change once Phase 2 confirms real values. There is no longer a dedicated divider stage (R3/R4 removed); R1/R2 exist only for channel isolation and short-circuit protection. C2/R7 (coupling cap and its reference resistor) and R6 aren't marked — see the notes above for why they're protective/topology choices rather than level-tuned values.

### Production pinout (ESP32-C3, already installed)

| Pin/net | Connects to | Purpose |
|---|---|---|
| `GPIO1` | Breakout board's conditioning circuit output (Node B) | ADC audio input — **new addition**, currently free |
| `LEFT_LED_PIN` / `RIGHT_LED_PIN` | Existing LED strips | **Unchanged** — reuses the current output path, no new strip or data pin |
| `GND` (existing PCB net) | Breakout board's `GND` — **which must trace to the RCA tap's own ground/shield**, not a separate chassis point, and is R6's return path | Common ground — see Ground note below |

The only genuinely new wiring on the production board is two connections to the breakout: `GND` and `GPIO1`. No `3.3V` connection is needed — R5 was dropped from the design (see the component list note above). Everything else (LED strips, level shifter, encoder, accelerometer) is completely untouched.

---

## Ground Note

The conditioning circuit's ground must trace back to the RCA tap's own ground/shield connection — **not** a separate chassis ground point used elsewhere in the car (e.g. the 12V system's ground). Referencing two different chassis points that aren't at exactly the same potential is a classic source of audible ground-loop hum in the actual car audio system, not just an LED-behaviour issue.

## Tap Method

Bridge off both signal wires and the ground in parallel at the radio's RCA harness. Never cut into the cable and route the circuit in series — that would interrupt the original signal to the amplifier.

---

## Open Items (Before Phase 3)

- [ ] Confirm R1/R2 (summing/isolation resistors) and C1 (smoothing cap) against real music through the car's actual radio and amp — no longer practical pre-manufacture (the car's RCA wiring isn't easily accessible without opening up the already-installed system), so this now happens on the assembled `v2` board instead, before permanent install — see the build plan's Phase 3
- [ ] Replace the `*`-marked placeholder values above with confirmed ones
- [ ] Confirm D1 (1N4007, chosen for availability rather than being a purpose-picked signal diode — see the note above for why that's expected to be fine, but not yet bench-verified)
- [ ] Source a genuinely non-polarized capacitor for C2 (ceramic or film, ~2.2–4.7µF) — the earlier back-to-back-electrolytics attempt was the wrong component for this job, not a wiring fault, see the note above
- [ ] Source a JST-XH 3-pin connector set for the new RCA input, matching the board's existing J2–J5 style
- [ ] Regenerate the testing and production schematic images — both still show R5, R3/R4, and the old R1/R2 value, and predate C2/R7 entirely
