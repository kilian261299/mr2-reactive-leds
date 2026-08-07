# MR2 Reactive LEDs — Project Status

Reactive LED lighting controller (ESP32-C3 + MPU6050 + WS2812B) built and installed in a 1992 Toyota MR2 SW20. See [README.md](README.md) for what the system does.

## Current State

All physical installation is **complete** — control box, accelerometer, rotary encoder, and both LED strips are mounted in their final positions, running on real vehicle power (fused 12V from the cigarette lighter circuit). Rotary encoder brightness adjustment and LED strip output were confirmed working correctly in the car at this initial installation. Hardware/PCB work is done, including a GPIO4 fault on the first assembled board that was resolved by swapping to a spare PCB (root cause: damaged GPIO4 on that specific ESP32-C3 module, not a design fault).

The originally specified buck converter was swapped for a repurposed USB charger module after it caused intermittent cold-boot failures (likely an unclean voltage rise affecting ESP32-C3 strapping pins on boot). The replacement has since been cold-boot tested repeatedly with no recurrence — **confirmed fixed**.

Two known issues were found and deliberately left unfixed (not bugs to chase, just documented trade-offs):
- USB power backfeeds enough current to power the car radio when connected with the key off (no reverse-blocking diode on the charger module). Not a safety/drain issue since it doesn't cross the ignition switch. Rule: never connect USB and vehicle power at the same time.
- Minor audible noise through the speakers when LED brightness changes — typical WS2812B PWM noise, barely noticeable.

Firmware is on the **v2.x line**. `v2.1` is the confirmed-clean, working baseline (accelerometer-only hill compensation, tuned `accelerationResponseG` to `0.18`, LED count corrected to 80/strip). `v2.2` is built on top of v2.1 with milder acceleration-hold tuning (targets a premature-fade issue during sustained acceleration) but **has not been flashed/drive-tested yet**.

`v3.0`/`v3.1` (gyroscope + accelerometer sensor fusion) is a parallel, parked branch — real-world testing found an unresolved question of whether the gyro absorbs genuine acceleration as if it were a hill. Not abandoned, just not the active line while v2.x is being tuned.

## Open Work

- **Flash and drive-test v2.2** — the only real outstanding task. Watch for: sustained acceleration holding noticeably longer than v2.1, and real hills still settling to blue in a reasonable time (expected trade-off).
- Decide whether the v3.0/v3.1 pitch-drift question is worth isolated testing (hard acceleration on confirmed-flat ground), or leave it parked.

## Key Docs

- [docs/build-log.md](docs/build-log.md) — full project/build history by stage
- [firmware/README.md](firmware/README.md) — firmware version history (v1.0–v3.1)
- [docs/wiring-plan.md](docs/wiring-plan.md) — wiring diagram and pinouts
