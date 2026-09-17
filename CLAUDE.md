# MR2 Reactive LEDs — Project Status

Reactive LED lighting controller (ESP32-C3 + MPU6050 + WS2812B) built and installed in a 1992 Toyota MR2 SW20. See [README.md](README.md) for what the system does.

## Core Project Complete

All physical installation is **complete** — control box, accelerometer, rotary encoder, and both LED strips are mounted in their final positions, running on real vehicle power (fused 12V from the cigarette lighter circuit). Rotary encoder brightness adjustment and LED strip output were confirmed working correctly in the car at this initial installation. Hardware/PCB work is done, including a GPIO4 fault on the first assembled board that was resolved by swapping to a spare PCB (root cause: damaged GPIO4 on that specific ESP32-C3 module, not a design fault).

The originally specified buck converter was swapped for a repurposed USB charger module after it caused intermittent cold-boot failures (likely an unclean voltage rise affecting ESP32-C3 strapping pins on boot). The replacement has since been cold-boot tested repeatedly with no recurrence — **confirmed fixed**.

**`v2.3`** was confirmed on real driving to work "almost perfectly" — `accelerationResponseG` tuned to `0.12` so higher gears reliably reach true orange, and Modes 1–4 fixed (via code review, before that drive) to actually track that same tuning instead of a stale hardcoded threshold. But it also surfaced a **new issue**, not present on earlier versions: flickering between red (braking) and blue (idle) while going downhill, with no braking input needed at all.

That flicker was traced to **v2.1 → v2.2**, not to v2.3's own changes: v2.2 slowed `gravitySmoothing` and made `baselineDynamicReentryThreshold` more sensitive, both specifically to stop sustained acceleration fading too early — but the same `gravitySmoothing` tracker also governs how fast a downhill grade gets absorbed into the baseline, so the fix for one problem quietly created another. It was initially assumed to be an inherent, unfixable accelerometer-only limitation; it wasn't.

