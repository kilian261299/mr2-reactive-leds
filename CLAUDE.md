# MR2 Reactive LEDs — Project Status

Reactive LED lighting controller (ESP32-C3 + MPU6050 + WS2812B) built and installed in a 1992 Toyota MR2 SW20. See [README.md](README.md) for what the system does.

## Project Complete

All physical installation is **complete** — control box, accelerometer, rotary encoder, and both LED strips are mounted in their final positions, running on real vehicle power (fused 12V from the cigarette lighter circuit). Rotary encoder brightness adjustment and LED strip output were confirmed working correctly in the car at this initial installation. Hardware/PCB work is done, including a GPIO4 fault on the first assembled board that was resolved by swapping to a spare PCB (root cause: damaged GPIO4 on that specific ESP32-C3 module, not a design fault).

The originally specified buck converter was swapped for a repurposed USB charger module after it caused intermittent cold-boot failures (likely an unclean voltage rise affecting ESP32-C3 strapping pins on boot). The replacement has since been cold-boot tested repeatedly with no recurrence — **confirmed fixed**.

**`v2.3`** was confirmed on real driving to work "almost perfectly" — `accelerationResponseG` tuned to `0.12` so higher gears reliably reach true orange, and Modes 1–4 fixed (via code review, before that drive) to actually track that same tuning instead of a stale hardcoded threshold. But it also surfaced a **new issue**, not present on earlier versions: flickering between red (braking) and blue (idle) while going downhill, with no braking input needed at all.

That flicker was traced to **v2.1 → v2.2**, not to v2.3's own changes: v2.2 slowed `gravitySmoothing` and made `baselineDynamicReentryThreshold` more sensitive, both specifically to stop sustained acceleration fading too early — but the same `gravitySmoothing` tracker also governs how fast a downhill grade gets absorbed into the baseline, so the fix for one problem quietly created another. It was initially assumed to be an inherent, unfixable accelerometer-only limitation; it wasn't.

**`v2.4` is the final firmware version.** It fixes the flicker by splitting `gravitySmoothing` by direction — the slow rate stays for accelerating (so the acceleration-hold fix is untouched), while the original fast v2.1 rate is restored for braking/downhill (which share a sign on the forward axis). This directly targets the flicker without the blunt trade-offs considered earlier (e.g. dulling `brakingDeadZone` globally). **Built and adopted as final; not yet confirmed on a real drive.**

Two known issues remain, deliberately left unfixed — accepted, permanent trade-offs, not bugs to chase:
- USB power backfeeds enough current to power the car radio when connected with the key off (no reverse-blocking diode on the charger module). Not a safety/drain issue since it doesn't cross the ignition switch. Rule: never connect USB and vehicle power at the same time.
- Minor audible noise through the speakers when LED brightness changes — typical WS2812B PWM noise, barely noticeable.

## Key Docs

- [docs/build-log.md](docs/build-log.md) — full project/build history by stage
- [firmware/README.md](firmware/README.md) — firmware version history (v1.0–v3.1)
- [docs/wiring-plan.md](docs/wiring-plan.md) — wiring diagram and pinouts
