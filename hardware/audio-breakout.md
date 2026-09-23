# Audio Conditioning Breakout

Documents the small conditioning circuit for the audio-reactive LED feature — see [docs/audio-reactive-led-plan.md](../docs/audio-reactive-led-plan.md) for the full build plan this belongs to. This is a new, optional addition layered on top of the completed core project; it does not change the existing PCB, wiring, or firmware.

**Status: component values are confirmed as a working starting point from Phase 2 bench testing (laptop/phone audio), not yet confirmed against the real car radio.** Everything marked `*` below reflects this — final confirmation only happens during Phase 4's install, since the car's RCA wiring isn't accessible before then (Phase 3 is PCB fabrication and bench assembly only, no real radio involved). Fabricate `v2` with these values populated as-is; if Phase 4's real-radio test shows any need correcting, that's a simple hand-swap of the physical part, not a re-fab. Update this file with the truly final values only once that real-radio confirmation has happened.

---

## Purpose

Takes a line-level stereo audio signal (car radio's Front L/R RCA outputs) and converts it into a single, slowly-varying 0–3.3V envelope that an ESP32 ADC pin can read directly — the firmware then smooths that envelope and drives a bar-graph audio visualizer (a growing/shrinking bar of lit LEDs with a bouncing peak-hold marker) on the strip.

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

Input is a 3.5mm breakout cable (jack end into a phone or PC, stripped end wired to the circuit) rather than the car's RCA outputs, as a convenient stand-in for early tuning — same topology as the production circuit, just with a phone/PC headphone output instead of RCAs. Currently mono (red/R1 only) — see the white-wire note below for why.

```
Red wire (signal) → R1 (470Ω) → D1 (1N4007) → Node B
White wire (ground — see note) ─┐
Shield wire (ground) ───────────┴── GND

Node B → C1 (2.2µF) → GND

Node B → R6 (10kΩ) → GND

Node B → ESP32-C3 (spare test module) GPIO1 (ADC input)
```

A small monitoring speaker taps directly across the red wire and shield, *before* R1 — a separate parallel branch, not part of the conditioning circuit's signal path.

![Testing circuit schematic](../images/audio-circuit/testing_circuit_schematic.png)

| Component | Role |
|---|---|
| R1 (470Ω) | Series input resistor, isolates the source from a dead short — not for signal attenuation (see notes below) |
| D1 (1N4007) | Diode rectifier — converts the AC-ish audio signal into a one-directional envelope |
| C1 (2.2µF) | Smoothing capacitor — turns the rectified pulses into a slower-moving envelope |
| R6 (10kΩ) | Discharge/bias resistor to GND — sets both the resting (silent) voltage (~0V) and, together with C1, the envelope's decay rate |

**White wire note:** on this particular breakout cable, "white" turned out to have continuity with the shield — a second ground/drain wire, not a real left-channel conductor (cheap 3.5mm pigtail cables don't reliably follow RCA's white=L/red=R convention). It ties to the same GND node as the shield, alongside the shield wire itself. There's no second resistor for a left channel here — testing runs mono, through the single R1 in series with red.

**On D1's part choice:** a 1N4007 (general-purpose power rectifier) is used here rather than the more typical small-signal choice (e.g. 1N4148), based on what was already on hand. The 1N4007 switches much slower than a dedicated signal diode — normally a mismatch for audio-frequency work, but not a practical problem here, since C1 is deliberately the slow part of this circuit already, turning the rectified signal into a "how loud is the music right now" envelope over hundreds of milliseconds. The diode's speed was never the limiting factor for something changing that slowly.

**On dropping R5 (design correction):** earlier versions of this circuit paired R6 with a second resistor, R5, pulling Node B up to 3.3V — a two-resistor bias network centring the resting voltage at their midpoint, 1.65V. ([Archived schematic showing the original circuit with R5, and the R3/R4 divider before it was changed and later removed](../images/audio-circuit/testing_circuit_schematic_original.png) — kept for reference since the current schematic no longer shows these. **Note:** that image also labels the white wire as the left-channel audio input, reflecting the assumption at the time — later found to be wrong for this specific cable, whose white wire is actually a second ground/drain conductor; see the white-wire note above.) That's incompatible with D1 ever conducting at realistic signal levels: D1 only conducts once Node A exceeds Node B by its own forward-voltage drop, so a 1.65V resting point demands roughly 2V+ at Node A just to register anything. Removing R5 and keeping R6 alone as the only path to ground brings the resting point down to ~0V, so D1 only needs to clear its own ~0.3–0.6V forward drop — the standard single-resistor diode-envelope-detector topology. Secondary effect: R6 is now the sole discharge path (previously R5 || R6), so the decay time constant is slower than before.

**R3/R4 removed entirely, R1 (the resistor in the red/signal path) reduced 2.2kΩ → 470Ω (further correction, same root cause).** *(Heads up: "R3"/"R4" get reused below for different, unrelated components in the production designator scheme — see the renumbering note in the production section. These two facts aren't connected; the numbers just get recycled once they're free.)* R3/R4 started at 10kΩ:1kΩ, then 1kΩ:1kΩ, each time sized as ADC-protection headroom against an unconfirmed car radio signal. Bench testing with the 1kΩ:1kΩ divider still in place showed no response to music at all — the combined attenuation through R1 (2.2kΩ at the time) plus the R3/R4 divider left too little signal for D1 to ever clear its own conduction threshold. Removing R3/R4 entirely and reducing R1 to 470Ω (still enough for channel isolation and short-circuit protection, without adding unnecessary attenuation) restored a real response on the bench. Safe for production too — see the confirmed radio-voltage note in the production section for the resulting margin.

**A DC-blocking coupling capacitor was also tried on this breadboard, and reverted.** A large, non-audio baseline jump appeared the instant the laptop's audio cable was connected, before any music played — most likely a DC bias specific to that laptop's headphone output. A first attempt at fixing it (two polarized electrolytics wired back-to-back) failed and stayed failed even after reseating, so it was removed from the circuit shown above — it was never actually working on this breadboard. See the C4/R5 notes in the production section for the full diagnosis and why it's still going into `v2` despite that failure.

### Testing pinout (spare ESP32-C3 module)

| Pin/net | Connects to | Purpose |
|---|---|---|
| `GPIO1` | Conditioning circuit output (Node B) | ADC audio input — ADC1_CH1, same pin production uses |
| `GPIO7` | Level shifter input → LED strip `DIN` | LED data output — free, non-strapping pin; confirm against your specific board's silkscreen |
| `5V` / `VIN` | LED strip `5V` power input, and the level shifter's own supply | Power — see note* |
| `GND` | Conditioning circuit ground, LED strip `GND`, ESP32-C3 `GND`, level shifter `GND` | Common ground — all must share this one reference |

\*The strip is powered at its rated 5V (most ESP32-C3 modules break out a separate `5V`/`VIN` pin). A level shifter now sits between `GPIO7` and the strip's `DIN`, converting the ESP32-C3's 3.3V data logic up to a clean 5V signal — the same role the production board's SN74AHCT125N plays — so the bench setup no longer has the "3.3V logic driving a 5V strip directly" risk it started with. This also means the bench rig now matches the production power/logic arrangement more closely than the original 3.3V-everything bench-only workaround did.

---

## Production Circuit (Car, ESP32-C3)

Now that testing uses a spare ESP32-C3 module, this circuit is topologically similar to the testing circuit, down to the same `GPIO1` ADC pin — besides the input source (a phone's 3.5mm jack for testing vs. the real Front L/R RCA here), it also differs in D1 (BAT85 here vs. 1N4007 on the bench — see the note below) and the addition of C4/R5 (never actually validated on the breadboard, see the note below).

**Car radio preout voltage — confirmed, not estimated.** Only relevant here, since testing never involves the actual radio: the Kenwood DPX-07MD's own service manual (`仕様一覧` / specifications page) lists `プリアウトレベル (FM): 1.8V/10kΩ` — a rated preout of 1.8V RMS **per channel** (each channel has its own independent output stage), which works out to roughly 2.55V peak (1.8V × √2). R3/R4 sum L/R into Node S via two equal 470Ω resistors, which *averages* rather than adds the two channels — so Node S never exceeds a single channel's own peak, even with both simultaneously maxed out. See the corrected C4/R5 maths below for what actually reaches Node B — the original figure here (~1.95–2.25V) was based on an incomplete analysis and has been corrected.

```
Front Left RCA  → R3 (470Ω*) ──┐
Front Right RCA → R4 (470Ω*) ──┼── Node S
RCA shield/ground → GND
   (shared ground reference for the whole circuit — see Ground
   note below, must be the RCA tap's own shield, not a separate
   chassis point)

Node S → C4 (coupling cap, non-polarized, 2.2µF) → Node A
Node A → R5 (4.7kΩ) → GND

Node A → D1 (BAT85) → Node B

Node B → C3 (2.2µF*) → GND

Node B → R6 (10kΩ) → GND

Node B → ESP32-C3 GPIO1 (ADC input, production board)
```

![Production circuit schematic](../images/audio-circuit/production_circuit_schematic.png)

**Note: the production schematic uses different designators, and isn't just a renumbered copy of the testing circuit.** The testing circuit above only has R1 (isolation), R6 (discharge), and C1 (smoothing) — no coupling cap or reference resistor, since that stage was never validated on the breadboard, and only one isolation resistor since only one channel (red) turned out to be usable on this cable. Production genuinely needs two isolation resistors (R3 *and* R4, one per real stereo channel from the RCA tap) plus the coupling cap/reference resistor (C4/R5), so it isn't a 1:1 relabeling of the testing parts — see the production component list below for the full picture. Designators had to move regardless, since `v1`'s real board already uses `R1`/`R2` (330Ω) and `C1`/`C2` (1000µF/100nF) for other components, so production continues numbering straight on from those instead. (One quirk worth flagging: production's `R5` here is unrelated to "R5" in the R5-removal note above, which refers to a different, testing-circuit-only component from earlier in this circuit's history.)

`*` = confirmed as a working starting point from Phase 2 bench testing, not yet confirmed against the real car radio — that only happens during Phase 4's install (see the status note above). There is no longer a dedicated divider stage (the old testing-circuit divider, removed entirely); R3/R4 exist only for channel isolation and short-circuit protection. C4/R5 (coupling cap and its reference resistor) and R6 aren't marked — see the notes below for why they're protective/topology choices rather than level-tuned values.

**C4/R5 — DC-blocking coupling capacitor, added for production.** Bench testing (with the testing circuit's original C2/R7 naming, before the production renumbering) showed a large, non-audio baseline jump the instant the laptop's cable was connected — most likely a DC bias specific to that laptop's headphone output (ground loops and a cable fault were both tested and ruled out). C4 blocks that steady bias while passing real audio through essentially untouched; R5 gives the post-cap node a defined reference instead of leaving it floating. Standard practice for audio inputs generally — while a proper automotive RCA preout is usually already internally AC-coupled (so the real radio may not have needed this), it's cheap insurance on a board being re-fabbed anyway.

**Correction (found via Multisim simulation, after the board was already ordered): R5 was originally spec'd at 100kΩ, based on an incomplete analysis — corrected to 4.7kΩ.** The original reasoning only modelled C4/R5 as a simple AC high-pass filter (the transfer function below), and on that basis alone 100kΩ looked ideal. What it missed: because C4 blocks DC entirely, Node A's *average* voltage has to self-adjust to balance out the current D1 pulls on each signal peak — a capacitor can't sustain net DC current, so that peak current gets balanced by current flowing the other way through R5. The result is Node A's average sits well below 0V rather than centred on it, which eats directly into how much peak voltage ever reaches D1. This is the same self-biasing behaviour used deliberately in clamper/DC-restorer circuits — a real, known effect, just not one the original transfer-function-only analysis accounted for.

The steady-state relationship: `V_B ≈ (A×H − Vf) / (1 + R5/R6)`. At the original `R5 = 100kΩ`, that divisor is `(1 + 10) = 11` — roughly **91% of the signal reaching Node A never made it to Node B**, confirmed in Multisim against the real 2.55V peak (predicted Node B ≈ 0.2V, not the ~2.2–2.4V originally assumed).

**Fix: R5 lowered to 4.7kΩ** — not simply "as low as possible" (there's a genuine trade-off against bass response, and Node B's voltage actually peaks around R5 ≈ 5–6kΩ and falls off below that), but `4.7kΩ` sits within ~1% of that optimum using the exact `2.2µF` C4 already on hand. The AC transfer function itself (still correct on its own, just not the whole picture): `H(f) = R5 / √[(R5 + R_series)² + (1/(2πfC4))²]`, where R_series is R3 or R4 (470Ω). At 20Hz (worst case):

```
Z_C4(20Hz) = 1 / (2π × 20 × 0.0000022) ≈ 3,617Ω
H(20Hz) = 4,700 / √(5,170² + 3,617²) ≈ 0.74
```

~74% passes at the worst-case bass frequency (up to ~91% at typical midrange/treble, where C4's impedance becomes negligible). Combined with the self-bias factor (`1/(1+4700/10000) ≈ 68%`), Node B lands around **~1.1–1.15V at worst case, up to ~1.4V for typical content** against the confirmed 2.55V peak — a 5x+ improvement over the original spec, validated in Multisim before committing to the change. See `docs/audio-reactive-led-plan.md` for the full derivation and the R5-value trade-off table.

**Note on the `v2` PCB:** the EasyEDA schematic/BOM was finalized with `R5 = 100kΩ` before this was found. R5 is hand-soldered (already on the cheap-hand-swap list with R3/R4/C3/C4) — solder `4.7kΩ` instead when assembling, no re-fab needed.

**Component choice matters for C4 — a first attempt using two polarized electrolytics wired back-to-back failed and stayed failed, even after reseating.** That trick only works for signals that are purely symmetric AC with no sustained DC offset — the opposite of what's needed here, since blocking a *sustained* bias puts one of the two caps under continuous reverse bias rather than the brief, symmetric reverse-bias the trick tolerates. Sustained reverse bias degrades an electrolytic's leakage behaviour over time, which likely explains the reproducible failure — a wrong component choice, not a wiring fault. **C4 must be genuinely non-polarized** (ceramic or film) — no reverse-bias concern at all. Not yet re-tested on the bench with a proper non-polarized part; the `v2` PCB is the next validation point, alongside the real radio.

**D1 upgraded to a BAT85 Schottky diode for production**, replacing the 1N4007 the testing circuit still uses. The 1N4007 was picked purely for being on hand, not for suiting this job — it's a general-purpose power rectifier, not a signal diode. BAT85's much lower forward voltage (~0.15–0.3V vs. the 1N4007's estimated ~0.3–0.6V at these currents) matters more than ever now: quiet passages need to register without vanishing below the diode's turn-on point, and the self-bias correction above already shows how much of the available signal gets used up before D1 even sees it — a standard choice for this kind of low-level envelope-detector duty. Cheap, through-hole, easy to hand-solder.

Node B's actual voltage (worked out in the corrected C4/R5 maths above, which already accounts for D1's forward drop) lands around **~1.1–1.4V** against the confirmed 2.55V peak — comfortably clear of the 3.3V ADC ceiling, with even more margin than the original (incorrect) figures suggested.

### Production pinout (ESP32-C3, already installed)

| Pin/net | Connects to | Purpose |
|---|---|---|
| `GPIO1` | Breakout board's conditioning circuit output (Node B) | ADC audio input — **new addition**, currently free |
| `LEFT_LED_PIN` / `RIGHT_LED_PIN` | Existing LED strips | **Unchanged** — reuses the current output path, no new strip or data pin |
| `GND` (existing PCB net) | Breakout board's `GND` — **which must trace to the RCA tap's own ground/shield**, not a separate chassis point, and is R6's return path | Common ground — see Ground note below |

The only genuinely new wiring on the production board is two connections to the breakout: `GND` and `GPIO1`. No `3.3V` connection is needed — the old bias pull-up resistor was dropped from the design (see the component list note above). Everything else (LED strips, level shifter, encoder, accelerometer) is completely untouched.

**`J6_SWITCH` removed.** `v1`'s BOM includes a 2-pin switch connector that was never actually populated — the physical toggle switch didn't fit, so that connector is currently just shorted instead (see `README.md`). Dropped entirely for `v2`; the new RCA connector is `J7` rather than reusing `J6`, to keep the freed-up number from meaning something unrelated between board revisions.

---

## Ground Note

The conditioning circuit's ground must trace back to the RCA tap's own ground/shield connection — **not** a separate chassis ground point used elsewhere in the car (e.g. the 12V system's ground). Referencing two different chassis points that aren't at exactly the same potential is a classic source of audible ground-loop hum in the actual car audio system, not just an LED-behaviour issue.

## Tap Method

Bridge off both signal wires and the ground in parallel at the radio's RCA harness. Never cut into the cable and route the circuit in series — that would interrupt the original signal to the amplifier.

---

## Open Items (Phase 3, in progress — board ordered, in fabrication/shipping)

- [x] Source a BAT85 Schottky diode (or equivalent) for D1 on the production board — replacing the 1N4007 the testing circuit still uses, see the note above. Ordered.
- [x] Source a genuinely non-polarized capacitor for C4 (ceramic or film, ~2.2–4.7µF) — the earlier back-to-back-electrolytics attempt was the wrong component for this job, not a wiring fault, see the note above. Ordered.
- [x] Source a JST-XH 3-pin connector set for the new RCA input (`J7`), matching the board's existing J2–J5 style. Ordered.
- [x] Clone the `v1` EasyEDA project rather than editing it in place, and remove `J6_SWITCH` (never populated — see the note above)
- [x] Regenerate the production schematic image to show D1 as BAT85 and the renumbered R3/R4/R5/C3/C4 designators — the testing schematic is already current and doesn't need this renumbering, since it isn't built from real EasyEDA designators
- [x] Design the `v2` schematic/layout in EasyEDA — DRC clean (0 errors), schematic connectivity manually verified against the exported netlist (Node S/A/B topology and GND all match this document); `J7`'s Front Left/Front Right wiring confirmed matching R3=Left/R4=Right
- [x] Export and commit the design files (`gerbers/`, `bom/`, `easyeda/`) under `hardware/pcb/v2/` — see [hardware/README.md](README.md)
- [x] Submit Gerbers to JLCPCB — ordered, Global Standard Direct Line shipping, 8–12 business days quoted
- [x] R5 corrected from `100kΩ` to `4.7kΩ` before assembly (found via Multisim simulation after Gerbers were already submitted — see the C4/R5 maths note above) — solder `4.7kΩ`, not the value shown on the `v2` schematic
- [ ] Assemble and bench-test the new board once it arrives (visual inspection, continuity, power-on — no real radio involved yet, see the build plan's Phase 3)
