# Firmware

This folder contains the firmware for the ESP32-C3 based MR2 Reactive LED controller.

The firmware consists of standalone hardware test sketches and the main production firmware.

---

# Folder Structure

```text
firmware/
├── tests/
│   ├── 01_encoder_led_test/
│   ├── 02_accelerometer_test/
│   ├── 03_full_bench_test/
│   └── 04_audio_reactive_test/
│
└── mr2-reactive-leds/
    ├── README.md
    ├── v1.0/
    ├── v1.1/
    ├── v1.2/
    ├── v1.3/
    ├── v1.4/
    ├── v1.5/
    ├── v1.6/
    ├── v1.7/
    ├── v2.0/
    ├── v2.1/
    ├── v2.2/
    ├── v2.3/
    ├── v2.4.0/
    ├── v2.4.1/
    ├── v3.0/
    └── v4.0/
```

---

# Hardware Test Sketches

The [`tests/`](tests/) folder contains individual Arduino sketches used to validate hardware components before integrating the complete system.

---

## 01_encoder_led_test

Tests:

- Rotary encoder input
- Push button operation
- WS2812B LED control
- SN74AHCT125N logic level shifter

Purpose:

Verify user input and LED output independently from vehicle sensing.

---

## 02_accelerometer_test

Tests:

- MPU6050 communication
- Accelerometer readings
- Sensor orientation
- Sensor stability and noise

Purpose:

Verify acceleration sensing and characterise sensor behaviour before implementing reactive processing.

---

## 03_full_bench_test

Tests the complete hardware system:

- ESP32-C3 controller
- MPU6050 accelerometer
- Rotary encoder
- WS2812B LEDs

Purpose:

Validate full hardware integration before vehicle installation.

---

## 04_audio_reactive_test

**New, experimental — not part of the core reactive-LED system above.** Bench-test sketch for the [audio-reactive LED feature](../docs/audio-reactive-led-plan.md), an optional addition layered on top of the completed core project. Runs on a spare ESP32-C3 module — not the ESP32-C3 installed in the car — using the same `GPIO1` audio ADC pin the production plan uses, since it's the same chip.

Tests:

- Reading a conditioned audio envelope on an ADC pin
- Smoothing that envelope in firmware, with separate fast-attack/slow-release rates for a natural bounce
- Driving a bar-graph audio visualizer on a real addressable strip — a growing/shrinking bar of lit LEDs with a bouncing peak-hold marker, snapping from cyan to orange once the level crosses a loudness threshold, not just uniform brightness

Purpose:

Prove the audio-conditioning-circuit-to-LED pipeline works before any of it touches the real car firmware. **Validated and working** — confirmed responding correctly to real music, with `audioFloor`/`audioCeiling` calibrated from real Serial readings. See [docs/audio-reactive-led-plan.md](../docs/audio-reactive-led-plan.md) for the full build plan and [hardware/audio-breakout.md](../hardware/audio-breakout.md) for the conditioning circuit this sketch reads from.

