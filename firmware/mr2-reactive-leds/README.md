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

---

## v3.5 – LED Output and Colour Refinement

The firmware was refined following continued development of the LED output and lighting behaviour.

### Changes

- Reworked LED brightness handling to use manual RGB channel scaling.
- Retained the NeoPixel library at maximum brightness while applying brightness directly to each colour channel before output.
- Improved consistency of colour ratios across different brightness levels.
- Refined the blue-to-orange acceleration colour transition.
- Reduced unwanted green contribution during acceleration colour transitions.
- Tuned the purple theme colour for a deeper, more controlled appearance.
- Retained full breathing effect depth across reactive and theme modes.
- Continued use of smoothed accelerometer values for stable reactive behaviour.
- Retained progressive acceleration, braking, and cornering responses.

### Manual RGB Brightness Scaling

Brightness is now controlled by directly scaling the red, green, and blue channels before the colour is sent to the LED strips.

This avoids relying on `setBrightness()` for normal runtime brightness control.

The approach provides more predictable colour behaviour during:

- Idle breathing.
- Acceleration response.
- Braking response.
- Cornering brightness changes.
- Static theme modes.

### Colour Refinement

The main reactive mode continues to transition from deep blue through a controlled violet-blue stage before reaching warm orange under stronger acceleration.

The green channel is deliberately kept low during the transition to prevent the LEDs from producing an unwanted pale purple, white, or yellow appearance.

The purple theme was also hand-tuned to use a controlled RGB value rather than a full-intensity mixture.

### Result

v3.5 provided a more consistent and visually controlled LED output while maintaining the established reactive lighting behaviour.

The firmware was now ready for further refinement of the accelerometer baseline system and real-world hill compensation behaviour.

---

## v3.6 – Smart Dynamic Baseline and Final Robust Firmware

v3.6 is the final planned firmware version for the MR2 Reactive LEDs project.

This version builds directly on the established v3.5 LED output, colour handling, reactive behaviour, and rotary encoder system.

The primary improvement in v3.6 is a more robust smart hill-compensation system designed to distinguish between long-term vehicle orientation changes and genuine dynamic vehicle movement.

### Smart Hill Compensation

The MPU6050 measures both gravity and dynamic acceleration.

Because the accelerometer is mounted to the vehicle, changes in vehicle pitch can alter the measured forward-axis acceleration even when the vehicle is travelling at a constant speed.

For example:

- Driving uphill can produce an apparent forward acceleration change.
- Driving downhill can produce an apparent braking change.

The hill-compensation system therefore maintains a slowly adapting accelerometer baseline representing the vehicle's current orientation.

Reactive acceleration and braking behaviour is calculated relative to this baseline.

### Smart Baseline Gating

Unlike the previous continuously adapting baseline system, v3.6 prevents the baseline from adapting during meaningful dynamic vehicle movement.

The baseline is frozen when the system detects:

- Significant forward acceleration.
- Significant braking.
- Significant lateral cornering movement.

This prevents genuine acceleration or braking events from being gradually absorbed into the baseline.

### Baseline State Machine

The smart baseline system operates using three states:

#### STABLE

The vehicle is considered sufficiently calm.

The baseline is allowed to adapt slowly to long-term changes in vehicle orientation.

This allows the system to gradually compensate for sustained gradients such as:

- Uphill driving.
- Downhill driving.
- Long-term changes in vehicle pitch.

#### DYNAMIC

Meaningful vehicle movement has been detected.

The baseline is completely frozen.

This includes:

- Acceleration.
- Braking.
- Cornering.

The baseline remains frozen for as long as meaningful dynamic movement continues.

This prevents genuine vehicle dynamics from being mistaken for a change in vehicle orientation.

#### SETTLING

Dynamic movement has stopped.

The baseline remains frozen temporarily while the vehicle settles.

A settling delay prevents the system from immediately adapting to the end of an acceleration, braking, or cornering event.

If dynamic movement resumes during this period, the system immediately returns to the DYNAMIC state.

Once the settling period has completed and the vehicle remains sufficiently stable, the system returns to STABLE and slow baseline adaptation resumes.

### Dynamic Movement Detection

v3.6 uses a separate dynamic acceleration estimate to determine whether the vehicle is genuinely moving dynamically.

The dynamic component is derived by comparing the raw accelerometer measurement against a slowly filtered gravity estimate.

This separates:

- Fast changes caused by acceleration, braking and cornering.
- Slow changes caused by vehicle pitch and long-term orientation.

The dynamic movement estimate is used to control the baseline state machine, while the baseline-corrected forward and lateral values are used for the reactive LED effects.

