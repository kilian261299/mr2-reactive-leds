# MR2 Reactive LEDs — Project Status

Reactive LED lighting controller (ESP32-C3 + MPU6050 + WS2812B) built and installed in a 1992 Toyota MR2 SW20. See [README.md](README.md) for what the system does.

## Current State

All physical installation is **complete** — control box, accelerometer, rotary encoder, and both LED strips are mounted in their final positions, running on real vehicle power (fused 12V from the cigarette lighter circuit). Rotary encoder brightness adjustment and LED strip output were confirmed working correctly in the car at this initial installation. Hardware/PCB work is done, including a GPIO4 fault on the first assembled board that was resolved by swapping to a spare PCB (root cause: damaged GPIO4 on that specific ESP32-C3 module, not a design fault).

The originally specified buck converter was swapped for a repurposed USB charger module after it caused intermittent cold-boot failures (likely an unclean voltage rise affecting ESP32-C3 strapping pins on boot). The replacement has since been cold-boot tested repeatedly with no recurrence — **confirmed fixed**.

Two known issues were found and deliberately left unfixed (not bugs to chase, just documented trade-offs):
- USB power backfeeds enough current to power the car radio when connected with the key off (no reverse-blocking diode on the charger module). Not a safety/drain issue since it doesn't cross the ignition switch. Rule: never connect USB and vehicle power at the same time.
- Minor audible noise through the speakers when LED brightness changes — typical WS2812B PWM noise, barely noticeable.

Firmware is on the **v2.x line**. `v2.1` is the confirmed-clean, working baseline (accelerometer-only hill compensation, tuned `accelerationResponseG` to `0.18`, LED count corrected to 80/strip). `v2.2` (milder acceleration-hold tuning) has been drive-tested and **works well overall**, mainly noticeable accelerating in 1st/2nd gear — but surfaced two limitations: higher gears rarely reach true orange, and very steep downhill sections over-trigger braking (red) beyond what the pedal input alone would cause. `v2.3` addresses the first issue (`accelerationResponseG` lowered from `0.18` to `0.12`, a physics-based estimate rather than measured data — see below).

Before its first drive, v2.3 also went through a code review that found and fixed a genuine bug unrelated to the acceleration tuning: **Modes 1–4 (the static colour themes) had silently stopped reacting to movement**, since their brightness curve used a hardcoded `0.50g` threshold that never moved across three rounds of `accelerationResponseG` tuning. They now track `accelerationResponseG` directly — a real, noticeable behaviour change (those modes will be far more reactive than before), separate from the acceleration threshold change itself. The review also added a guard against recalibrating while the car is moving, and cleaned up dead code/duplicated intensity-curve math. v2.3 **has not been flashed/drive-tested yet**.

The steep-downhill-braking issue is **not** fixed by v2.3 and isn't expected to be fixable on the accelerometer-only v2.x line at all — it's the same tilt-vs-dynamic-event ambiguity as the acceleration-fade problem v2.2 targets, mirrored onto braking. Documented as an accepted limitation; a real fix would mean revisiting the gyroscope approach below.

`v3.0`/`v3.1` (gyroscope + accelerometer sensor fusion) is a parallel, parked branch — real-world testing found an unresolved question of whether the gyro absorbs genuine acceleration as if it were a hill. Not abandoned, just not the active line while v2.x is being tuned.

**Note:** the board now runs permanently on vehicle power, and USB (needed for Serial logging) can never be connected at the same time as vehicle power. Further tuning is based on visual driving feedback, not fresh logged data.

## Open Work

- **Flash and drive-test v2.3** — the only real outstanding task. Watch for: whether higher gears now reach orange, whether the lower threshold overreacts on normal light-throttle driving in lower gears, and how Modes 1-4 feel now that they actually respond to movement (code-review fix, untested).
- Decide whether the v3.0/v3.1 pitch-drift question is worth isolated testing (hard acceleration on confirmed-flat ground), or leave it parked — the steep-downhill-braking limitation found on v2.2 is an added reason this might eventually be worth revisiting.

## Key Docs

- [docs/build-log.md](docs/build-log.md) — full project/build history by stage
- [firmware/README.md](firmware/README.md) — firmware version history (v1.0–v3.1)
- [docs/wiring-plan.md](docs/wiring-plan.md) — wiring diagram and pinouts