This sketch's logic has since been ported into the real production firmware as `v4.0`'s Mode 1 — see [mr2-reactive-leds/README.md](mr2-reactive-leds/README.md#v40--audio-reactive-mode-drafted-not-yet-bench-tested).

---

# Production Firmware

The production firmware is located in:

[mr2-reactive-leds/](mr2-reactive-leds/)

The firmware was developed through sixteen versions, grouped below by what each one changed. Full detail on every version, including issues found and fixed along the way, is in [mr2-reactive-leds/README.md](mr2-reactive-leds/README.md).

---

## v1.0 – Initial Reactive Lighting

First complete implementation of the reactive lighting system.

Implemented:

- Acceleration lighting
- Braking detection
- Cornering effects
- User controls
- Sensor calibration

---

## v1.1 – v1.7 — Incremental Refinement

A series of smaller, focused improvements built on top of v1.0, each addressing one specific area:

- **v1.1** — Dynamic acceleration baseline, reducing false triggers caused by hills and vehicle angle.
- **v1.2** — Removed diagnostic/test modes ahead of final vehicle use.
- **v1.3** — Startup sweep animation, idle breathing effect, four static colour theme modes.
- **v1.4** — Colour transition refinement; theme modes made reactive to movement and cornering.
- **v1.5** — Rotary encoder reliability fix (full quadrature decoding).
- **v1.6** — Rotary encoder moved to interrupt-driven handling for better responsiveness during rapid rotation.
- **v1.7** — LED brightness reworked to manual RGB channel scaling for more consistent colour at all brightness levels.

---

## v2.0 – Smart Dynamic Baseline

A more robust accelerometer-only hill-compensation system, using a STABLE/DYNAMIC/SETTLING state machine to distinguish long-term vehicle orientation changes (hills) from genuine dynamic movement (acceleration, braking, cornering), and freeze the baseline during the latter.

---

## v3.0 – Gyroscope + Accelerometer Sensor Fusion

Replaces the accelerometer-only hill compensation with gyroscope + accelerometer sensor fusion: the gyro tracks the vehicle's actual pitch angle (a hill causes rotation; genuine acceleration doesn't), letting gravity's contribution to the forward-axis reading be calculated and removed directly, rather than inferred from a slowly adapting baseline.

Includes several fixes found during bench testing (gyro bias correction, accelerometer reliability gating, sign tuning, side-axis gating rework) — see the full changelog for details.

Real-world testing found an unresolved question: the pitch estimate showed large swings during acceleration, which may mean the gyro is absorbing genuine acceleration as if it were a hill — or may simply reflect a real road gradient, since the test wasn't confirmed to be on flat ground. Not isolated — development focus moved to v2.1 (below) instead, and stayed there through v2.4.1 (adopted as the final firmware version). v3.0/v3.1 is parked and not being pursued further; see v3.1 in the full changelog.

---

## v2.1 – Field Tuning (Confirmed Working Baseline)

Branches from v2.0 (not v3.0) — see the full changelog for why. Same accelerometer-only hill compensation as v2.0, unchanged.

`accelerationResponseG` lowered from `0.35` to `0.18`, based on real driving data showing hard acceleration peaking around `0.17g` — the old value meant acceleration rarely reached true orange.

Confirmed on real driving: braking, cornering, and the tuned acceleration response all work well. One further issue found — acceleration can prematurely fade back to blue on a sufficiently long, sustained pull — addressed with a milder compromise in v2.2 below (a full fix was considered and rejected; see the full changelog for why).

This remains a genuinely good, working version even without v2.2's further tuning.

---

## v2.2 – Milder Acceleration-Hold Tuning (Confirmed Working, With Known Limitations)

Branches from the confirmed-clean v2.1 above. Slows down (rather than freezes) the tracker responsible for the premature-fade issue, and splits a previously shared smoothing constant into separate acceleration/braking/cornering/movement values, so tuning one no longer affects the others.

