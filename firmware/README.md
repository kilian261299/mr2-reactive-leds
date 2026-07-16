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
    ├── v2.0/
    └── v3.0/
```

---

# Hardware Test Sketches

The `tests/` folder contains individual Arduino sketches used to validate hardware components before integrating the complete system.

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

```text
mr2-reactive-leds/
```

The firmware was developed through three major versions.

---

## v1.0 - Initial Reactive Lighting

First complete implementation of the reactive lighting system.

Implemented:

- Acceleration lighting
- Braking detection
- Cornering effects
- User controls
- Sensor calibration

---

## v2.0 - Dynamic Baseline Filtering

Improved acceleration detection by reacting to changes in acceleration rather than absolute readings.

Added:

- Dynamic acceleration baseline
- Improved filtering
- Reduced false triggers caused by hills and vehicle angle

---

## v3.0 - Production Firmware

Final production version.

Added:

- Refined reactive lighting behaviour
- Startup animation
- Idle breathing effect
- Ambient colour modes
- Improved smoothing
- Improved brightness scaling
- Improved user interaction

For detailed version history and development notes, see:

```text
mr2-reactive-leds/README.md
```

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
