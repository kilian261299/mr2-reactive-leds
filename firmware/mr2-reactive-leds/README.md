# Firmware

This document describes the development of the MR2 Reactive LEDs firmware.

---

# Version History

## v1.0 – Initial Reactive Lighting

The first complete version of the firmware implementing the reactive lighting system.

### Features

- Reactive acceleration lighting
- Braking detection
- Left and right cornering effects
- Four diagnostic modes
- Rotary encoder brightness adjustment
- MPU6050 calibration
- Adjustable sensitivity

### Notes

This version used the raw accelerometer readings directly. While effective during normal driving, sustained gradients (such as hills) could cause the LEDs to remain in an acceleration or braking state.

---

## v2.0 – Dynamic Baseline Filtering

Improved acceleration detection by reacting to changes in acceleration rather than absolute sensor readings.

### Improvements

Instead of using the raw longitudinal acceleration value directly, the firmware continuously maintains a slowly updating baseline representing the vehicle's current orientation.

Acceleration effects are then calculated from the difference between the current reading and this moving baseline.

This allows the baseline to gradually follow long-term changes, such as driving uphill or downhill, while still responding quickly to genuine acceleration and braking events.

### Benefits

- Eliminates false acceleration detection on hills.
- Eliminates false braking detection on downhill sections.
- Produces smoother and more realistic vehicle response.
- Greatly improves real-world driving behaviour.

---

## v3.0 – Production Firmware

The production firmware simplifies the user interface and improves the overall visual experience.

### Changes

- Removed all diagnostic operating modes.
- Added animated startup sequence.
- Added idle breathing effect while stationary.
- Added selectable solid colour ambient modes.
- Improved colour transitions.
- Improved brightness scaling.
- Improved responsiveness and smoothing.
- General code clean-up and optimisation.

### Current Features

- Reactive acceleration lighting
- Reactive braking lighting
- Independent left/right cornering effects
- Dynamic baseline filtering
- Startup animation
- Idle breathing animation
- Solid colour modes
- Rotary encoder brightness adjustment
- Rotary encoder mode selection
- Automatic sensor calibration
