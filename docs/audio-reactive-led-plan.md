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
Firmware: smooth (fast attack, slow release), drive a bar-graph visualizer
        │
        ▼
WS2812B LED strips (existing LEFT_LED_PIN / RIGHT_LED_PIN)
```

### Testing setup — full pipeline

![Testing setup overview](../images/audio-circuit/testing_setup_overview.png)

This is the complete bench-test signal path: a 3.5mm breakout cable (jack end into a phone or PC, stripped end wired to the circuit), summed and conditioned, into a spare ESP32-C3 module, driving the addressable LED strip. This is the testing configuration only — the production install uses the car's actual RCA tap and the ESP32-C3 installed in the car, not a phone/PC or this spare board.

A small monitoring speaker is also tapped across the R wire and shared ground (not part of the conditioning circuit itself) so you can hear what's actually playing while watching the LED/Serial response — useful for correlating specific sounds (bass hits, vocals, silence) with the tuned behaviour during Stage A/B below.

**Testing board update:** originally planned around a full ESP32 dev board (`GPIO34`/`GPIO5`). A spare ESP32-C3 module turned out to be available instead, which is actually simpler: it's the same chip as production, so the audio ADC pin (`GPIO1`) is identical on both, and Phase 2 tuning carries straight into Phase 4 with no pin remapping. See the updated pinout table below.

### Testing circuit — schematic

![Testing circuit schematic](../images/audio-circuit/testing_circuit_schematic.png)

### Testing circuit — component list

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

A small monitoring speaker taps directly across the red wire and shield, *before* R2 — a separate parallel branch off the raw source connection, not part of the conditioning circuit's signal path.

**White wire note:** on this particular breakout cable, the "white" wire turned out to have continuity with the shield — it's a second ground/drain wire, not a real left-channel conductor (common on cheap 3.5mm pigtail cables, which don't follow RCA's white=L/red=R convention). It ties to the same GND node as the shield. R1 stays in the circuit for whenever a genuine stereo source is available, but its input side is unconnected for now — testing currently runs mono, through R2 only.

**R5 removed (design correction):** the original two-resistor bias network (R5 to 3.3V, R6 to GND) set Node B's resting/silent voltage at their midpoint, 1.65V. ([Archived schematic showing the original circuit with R5, and the R3/R4 divider before it was changed and later removed](../images/audio-circuit/testing_circuit_schematic_original.png) — kept for reference since the current schematic no longer shows these. **Note:** that image also labels the white wire as the left-channel audio input, reflecting the assumption at the time — later found to be wrong for this specific cable, whose white wire is actually a second ground/drain conductor; see the white-wire note above.) Worked through numerically against the divider's realistic output at the time, that's a problem: D1 only conducts once Node A exceeds Node B's voltage by its own forward-voltage drop, so a 1.65V resting point demands roughly 2V+ at Node A just to register anything — but Node A's actual peak was nowhere near that. The diode would essentially never conduct; the circuit would sit at a fixed ~1.65V regardless of music. Dropping R5 and keeping R6 alone as the only path to ground brings the resting point down to ~0V instead, so D1 only needs to clear its own ~0.3–0.6V forward drop. This is also just the standard, single-resistor diode-envelope-detector topology; R5 wasn't buying anything a plain design doesn't already handle better.

**R3/R4 removed entirely, R1/R2 reduced 2.2kΩ → 470Ω (further design correction, same root cause).** *(Heads up: "R3"/"R4" get reused below for different, unrelated components in the production designator scheme — see the renumbering note in the production section. These two facts aren't connected; the numbers just get recycled once they're free.)* R3/R4 started at 10kΩ:1kΩ, then 1kΩ:1kΩ, sized each time as ADC-protection headroom against an unconfirmed car radio signal. Bench testing with the 1kΩ:1kΩ divider still in place showed the circuit essentially dead — no response to music at all — because the combined attenuation through R2 (2.2kΩ) plus the R3/R4 divider left too little signal for D1 to ever clear its own conduction threshold. Removing R3/R4 entirely and reducing R1/R2 to 470Ω (still enough to isolate the two channels during summing and protect the source from a dead short, just without adding unnecessary attenuation on top) restored a real, visible response on the bench. This is safe for the production circuit too: with no divider at all, the ADC is protected purely by D1's own forward drop — see the confirmed radio-voltage note in the production section for the resulting margin.

Values are starting points for Phase 2 — expect to retune R1/R2 and C1 (smoothing, jointly setting decay rate with R6) once you're actually watching the LED respond to real music.

### Production setup — full pipeline

![Production setup overview](../images/audio-circuit/production_setup_overview.png)

Same Y-split concept as the testing pipeline, now with the real components: the radio's Front L/R RCA (plus shield/ground) splits to the amplifier, completely unchanged, and separately to the new conditioning circuit → ESP32-C3 → the existing LED strips. No new strip or amp wiring — everything downstream of the ESP32-C3 already exists.

### Production circuit — schematic

![Production circuit schematic](../images/audio-circuit/production_circuit_schematic.png)

### Production circuit — component list

```
Front Left RCA  → R3 (470Ω*) ──┐
Front Right RCA → R4 (470Ω*) ──┼── Node S
RCA shield/ground → GND
   (shared ground reference for the whole circuit — see Ground
   note below, this must be the RCA tap's own shield, not a
   separate chassis point)

