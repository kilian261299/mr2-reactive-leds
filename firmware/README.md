# Firmware

This folder contains Arduino sketches for the ESP32-C3 reactive LED controller.

## Test Sketches

Test sketches will be stored in `firmware/tests/`.

Planned tests:

- `01_encoder_led_test` — rotary encoder controlling WS2812B LEDs through the SN74AHCT125N level shifter
- `02_accelerometer_test` — MPU6050 accelerometer readings
- `03_full_bench_test` — combined encoder, accelerometer, and LED behaviour

## Final Sketch

The final car-ready firmware will be stored in:

```text
firmware/mr2-reactive-leds/mr2-reactive-leds.ino
```
