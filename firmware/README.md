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
│   └── 03_full_bench_test/
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
    └── v3.0/
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

# Production Firmware

The production firmware is located in:

[mr2-reactive-leds/](mr2-reactive-leds/)

The firmware was developed through fourteen versions, grouped below by what each one changed. Full detail on every version, including issues found and fixed along the way, is in [mr2-reactive-leds/README.md](mr2-reactive-leds/README.md).

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

Real-world testing found an unresolved question: the pitch estimate showed large swings during acceleration, which may mean the gyro is absorbing genuine acceleration as if it were a hill — or may simply reflect a real road gradient, since the test wasn't confirmed to be on flat ground. Not isolated — development focus moved to v2.1 (below) instead, and stayed there through v2.3 (adopted as the final firmware version). v3.0/v3.1 is parked and not being pursued further; see v3.1 in the full changelog.

---

## v2.1 – Field Tuning (Confirmed Working Baseline)

Branches from v2.0 (not v3.0) — see the full changelog for why. Same accelerometer-only hill compensation as v2.0, unchanged.

`accelerationResponseG` lowered from `0.35` to `0.18`, based on real driving data showing hard acceleration peaking around `0.17g` — the old value meant acceleration rarely reached true orange.

Confirmed on real driving: braking, cornering, and the tuned acceleration response all work well. One further issue found — acceleration can prematurely fade back to blue on a sufficiently long, sustained pull — addressed with a milder compromise in v2.2 below (a full fix was considered and rejected; see the full changelog for why).

This remains a genuinely good, working version even without v2.2's further tuning.

---

## v2.2 – Milder Acceleration-Hold Tuning (Confirmed Working, With Known Limitations)

Branches from the confirmed-clean v2.1 above. Slows down (rather than freezes) the tracker responsible for the premature-fade issue, and splits a previously shared smoothing constant into separate acceleration/braking/cornering/movement values, so tuning one no longer affects the others.

**Confirmed on real driving**: works well overall, mainly noticeable accelerating in 1st and 2nd gear. Two issues found: higher gears rarely reach true orange (addressed in v2.3 below), and very steep downhill sections trigger heavy/frequent braking beyond what pedal input alone would cause (a known, accepted limitation — see v2.3).

## v2.3 – Higher-Gear Acceleration Tuning (Final Version)

Branches from v2.2, based on its first real driving feedback. `accelerationResponseG` lowered further, from `0.18` to `0.12`, to give higher gears (lower forward g for the same "hard" feel) more room to reach orange. This is a physics-based estimate rather than measured data — Serial logging is no longer possible now the board runs permanently on vehicle power (USB and vehicle power can never be connected together).

A code review of this file (before it had been driven) found and fixed several issues in place: Modes 1-4 were using a hardcoded brightness threshold that had silently fallen out of sync with three rounds of acceleration tuning (a genuine bug, now fixed), plus a moving-vehicle recalibration guard and some dead-code/duplication cleanup. See the changelog for the full list.

**Confirmed on real driving — works "almost perfectly."** Higher gears now reach true orange, and Modes 1-4 are confirmed noticeably livelier. Braking and cornering unaffected, as expected. One known limitation remains: steep downhill braking still over-triggers on very steep hills — the same accelerometer-only tilt/dynamic-event ambiguity as the acceleration-fade problem, mirrored onto braking, and not fixable with a threshold tweak.

**v2.3 is adopted as the final firmware version — project complete.** The downhill-braking limitation is accepted rather than pursued further, the same treatment given to the USB power backfeed and PWM speaker noise found earlier in the build. v3.0/v3.1's gyroscope approach — the real fix for that limitation — remains parked and is not being pursued further; see the changelog for the parked v3.1 experiment.

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