**`v2.4.0`** attempted the fix by splitting `gravitySmoothing` by direction on the forward axis — the slow rate stays for accelerating (so the acceleration-hold fix is untouched), while the original fast v2.1 rate is restored for braking/downhill (which share a sign on the forward axis). **Drive-tested and found to make no difference at all** — downhill flicker unchanged, uphill/flat unchanged (both were already fine, so that's not evidence of success). Root cause: a hill pitch shifts gravity on the forward axis (X) *and* the vertical axis (Z) at once, since both are involved in the same rotation — v2.4.0 only made `gravityX` direction-aware, and `gravityZ`'s unchanged slow rate kept the combined gating signal (which sums all three axes) elevated regardless, so the baseline never settled any faster than on v2.3.

**`v2.4.1` is the final firmware version, confirmed on real driving.** It extends the same direction-aware rate to `gravityZ` as well (same forward-axis direction flag; `gravityY`, the lateral/cornering axis, is untouched) — this directly targeted the flicker without the blunt trade-offs considered earlier (e.g. dulling `brakingDeadZone` globally), and the next drive confirmed it worked: **the downhill flicker is gone**, acceleration-hold and braking both feel unchanged from v2.2/v2.3. `baselineDynamicReentryThreshold` (the fallback if this hadn't worked) is no longer a live concern.

**The core reactive-LED project is finished.** Two known issues remain, deliberately left unfixed — accepted, permanent trade-offs, not bugs to chase:
- USB power backfeeds enough current to power the car radio when connected with the key off (no reverse-blocking diode on the charger module). Not a safety/drain issue since it doesn't cross the ignition switch. Rule: never connect USB and vehicle power at the same time.
- Minor audible noise through the speakers when LED brightness changes — typical WS2812B PWM noise, barely noticeable.

**Active project work has moved on to the audio-reactive LED addition below.**

## Audio-Reactive LED Addition (Active — Current Focus)

A new, optional mode layered on top of the completed core project above: LEDs also react to music from the car radio, via an RCA tap → conditioning circuit → spare ADC pin (`GPIO1` on the production ESP32-C3). Independent of the v2.x firmware — doesn't touch or depend on it. Full plan: [docs/audio-reactive-led-plan.md](docs/audio-reactive-led-plan.md).

**Phase 1 (GitHub/bench-test sketch setup): complete.** Plan doc and its four circuit images committed, `firmware/tests/04_audio_reactive_test/` bench-test sketch added (a spare ESP32-C3 module — not the ESP32-C3 installed in the car), `hardware/audio-breakout.md` added, and a new build-log section tracking this separately from the numbered core-project Stages.

Design updated after the initial plan: D1 (rectifier diode) is a 1N4007, not the originally-planned 1N4148, based on what was on hand — fine here since the smoothing capacitor already makes this a hundreds-of-milliseconds envelope, not audio-frequency work. The testing circuit sums stereo L/R via R1/R2 from a 3.5mm breakout cable, mirroring the production circuit's RCA summing (in principle — see the mono note below), and now also has a monitoring speaker tapped in so audio content can be heard while tuning. **Testing board also changed**, from a generic full ESP32 dev board to a spare ESP32-C3 module — since that's the same chip as production, the audio ADC pin (`GPIO1`) is now identical on both test and production, so Phase 2 tuning carries straight into Phase 4 with no pin remapping (LED data uses `GPIO7` on the test board, arbitrary/free).

**Phase 2 (bench-test): Stage A validated and working.** Conditioning circuit breadboarded and confirmed responding correctly to real music. Firmware rewritten from simple whole-strip brightness to a proper bar-graph audio visualizer (growing/shrinking LED bar with a bouncing peak-hold marker — fast attack, slow gravity-fall release) in `firmware/tests/04_audio_reactive_test/`. `audioFloor`/`audioCeiling` calibrated from real Serial readings to `900`/`1300` (idle ~950, loud peaks ~1200). Real-radio validation (originally Stage B) still moved to Phase 3 — the car's RCA wiring isn't practically accessible without opening up the already-installed system.

**Known issue, deferred:** the USB-C-to-3.5mm audio dongle used for bench testing occasionally distorts mid-playback, recovering after a reconnect — likely the monitoring speaker (8Ω) overloading the dongle's small onboard amp, which isn't rated to drive that low an impedance continuously. Not yet fixed since it hasn't blocked validation; a series resistor on the speaker tap or a powered speaker would address it.

**Car radio preout voltage confirmed from the Kenwood DPX-07MD's own service manual** (`B64-2858-00`, `仕様一覧`/specifications page): `プリアウトレベル (FM): 1.8V/10kΩ` — a rated preout of 1.8V RMS, ~2.55V peak. Replaces the earlier unconfirmed "~2–4V RMS" guess.

**R5 removed (design correction, before bench-testing):** the originally-specified bias network (R5 10kΩ to 3.3V, R6 10kΩ to GND) set the envelope's resting voltage at 1.65V — but D1 only conducts once Node A exceeds that by its own forward-voltage drop, and Node A's realistic peak was nowhere near that. R5 removed entirely; R6 alone now returns Node B to GND (~0V resting point), the standard single-resistor diode-envelope-detector topology.

**R3/R4 removed entirely, R1/R2 reduced 2.2kΩ → 470Ω (found via live bench testing, not just calculation):** R3/R4 went from 10kΩ:1kΩ to 1kΩ:1kΩ in an earlier correction, but breadboarding with the 1kΩ:1kΩ divider still in place showed the circuit completely dead — no response to music at all, because the combined attenuation through R2 (2.2kΩ) plus the divider left too little signal for D1 to ever conduct. **Fix: R3/R4 removed entirely**, R1/R2 reduced to 470Ω (still enough for channel isolation/short-protection, without adding unneeded attenuation) — this restored a real response on the bench. Safe for production too: with no divider, ADC protection comes purely from D1's own forward drop against the confirmed 2.55V-peak radio signal, landing around ~1.95–2.25V at Node B, comfortably under the 3.3V ceiling.

**A DC-blocking coupling capacitor was tried and reverted.** Bench testing found a large, non-audio baseline jump on cable connection alone (laptop-specific DC bias suspected; ground loops and a cable fault were ruled out). A coupling cap + bleed resistor would fix that, but proved fiddly to get working reliably on the breadboard, and isn't clearly needed for production (a proper automotive RCA preout is almost always already AC-coupled) — deferred as an open Phase 3 PCB-design decision, not part of the current circuit.

**Testing circuit is currently mono, not stereo:** the breakout cable's "white" wire turned out to have continuity with the shield — it's a second ground/drain wire on this cable, not a real left-channel conductor. Testing runs through the red wire/R2 only for now; R1 stays in the circuit but its input is unconnected.

**Phase 3 changed from the original plan.** Rather than a standalone breakout board, the audio circuit will be integrated directly into a new PCB revision (`v2`) — same EasyEDA → JLCPCB process as the original board. **PCB files are now organised by revision**: `hardware/pcb/v1/{gerbers,bom,easyeda}/` (currently installed) and `hardware/pcb/v2/` (planned, not yet designed) — mirrors how firmware versions are folder-organised. `hardware/audio-breakout.md` documents the now-superseded standalone-breakout circuit design (still accurate on the circuit itself, just not the "separate board" framing) and will need revisiting once Phase 3 starts. **Phase 3 is also where real-radio validation now happens** — on the assembled `v2` board, before permanent install; if `R1`/`R2`/`C1` need correcting from their laptop-derived Stage A values, that's a cheap hand-swap of those parts, not a re-fab. Whether to add the coupling cap for production is also decided there.

**Phase 4 (firmware integration): not started**, depends on Phase 3.

## Key Docs

- [docs/build-log.md](docs/build-log.md) — full project/build history by stage
- [firmware/README.md](firmware/README.md) — firmware version history (v1.0–v3.1)
- [docs/wiring-plan.md](docs/wiring-plan.md) — wiring diagram and pinouts
- [docs/audio-reactive-led-plan.md](docs/audio-reactive-led-plan.md) — audio-reactive LED addition: circuit design and four-phase build plan
- [hardware/audio-breakout.md](hardware/audio-breakout.md) — audio conditioning circuit design detail (superseded "standalone board" framing — see Phase 3 above)
- [hardware/README.md](hardware/README.md) — PCB revision structure (`v1`/`v2`)