This allows the system to distinguish between a sustained hill and genuine vehicle acceleration more reliably than the previous continuously adapting baseline approach.

### Baseline Protection

The baseline is protected from adapting during genuine dynamic movement.

The intended behaviour is:

    Vehicle stable
          |
          v
    STABLE
    Baseline slowly adapts
    to long-term orientation
          |
          v
    Genuine dynamic movement detected
          |
          v
    DYNAMIC
    Baseline FREEZES
          |
          v
    Acceleration / braking / cornering continues
          |
          v
    Baseline remains FROZEN
          |
          v
    Dynamic movement stops
          |
          v
    SETTLING
    Baseline remains FROZEN
          |
          v
    Vehicle remains calm
    for settling period
          |
          v
    STABLE
    Baseline slowly adapts again

### Reactive Behaviour During State Changes

The baseline state machine is used internally to control hill compensation and does not directly determine whether the LEDs are visually reactive.

The LEDs continue to respond to the baseline-corrected acceleration, braking, and cornering values.

This means that:

- Entering `DYNAMIC` freezes the baseline so genuine acceleration or braking is preserved.
- Remaining in `DYNAMIC` keeps the baseline protected while the event continues.
- Entering `SETTLING` prevents the baseline from immediately absorbing the end of the event.
- Returning to `STABLE` allows slow baseline adaptation to resume.
- The LED effects can therefore return gradually toward the normal blue idle state rather than relying solely on the baseline state itself.

The purpose of the state machine is to control how the accelerometer baseline behaves, not to act as a direct LED on/off trigger.

### v3.6 Improvements Over v3.5

Compared with v3.5, the main improvements are:

- More reliable distinction between genuine acceleration and vehicle pitch changes.
- More reliable distinction between genuine braking and downhill gradients.
- Baseline protection during genuine dynamic movement.
- Temporary settling period after dynamic movement ends.
- Reduced risk of genuine acceleration being absorbed into the baseline.
- Improved long-term hill compensation.
- More controlled return to stable baseline tracking after acceleration, braking, or cornering.

### Result

v3.6 provides a more robust foundation for real-world vehicle use.

The firmware now separates the concepts of:

- Long-term vehicle orientation.
- Short-term dynamic acceleration.
- Baseline adaptation.
- Dynamic movement detection.
- Baseline settling.

This allows the system to compensate for hills while preserving genuine acceleration, braking, and cornering events for the reactive LED effects.

The result is intended to provide more natural and reliable reactive lighting during normal road driving, including sustained uphill and downhill sections.

---

# Firmware Architecture Summary

The firmware is now structured around four primary systems:

1. **MPU6050 Accelerometer**
   - Measures vehicle motion and orientation.
   - Provides forward and lateral acceleration information.
   - Supplies the data used for dynamic movement detection and reactive lighting.

2. **Smart Dynamic Baseline**
   - Tracks long-term changes in vehicle orientation.
   - Compensates for sustained gradients.
   - Freezes during genuine dynamic movement.
   - Uses STABLE, DYNAMIC, and SETTLING states.

3. **Reactive LED Engine**
   - Converts baseline-corrected acceleration, braking, and cornering values into lighting effects.
   - Controls colour transitions and brightness responses.
   - Provides idle breathing when movement is minimal.

4. **User Interface and Control**
   - Rotary encoder controls brightness.
   - Rotary encoder selects between the five lighting modes.
   - Physical master toggle switch controls system power.
   - LED strips provide the final visual output.

---

# Current Firmware Status

**Current Version:** v3.6

**Status:** Final planned firmware version

### Current Features

- Smart hill compensation
- Dynamic baseline filtering
- Baseline state machine
- STABLE / DYNAMIC / SETTLING states
- Baseline protection during dynamic movement
- Dynamic acceleration detection
- Acceleration lighting
- Braking lighting
- Left and right cornering effects
- Progressive reactive lighting
- Idle breathing effect
- Five selectable lighting modes
- Four ambient colour themes
- Startup sweep animation
- Manual RGB brightness scaling
- Rotary encoder brightness control
- Responsive quadrature encoder handling
- MPU6050 calibration
- Adjustable sensitivity
- Physical master power control

### Development Objective

The firmware development process has progressively refined the system from direct raw accelerometer readings into a more robust vehicle-dynamics detection system.

The primary objective of v3.6 is to ensure that the reactive lighting responds to genuine changes in vehicle dynamics while remaining stable during long-term changes in vehicle orientation, such as sustained uphill and downhill driving.

The firmware is now considered ready for final hardware integration and real-world vehicle testing.