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

The firmware was developed through eleven versions, grouped below by what each one changed. Full detail on every version, including issues found and fixed along the way, is in [mr2-reactive-leds/README.md](mr2-reactive-leds/README.md).

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

Real-world testing found an unresolved question: the pitch estimate showed large swings during acceleration, which may mean the gyro is absorbing genuine acceleration as if it were a hill — or may simply reflect a real road gradient, since the test wasn't confirmed to be on flat ground. Not yet isolated. Development focus has moved to v2.1 (below) while this is pending, but v3.0 is not considered abandoned.

---

## v2.1 – Field Tuning (Confirmed Working Baseline)

Branches from v2.0 (not v3.0) — see the full changelog for why. Same accelerometer-only hill compensation as v2.0, unchanged.

`accelerationResponseG` lowered from `0.35` to `0.18`, based on real driving data showing hard acceleration peaking around `0.17g` — the old value meant acceleration rarely reached true orange.

Confirmed on real driving: braking, cornering, and the tuned acceleration response all work well. One further issue found — acceleration can prematurely fade back to blue on a sufficiently long, sustained pull — addressed with a milder compromise in v2.2 below (a full fix was considered and rejected; see the full changelog for why).

This remains a genuinely good, working version even without v2.2's further tuning.

---

## v2.2 – Milder Acceleration-Hold Tuning (Newest, Not Yet Tested)

Branches from the confirmed-clean v2.1 above. Slows down (rather than freezes) the tracker responsible for the premature-fade issue, and splits a previously shared smoothing constant into separate acceleration/braking/cornering/movement values, so tuning one no longer affects the others.

Expected to hold sustained acceleration noticeably longer than v2.1; expected trade-off is that real hills may take somewhat longer to settle to blue. Built but not yet tested in the car — full detail, including the rejected full-fix reasoning, in the changelog.

The decision to branch from v2.0 rather than continue v3.0 is provisional — v3.0's gyroscope approach may be revisited once its own pitch behaviour can be tested unambiguously; see the changelog for the parked v3.1 experiment.

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