Node S → C4 (coupling cap, non-polarized, ~2.2–4.7µF) → Node A
Node A → R5 (100kΩ) → GND

Node A → D1 (BAT85) → Node B

Node B → C3 (2.2µF*) → GND

Node B → R6 (10kΩ) → GND

Node B → ESP32-C3 GPIO1 (ADC input, production board)
```

**Note: resistor/capacitor designators were renumbered for the production schematic.** The testing-circuit list above uses R1, R2, R6, R7, C1, C2; the production list uses R3–R6, C3, C4 instead. This is deliberate, not a typo — the testing circuit is breadboard-only and never gets loaded into EasyEDA, but the production designators have to avoid colliding with names `v1`'s real board already uses elsewhere (`R1`/`R2` = 330Ω, `C1`/`C2` = 1000µF/100nF), so they continue numbering straight on from `v1`'s existing `R1`/`R2` instead. (One quirk worth flagging: production's `R5` here is unrelated to "R5" in the R5-removal note below, which refers to a different, testing-circuit-only component from earlier in this circuit's history.)

`*` marks values confirmed as a *working starting point* from Phase 2 bench testing (laptop/phone audio) — currently the same as the testing circuit's values. They are **not yet confirmed against the real car radio**; that only happens in Phase 3, on the assembled board, since the car's RCA wiring isn't accessible before then. Fabricate `v2` with these values populated as-is — if Phase 3's real-radio test shows they need correcting, that's a simple hand-swap of the physical part on the assembled board, not a re-fab (see Phase 3's checklist). Only remove the `*` and update this list once that real-radio confirmation has actually happened. There is no longer a dedicated divider stage (the old R3/R4 divider from the testing-circuit's history, removed entirely — see the note above); R3/R4 here exist only to isolate the two channels during summing and protect the source from a dead short, not to attenuate the signal, and the ADC-protection margin now comes purely from D1's own forward drop against the Kenwood DPX-07MD's confirmed preout spec. C4/R5 (the coupling cap and its reference resistor) aren't marked, since they're a protective addition rather than a level-tuned value — see the note above for why C4 specifically must be non-polarized. R6 isn't marked either, for the same reason — but see the R5-removal note above: earlier versions of this doc paired the discharge resistor with a pull-up to 3.3V that turned out to make D1 never conduct at realistic signal levels, so that pull-up has been dropped from the design entirely.

Now that testing uses a spare ESP32-C3 module (see "Testing board update" above), this circuit and the testing circuit are topologically similar, down to the same `GPIO1` ADC pin — but no longer identical: besides the input source (a phone's 3.5mm jack for testing vs. the real Front L/R RCA here) and which physical ESP32-C3 module it's wired to, production also adds C4/R5 and uses D1 as a BAT85 rather than the testing circuit's 1N4007 — neither was ever part of the validated breadboard circuit, so both are explained here rather than in the testing section above.

**Car radio preout voltage — now confirmed, not estimated.** Only relevant to production, since testing never involves the actual radio: the Kenwood DPX-07MD's own service manual (`仕様一覧` / specifications page) lists `プリアウトレベル (FM): 1.8V/10kΩ` — a rated preout level of 1.8V RMS into a 10kΩ load. That's the output stage's own voltage-swing ceiling (the same hardware drives every source — CD, MD, AUX, tuner — the "(FM)" just notes the test condition used to rate it), so a full-scale peak works out to roughly 1.8V × √2 ≈ **2.55V peak** — replacing the earlier unconfirmed "~4V RMS / ~5.6V peak" guess entirely. With R3/R4 (the old divider) removed, Node A sees close to that full 2.55V peak directly (R3/R4 at 470Ω is too small to meaningfully attenuate it) — but C3 only ever charges to `peak − D1's forward drop`, landing around **1.95–2.25V** at Node B, still comfortably under the ADC's 3.3V ceiling (1V+ margin). The production circuit's ADC-protection margin is no longer a guess, and no dedicated divider is needed to provide it.

