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

## v3.0 – Mode Cleanup

The firmware was simplified following validation of the accelerometer axis orientation and reactive lighting behaviour.

### Changes

- Removed X/Y/Z diagnostic modes.
- Removed one-off Left/Right test modes.
- Removed the White/current test mode.
- Removed the software Off mode.
- Power control is now handled by the physical master toggle switch.

The removal of diagnostic and test modes simplified the user interface and prepared the firmware for use in the final vehicle installation.

---

## v3.1 – New Modes and Effects

The firmware was updated with improved startup behaviour, idle lighting effects, and additional ambient lighting modes.

### Changes

- Added a startup sweep animation.
- Added idle breathing effect to the main reactive mode.
- Replaced the removed software Off mode with four static theme colours.
- Increased the total number of user-selectable modes to five.

### Startup Sweep

A one-off fading chase animation runs across both LED strips when the system powers on.

The startup animation completes before normal MPU6050 accelerometer-based reactive lighting begins.

### Idle Breathing

When the vehicle is stationary or experiencing minimal movement, the reactive lighting mode now produces a gentle breathing effect with an approximately four-second cycle.

The breathing effect is automatically overridden when genuine acceleration, braking, or cornering movement is detected.

### Theme Modes

Four selectable static colour themes were added:

- Purple
- Red
- Blue
- Green

These provide ambient lighting options when reactive vehicle-dynamics lighting is not required.

The firmware now provides five selectable modes in total.

---

## v3.2 – Colour and Reactivity Refinement

The firmware was refined to improve colour transitions and extend reactive behaviour across all lighting modes.

### Changes

- Tuned the acceleration fade target colour to reduce the green channel from 80 to 40.
- Reduced the yellow tint previously visible during the blue-to-orange acceleration transition.
- Extended movement-reactive brightness behaviour to all four theme modes.
- Extended cornering brightness shifts to all four theme modes.
- Extended idle breathing effects to all four theme modes.
- Hand-tuned final purple and green RGB colour values.

### Improvements

The four theme modes are no longer completely static.

They now respond to overall vehicle movement and left/right cornering in the same way as the main reactive mode, while retaining their selected base colour.

This provides consistent reactive behaviour across all five available lighting modes.

---

## v3.3 – Rotary Encoder Reliability Fix

The rotary encoder brightness control was updated to improve reliability and eliminate erratic brightness adjustments.

### Changes

The previous single-edge CLK detection method was replaced with full quadrature decoding.

The new implementation tracks both the CLK and DT signals simultaneously using a state-transition table.

### Improvements

The previous implementation could occasionally detect encoder transitions incorrectly.

This was particularly noticeable when the firmware was busy updating the LED strips, as the large LED update operation could cause the encoder to be sampled during a transition or contact bounce.

This could result in:

- Erratic brightness changes.
- Incorrect direction detection.
- Difficulty reliably reaching minimum brightness.
- Inconsistent encoder behaviour during rapid rotation.

The new full quadrature decoding method tracks valid CLK/DT state transitions and provides more reliable direction detection.

### Result

- Improved rotary encoder reliability.
- Reduced sensitivity to contact bounce.
- More consistent clockwise and counter-clockwise detection.
- Reliable brightness adjustment across the full brightness range.
- Improved ability to reach minimum brightness.

---

## v3.4 – Improved Rotary Encoder Responsiveness

Following testing of v3.3, the rotary encoder brightness control was further refined.

Although the full quadrature decoding introduced in v3.3 improved direction reliability, brightness adjustment could still become less responsive during rapid encoder rotation.

This was caused by the main firmware loop performing large LED update operations. The two 160-LED strips are updated repeatedly, and these operations can temporarily prevent the encoder from being sampled frequently enough during fast rotation.

### Changes

The rotary encoder handling was redesigned to improve responsiveness during rapid rotation.

The encoder is now handled using a more responsive quadrature decoding approach that is better suited to the high-frequency LED update loop.

The brightness control was also tuned to provide a more direct response to physical encoder movement.

### Improvements

- Improved brightness adjustment responsiveness.
- Improved encoder performance during rapid rotation.
- Reduced missed encoder transitions when turning the knob quickly.
- Maintained reliable clockwise and counter-clockwise direction detection.
- Maintained stable minimum brightness control.
- Improved overall feel of the brightness control during normal use.

### Result

The rotary encoder now provides a more responsive and usable brightness adjustment experience while maintaining the reliability improvements introduced in v3.3.

The encoder can be rotated more quickly without losing as many brightness adjustments, making it easier to move rapidly through the full brightness range.

Further refinement may be carried out following vehicle testing if additional improvements to encoder responsiveness or brightness adjustment are required.

---

# Current Firmware – v3.4

The current firmware version is **v3.4**.

The production firmware currently provides:

- Reactive acceleration lighting.
- Reactive braking lighting.
- Independent left/right cornering effects.
- Dynamic baseline filtering for hill compensation.
- Startup sweep animation.
- Idle breathing animation.
- Four selectable solid colour theme modes.
- Reactive movement behaviour across all theme modes.
- Reactive left/right cornering brightness shifts across all theme modes.
- Rotary encoder brightness adjustment.
- Responsive rotary encoder brightness control.
- Rotary encoder mode selection.
- Full quadrature rotary encoder decoding.
- Automatic MPU6050 sensor calibration.
- Improved colour transitions.
- Smoothed brightness and movement response.

The firmware is now ready for vehicle testing on the completed PCB and control box assembly.

Further firmware changes may be made following real-world vehicle testing if sensitivity, filtering, brightness response, encoder responsiveness, or reactive behaviour require additional refinement.