**Confirmed on real driving**: works well overall, mainly noticeable accelerating in 1st and 2nd gear. Two issues found: higher gears rarely reach true orange (addressed in v2.3 below), and very steep downhill sections trigger heavy/frequent braking beyond what pedal input alone would cause (initially believed to be an inherent accelerometer-only limitation — turned out to be partly a side effect of this version's own tuning; see v2.4.1).

## v2.3 – Higher-Gear Acceleration Tuning

Branches from v2.2, based on its first real driving feedback. `accelerationResponseG` lowered further, from `0.18` to `0.12`, to give higher gears (lower forward g for the same "hard" feel) more room to reach orange. This is a physics-based estimate rather than measured data — Serial logging is no longer possible now the board runs permanently on vehicle power (USB and vehicle power can never be connected together).

A code review of this file (before it had been driven) found and fixed several issues in place: Modes 1-4 were using a hardcoded brightness threshold that had silently fallen out of sync with three rounds of acceleration tuning (a genuine bug, now fixed), plus a moving-vehicle recalibration guard and some dead-code/duplication cleanup. See the changelog for the full list.

**Confirmed on real driving — works "almost perfectly."** Higher gears now reach true orange, and Modes 1-4 are confirmed noticeably livelier. Braking and cornering unaffected, as expected. **One new issue found**: flickering between red and blue while going downhill, worse than the "heavy/frequent braking" originally reported on v2.2. Traced to v2.2's `gravitySmoothing`/`baselineDynamicReentryThreshold` tuning, not to this version's own changes — fixed in v2.4.1 below (v2.4.0 was a first attempt that didn't work).

## v2.4.0 – Downhill Flicker Fix, Attempt 1 (Superseded)

Branches from v2.3. Splits `gravitySmoothing` by direction instead of reverting it: the slow v2.2 rate (`0.003`) is kept for the *accelerating* direction (so the acceleration-hold fix is untouched), while a restored fast rate (`gravitySmoothingBraking = 0.008`, the original v2.1 value) is used for the *braking/downhill* direction — the direction a hill grade and real braking both share. This lets hills settle quickly again without dulling any braking response and without giving back v2.2's acceleration improvement.

**Drive-tested: no improvement.** Downhill flicker was unchanged from v2.3. Uphill and flat also felt unchanged — but they were already fine, so that's not evidence this fix worked, just that it's consistent with it having no effect. Root cause: this version only made the forward axis (`gravityX`) direction-aware — a hill pitch also shifts the vertical axis (`gravityZ`) at the same time, which was left on the old slow rate throughout. Since the state machine's gating signal combines all three axes into one magnitude, `gravityZ`'s slow recovery kept that combined signal elevated regardless of how fast `gravityX` alone recovered, so the baseline never settled any sooner than it did on v2.3. Fixed in v2.4.1.

## v2.4.1 – Downhill Flicker Fix, Attempt 2 (Final Version, Confirmed)

Branches from v2.4.0. Extends the same direction-aware rate to `gravityZ`, using the same forward-axis direction flag as `gravityX` (a hill/braking event is a forward-axis phenomenon; `gravityZ`'s shift is a side effect of it, not independent). `gravityY` (lateral/cornering) is left untouched.

**Confirmed on real driving — the downhill flicker is gone.** Acceleration-hold and braking both feel unchanged from v2.2/v2.3, as expected. **v2.4.1 is confirmed as the final firmware version — the core reactive-LED project is complete.** See the changelog for the full root-cause trace and implementation detail.

---

## v4.0 – Audio-Reactive Mode (Drafted, Not Yet Bench-Tested)

New version line branching from v2.4.1 (v3.0/v3.1's gyroscope work stays parked, unrelated) — adds the [audio-reactive LED feature](../docs/audio-reactive-led-plan.md). Mode 1 (previously a fixed purple theme) is replaced entirely by a bar-graph visualizer driven by the conditioning circuit's envelope on `GPIO1`, ported from the validated bench-test sketch: bar length tracks volume, colour snaps cyan-to-orange with hysteresis once loud enough, LED brightness stays capped by the same rotary-encoder ceiling every other mode uses. Modes 0, 2, 3, 4 are untouched. Also applies the bench-test sketch's `Serial.availableForWrite()` fix to the rest of this file's pre-existing Serial calls, which never had it.

Compiles clean against the real `ESP32-C3 Super Mini` target. `audioFloor`/`audioCeiling` are still Phase 2 bench placeholders (`900`/`1300`) needing retuning against the real radio. Not yet bench-tested — the conditioning circuit parts are with the `v2` PCB rather than available for a bench rebuild; first real test will be a semi-install against the car radio in Phase 4.

For detailed version history and development notes, see:

[mr2-reactive-leds/README.md](mr2-reactive-leds/README.md)

---

# Hardware Platform

## Controller

- ESP32-C3

## Sensors

- MPU6050 accelerometer/gyroscope

## Lighting

- WS2812B addressable LEDs
- SN74AHCT125N 3.3V to 5V logic level shifter

## User Interface

- Rotary encoder with push button