**C4/R5 — DC-blocking coupling capacitor, added for production.** Bench testing (with the testing circuit's original C2/R7, before the production renumbering) surfaced a large, non-audio baseline jump the instant the laptop's audio cable was connected, before any music played — most likely a DC bias specific to that laptop's headphone output; ground loops and cable faults were both tested and ruled out as the cause. C4 (a coupling cap between Node S and Node A) blocks that steady bias while passing real audio through essentially untouched; R5 gives the post-cap node a defined reference to ground rather than leaving it floating. This is standard practice for audio inputs generally, and while a proper automotive RCA preout is usually already internally AC-coupled (so the real radio may never have needed this), it's cheap insurance on a board being re-fabbed anyway, and directly protects against the class of problem just observed on the bench.

**The maths behind why C4/R5 barely touch the real signal:** R5 (100kΩ) so thoroughly dominates the circuit that adding C4 in series changes almost nothing. The transfer function from Node S to Node A is:

```
H(f) = R5 / √[(R5 + R_series)² + (1 / (2πfC4))²]
```

where R_series is R3 or R4 (470Ω, whichever channel is driving). Working this out at 20Hz — the worst case, since a capacitor's impedance is highest at the lowest frequency, and bass content is exactly where losing signal would matter most — with C4 at its smallest recommended value (2.2µF, since a smaller cap has higher impedance and is the more conservative case to check):

```
Z_C4(20Hz) = 1 / (2π × 20 × 0.0000022) ≈ 3,617Ω
R5 + R_series = 100,000 + 470 = 100,470
H(20Hz) = 100,000 / √(100,470² + 3,617²) = 100,000 / √10,107,303,589 ≈ 0.9947
```

So even at 20Hz, roughly **99.5% of the signal passes through** — C4/R5 cost almost nothing in signal strength, at any frequency in the audible range (higher frequencies are attenuated even less, since C4's impedance keeps shrinking). Applied to the confirmed radio peak: 2.55V × 0.9947 ≈ 2.54V reaches Node A, essentially unchanged from the no-C4 case.

**D1 upgraded to a BAT85 Schottky diode for production** (testing circuit keeps the 1N4007 it's already validated with). The 1N4007 was originally picked purely for being on hand, not for suiting this job — it's a general-purpose power rectifier, not a signal diode. A Schottky like the BAT85 has a much lower forward voltage (~0.15–0.3V vs. the 1N4007's estimated ~0.3–0.6V at these tiny currents). BAT85 is a standard choice for exactly this kind of low-level envelope-detector duty (it's a textbook part for AM-detector circuits, the same basic task). Cheap, through-hole, easy to hand-solder alongside everything else on the board.

**The maths behind why this works comfortably:** taking the confirmed 2.54V that reaches Node A (from the C4/R5 calculation above), C1 only ever charges to `peak − D1's forward drop`:

```
Node B (BAT85, low estimate):  2.54V − 0.15V = 2.39V
Node B (BAT85, high estimate): 2.54V − 0.30V = 2.24V
Node B (1N4007, for comparison): 2.54V − 0.30 to 0.60V = 2.24V to 1.94V
```

Either diode clears the 3.3V ADC ceiling with real margin (0.75V+ to spare) for loud content — that question was already settled once R3/R4's old divider was removed. What BAT85 actually buys is at the *quiet* end: its lower threshold means less signal is needed before D1 conducts at all, so quiet passages register instead of vanishing below the diode's turn-on point — the same reasoning that made C4/R5 worth adding now that the circuit's resting baseline is genuinely near 0V instead of an elevated bias.

**Component choice matters for C4 — a first attempt using two polarized electrolytics wired back-to-back failed and stayed failed, even after reseating.** That trick (tying two same-polarity electrolytics together to approximate a non-polarized cap) only works for a signal that's purely symmetric AC with no sustained DC offset — exactly the opposite of what C4 needs to handle here, since blocking a *sustained* DC bias means one of the two caps in that pair ends up under continuous reverse bias, not the brief, symmetric reverse-bias the trick is designed to tolerate. Sustained reverse bias degrades an electrolytic's leakage behaviour over time, which likely explains why the failure was reproducible — it wasn't a wiring fault, it was the wrong kind of capacitor for this specific job. **C4 must be a genuinely non-polarized capacitor** (ceramic or film, not two electrolytics) — no reverse-bias concern at all, regardless of which direction (or whether) a DC bias is present. Not yet re-tested on the bench with a proper non-polarized part — the `v2` PCB is the next point this gets validated, alongside the real radio.

**Ground note:** the conditioning circuit's ground must trace back to the RCA tap's own ground/shield connection — not a separate chassis ground point used elsewhere in the car (e.g. the 12V system's ground). Referencing two different chassis points that aren't at exactly the same potential is a classic source of audible ground-loop hum in the actual audio system, not just an LED-behaviour issue.

**Tap method:** bridge off both signal wires and the ground in parallel at the radio's RCA harness. Never cut into the cable and route the circuit in series — that would interrupt the original signal to the amplifier.

### Pinout — testing board (spare ESP32-C3 module)

Used for bench testing only — a separate, spare ESP32-C3 module, not the one installed in the car.

| Pin/net | Connects to | Purpose |
|---|---|---|
| `GPIO1` | Conditioning circuit output (Node B) | ADC audio input — ADC1_CH1, same pin the production board uses |
| `GPIO7` | Level shifter input → LED strip `DIN` | LED data output — free, non-strapping pin; confirm against your specific board's silkscreen |
| `5V` / `VIN` | LED strip `5V` power input, and the level shifter's own supply | Power — see note* |
| `GND` | Conditioning circuit ground, LED strip `GND`, ESP32-C3 `GND`, level shifter `GND` | Common ground — all must share this one reference |

*The strip is powered at its rated 5V here (most ESP32-C3 modules break out a separate `5V`/`VIN` pin). A level shifter now sits between `GPIO7` and the strip's `DIN`, converting the ESP32-C3's 3.3V data logic up to a clean 5V signal — the same role the production board's SN74AHCT125N plays — so the bench setup no longer has the "3.3V logic driving a 5V strip directly" risk it started with, and now matches the production power/logic arrangement more closely than the original bench-only 3.3V workaround did.

### Pinout — production board (ESP32-C3, new PCB revision)

| Pin/net | Connects to | Purpose |
|---|---|---|
| `GPIO1` | Conditioning circuit output (Node B), routed directly on the new PCB | ADC audio input — **new addition**, currently free |
| `LEFT_LED_PIN` / `RIGHT_LED_PIN` | Existing LED strips | **Unchanged** — reuses the current output path, no new strip or data pin |
| `GND` (existing board net) | Conditioning circuit ground — **which must trace to the RCA tap's own ground/shield**, tied into the board's single shared ground plane, and is R6's return path (the discharge/bias resistor) | Common ground — see Ground note above |

The audio circuit is now part of the same PCB as everything else — R3, R4, R5, R6, C3, C4, and D1 sit alongside the existing components, with a new **JST-XH connector (`J7`)** for the RCA input (matching the J2–J5 style and 2.54mm pitch already used for the LED outputs, MPU6050, and encoder — `J6` was the now-removed switch connector, see below). No `3V3` connection is needed for the conditioning circuit itself — the old bias pull-up resistor was dropped from the design (see the component list note above), so nothing in this circuit draws from the 3.3V rail. Nothing about the LED strips, level shifter, encoder, or accelerometer changes.

**`J6_SWITCH` removed.** `v1`'s BOM includes a 2-pin switch connector that was never actually populated — the physical toggle switch didn't fit, so that connector is currently just shorted instead (see `README.md`). Dropped entirely for `v2` rather than carried forward unused.

---

## Phase 1: GitHub setup — Complete

The repo scaffolding is done: this plan document and its images are committed at `docs/audio-reactive-led-plan.md`, the bench-test sketch exists at `firmware/tests/04_audio_reactive_test/`, `hardware/audio-breakout.md` documents the conditioning circuit, and a new build log section tracks this as a separate addition on top of the completed core project. The v2.x firmware line was left untouched throughout.

**Note:** `hardware/audio-breakout.md`'s content will need revisiting once Phase 3 begins, since it currently describes a standalone breakout board — Phase 3 below has since changed to a full PCB integration instead.

---

## Phase 2: Build and bench-test

Using a spare ESP32-C3 module and the real addressable LED strip found for testing — **the ESP32-C3 installed in the car is not touched during this phase**, avoiding any risk to the already-working, installed v2.4.1 firmware. (Originally planned around a full ESP32 dev board — see the "Testing board update" note above for why a spare ESP32-C3 module is used instead.)

**Stage A — initial tuning, laptop/phone audio (the only validation available before Phase 3): validated, working.**
- [x] Breadboard the conditioning circuit using the component list above
- [x] Wire audio output to `GPIO1`, LED data to `GPIO7` (or confirmed equivalents)
- [x] Flash the Phase 1 test sketch — since rewritten as a bar-graph audio visualizer (growing/shrinking LED bar with a bouncing peak-hold marker, fast attack/slow release) rather than uniform whole-strip brightness; see `firmware/tests/04_audio_reactive_test/`
- [x] Feed it laptop/phone headphone audio (line-level output, good first stand-in for the radio) — via a USB-C-to-3.5mm dongle; the laptop's built-in 3.5mm jack was tried and found too quiet even at max volume, reverted to the dongle
- [x] Tune R1/R2 and C1 (smoothing) by watching how the LED actually responds — confirmed working with R1/R2 at 470Ω, C1 unchanged at 2.2µF; `audioFloor`/`audioCeiling` calibrated from real Serial readings to `900`/`1300` (idle settled ~950, loud peaks ~1200)

**Known issue, not yet fixed:** the small USB-C-to-3.5mm dongle's audio occasionally distorts/corrupts mid-playback, recovering temporarily after unplugging and reconnecting it. Most likely cause: the monitoring speaker (8Ω) is a much lower-impedance load than the dongle's tiny onboard amp is rated to drive continuously, causing it to overload/distort over time. Since the speaker taps the same raw wire the conditioning circuit reads from, this could affect the LED response too during those moments, not just the audible sound. Left unaddressed for now since it hasn't blocked validation; a series resistor (~47–100Ω) between the source and the speaker, or swapping to a powered/amplified speaker, would fix it if it becomes a problem.

**Real-radio validation moved to Phase 3.** The car's actual RCA wiring isn't practically accessible without opening up the already-installed system — not worth the risk to a working install just to bench-test. Two consequences, both accepted:

- **The head unit's actual preamp output voltage is now confirmed.** The Kenwood DPX-07MD's own service manual (`仕様一覧` / specifications page) lists `プリアウトレベル (FM): 1.8V/10kΩ` — a rated preout of 1.8V RMS (~2.55V peak), well inside what was previously assumed (~2–4V RMS). Switching the head unit's internal amp off (already done here, since an external amp is used) doesn't affect this — RCA preamp outs are a separate signal path from the internal amp regardless of that setting. See the confirmed-voltage note above for what this means for ADC-protection margin now that there's no dedicated divider.
- **R1/R2 and C1 should still be tuned by ear/Serial-monitor on the bench.** Watch how loud laptop content lands on the ADC's range and adjust `audioFloor`/`audioCeiling` in firmware to match what you actually see, rather than trusting the placeholder 200/3000 defaults — the confirmed spec means overshoot/clipping risk is low, but the real radio's exact levels can still differ from a laptop's. See Phase 3 for validating this in the car once real hardware is available.

- [x] **Record the working component values and firmware `audioFloor`/`audioCeiling` from Stage A** — R1/R2 = 470Ω, R3/R4 removed, C1 = 2.2µF unchanged, `audioFloor` = 900, `audioCeiling` = 1300; carries into Phase 3 as a starting point, not a final answer

---

## Phase 3: Remanufacture the PCB with the audio circuit integrated

Decided against a standalone breakout board — the audio conditioning circuit will instead be added directly to a new PCB revision, following the same EasyEDA → JLCPCB process already used for the original board (including its GPIO4-fault replacement revision).

**Parts to source before starting** (everything else needed is already on hand from Phase 2 bench testing or the original `v1` build):
- Coupling capacitor (`C4` on the real board) — genuinely **non-polarized** (ceramic or film, not electrolytic), ~2.2–4.7µF, 16V+ rating. Worth getting a couple of different values, since the best one may shift once tested against the real radio.
- JST-XH 3-pin connector set (male PCB header + female housing + crimp pins), 2.54mm pitch, matching the J2–J5 connectors already on the board — for `J7`, carrying Front Left, Front Right, and shield/ground from the RCA tap.
- D1 — a **BAT85** Schottky diode (or equivalent, e.g. 1N5711/1N5817), replacing the 1N4007 for production — see the diode note above.

- [ ] Confirm the RGB/addressable LED and conditioning circuit values from Phase 2
- [ ] Clone the `v1` EasyEDA project rather than editing it in place — keeps the original, still-installed board's design intact
- [ ] Remove `J6_SWITCH` — never populated on `v1` (shorted instead of a real switch), no reason to carry it forward
- [ ] Add `R3`, `R4`, `R5`, `R6`, `C4`, `D1` (BAT85), and `C3` as new components, matching the confirmed values (the old bias pull-up resistor and the old divider, both from the testing-circuit's history, stay dropped from the design — see the component list note above). **Note the renumbering**: `v1` already uses `R1`/`R2` and `C1`/`C2` for other components, so the audio circuit's resistors/caps continue on as `R3`–`R6`/`C3`/`C4` instead — see the note above the production component list for the full mapping.
- [ ] Add the new JST-XH connector (`J7`) for the RCA input (Front Left, Front Right, shield/ground), matching the existing J2–J5 connector style already used for the LED outputs, MPU6050, and encoder
- [ ] Route the conditioning circuit's output to a free ADC-capable pin — `GPIO1`, per the pinout table above
- [ ] Tie the new circuit's ground into the board's existing GND net — no separate ground path needed, since the whole board (and now the audio circuit too) already shares one common ground plane; no 3V3 connection is needed for this circuit
- [ ] Run design checks (ERC/DRC) same as the original board
- [ ] Submit Gerbers to JLCPCB
- [ ] Assemble the new board once manufactured, bench-test before installing (same validation approach used for the original PCB and its replacement)
- [ ] **First real validation against the actual car radio happens here**, on the assembled board, before permanent (re)install — this is the earliest point real RCA access is practical. Confirm the ADC isn't clipping (a maxed-out, unresponsive-to-volume reading is the signature), that the idle baseline actually sits near-zero (confirming C4 is doing its job against whatever the real radio's DC characteristics turn out to be), and that C3's smoothing still feels right against real music.
- [ ] If R3/R4/C3/C4 need correction, hand-swap those specific parts on the assembled board — cheap, quick, doesn't require a re-fab, since the values from Stage A were only ever a laptop-derived starting point
- [ ] Save the new design files under `hardware/pcb/v2/` (gerbers/, bom/, easyeda/), matching the structure the original board's files already use under `hardware/pcb/v1/` — see the note in `hardware/README.md` on the versioned PCB folder layout

---

## Phase 4: Install and integrate the firmware

- [ ] Install the new PCB revision in the control box, replacing the currently-installed board
- [ ] Tap Front Left + Front Right RCA, plus ground/shield, in parallel at the radio harness (see Ground note above)
- [ ] Add a new mode to the real firmware (new version, e.g. v2.5, branching from the current final v2.4.1), using Phase 2's tuned values, driving the actual strips through the existing `setStrip()` / NeoPixel functions
- [ ] Test in the car with real music — expect some retuning against real strips, cabin acoustics, and road noise
- [ ] Update `docs/build-log.md` and the firmware changelog with results, including documenting the new PCB revision (matching how the original GPIO4-fault replacement was documented)
