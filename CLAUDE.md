# MR2 Reactive LEDs — Project Status

Reactive LED lighting controller (ESP32-C3 + MPU6050 + WS2812B) built and installed in a 1992 Toyota MR2 SW20. See [README.md](README.md) for what the system does.

## Project Complete

All physical installation is **complete** — control box, accelerometer, rotary encoder, and both LED strips are mounted in their final positions, running on real vehicle power (fused 12V from the cigarette lighter circuit). Rotary encoder brightness adjustment and LED strip output were confirmed working correctly in the car at this initial installation. Hardware/PCB work is done, including a GPIO4 fault on the first assembled board that was resolved by swapping to a spare PCB (root cause: damaged GPIO4 on that specific ESP32-C3 module, not a design fault).

The originally specified buck converter was swapped for a repurposed USB charger module after it caused intermittent cold-boot failures (likely an unclean voltage rise affecting ESP32-C3 strapping pins on boot). The replacement has since been cold-boot tested repeatedly with no recurrence — **confirmed fixed**.

**`v2.3` is the final firmware version**, confirmed on real driving to work "almost perfectly": accelerometer-only hill compensation (`v2.1`/`v2.2` heritage), `accelerationResponseG` tuned to `0.12` so higher gears reliably reach true orange, and Modes 1–4 fixed (via code review, before this drive) to actually track that same tuning instead of a stale hardcoded threshold — those modes are now noticeably livelier, a genuine bug fix. Braking and cornering both confirmed working well.

Three known issues were found and deliberately left unfixed — accepted, permanent trade-offs, not bugs to chase:
- **Steep downhill braking over-triggers** on very steep hills (confirmed on Brighton hills) — the accelerometer can't distinguish a steep sustained grade from genuine sustained braking, since both freeze the same baseline. Structural limitation of the accelerometer-only approach; a real fix would need the gyroscope-based sensor fusion explored in the parked `v3.0`/`v3.1` branch (not pursued further — that branch had its own unresolved pitch-drift question, and wasn't needed once `v2.3` proved good enough).
- USB power backfeeds enough current to power the car radio when connected with the key off (no reverse-blocking diode on the charger module). Not a safety/drain issue since it doesn't cross the ignition switch. Rule: never connect USB and vehicle power at the same time.
- Minor audible noise through the speakers when LED brightness changes — typical WS2812B PWM noise, barely noticeable.

## Key Docs

- [docs/build-log.md](docs/build-log.md) — full project/build history by stage
- [firmware/README.md](firmware/README.md) — firmware version history (v1.0–v3.1)
- [docs/wiring-plan.md](docs/wiring-plan.md) — wiring diagram and pinouts
