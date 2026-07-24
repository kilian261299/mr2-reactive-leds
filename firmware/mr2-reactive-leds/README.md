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

This version used the raw accelerometer readings directly.

While effective during normal driving, sustained gradients such as hills could cause the LEDs to remain in an acceleration or braking state.

The system could therefore interpret changes in vehicle pitch caused by a hill as genuine vehicle acceleration or braking.

---

## v2.0 – Dynamic Baseline Filtering

Improved acceleration detection by reacting to changes in acceleration rather than absolute sensor readings.

### Improvements

Instead of using the raw longitudinal acceleration value directly, the firmware continuously maintains a slowly updating baseline representing the vehicle's current orientation.

Acceleration effects are then calculated from the difference between the current reading and this moving baseline.

This allows the baseline to gradually follow long-term changes, such as driving uphill or downhill, while still responding to genuine acceleration and braking events.

### Benefits

- Reduced false acceleration detection on hills.
- Reduced false braking detection on downhill sections.
- Smoother and more realistic vehicle response.
- Improved behaviour during sustained gradients.
- Reduced dependence on the absolute accelerometer reading.

### Limitation

The continuously adapting baseline could still gradually absorb genuine acceleration or braking if the dynamic movement was sustained for long enough.

This meant that the system could eventually interpret genuine vehicle movement as a new baseline.

Further development was therefore required to distinguish genuine dynamic movement from long-term changes in vehicle orientation.

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

When the vehicle is stationary or experiencing minimal movement, the reactive lighting mode produces a gentle breathing effect with an approximately four-second cycle.

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

v3.6 is currently the final planned firmware version for the MR2 Reactive LEDs project.

This version builds directly on the established v3.5 LED output, colour handling, reactive behaviour, and rotary encoder system.

The primary improvement in v3.6 is a redesigned accelerometer processing system intended to better distinguish genuine vehicle acceleration and braking from long-term changes in vehicle orientation caused by hills and gradients.

### Smart Hill Compensation

The MPU6050 measures the total acceleration acting on the sensor.

Because the accelerometer is mounted to the vehicle, a change in vehicle pitch can alter the measured acceleration on the forward axis even when the vehicle is travelling at a relatively constant speed.

For example:

- Driving onto an uphill gradient can produce an apparent forward acceleration component.
- Driving onto a downhill gradient can produce an apparent braking component.
- A sustained gradient can therefore resemble a continuous acceleration or braking event if the raw accelerometer values are used directly.

v3.6 addresses this by separating the accelerometer signal into:

- A **slow-changing gravity/orientation component**, representing long-term changes in vehicle pitch and gradient.
- A **fast-changing dynamic component**, representing genuine changes in vehicle motion.

The slow component is used to allow the system to gradually adapt to changes in vehicle orientation, while the dynamic component is used to determine whether the vehicle is currently undergoing genuine movement.

This allows the system to gradually compensate for sustained hills without immediately absorbing genuine acceleration or braking into the baseline.

---

### Dynamic Baseline Protection

The baseline is not continuously updated regardless of vehicle movement.

Instead, the firmware monitors the dynamic component of the accelerometer signal and uses this to control whether the baseline is allowed to adapt.

When genuine dynamic movement is detected:

- The baseline is frozen.
- The current acceleration and braking response remains available to the reactive lighting system.
- The system does not immediately learn the dynamic movement as a new vehicle orientation.

This prevents a sustained acceleration or braking event from gradually becoming the new baseline.

---

### Baseline State Machine

The smart baseline system operates using three states:

#### STABLE

The vehicle is considered sufficiently stable.

The baseline is allowed to adapt slowly.

This allows the system to gradually follow long-term changes in vehicle orientation, such as:

- Driving onto an uphill gradient.
- Driving onto a downhill gradient.
- Sustained changes in vehicle pitch.

The purpose of this state is to allow the system to distinguish a long-term change in orientation from a temporary acceleration or braking event.

---

#### DYNAMIC

The system has detected genuine dynamic movement.

The adaptive baseline is frozen.

This prevents the system from immediately absorbing genuine acceleration, braking, or other dynamic movement into the long-term baseline.

The system remains in this state while meaningful dynamic movement is present.

During this state:

- The adaptive baseline does not follow the dynamic acceleration.
- The reactive LED system continues to respond to the detected vehicle dynamics.
- The system waits for the dynamic movement to end before considering baseline adaptation again.

---

#### SETTLING

The dynamic movement has ended.

The system does not immediately return to baseline adaptation.

Instead, it enters a temporary settling period during which the baseline remains frozen.

This prevents the end of an acceleration or braking event from immediately becoming part of the adaptive baseline.

If significant dynamic movement resumes during the settling period, the system returns to the DYNAMIC state.

Once the settling period has completed and the vehicle remains sufficiently stable, the system returns to STABLE and slow baseline adaptation resumes.

---

### State Transition Behaviour

The intended behaviour is:

```text
                 Vehicle stable
                      |
                      v
                   STABLE
                      |
                      | Slow baseline adaptation
                      |
          Genuine dynamic movement detected
                      |
                      v
                   DYNAMIC
                      |
                      | Baseline frozen
                      |
          Dynamic movement stops
                      |
                      v
                  SETTLING
                      |
                      | Baseline remains frozen
                      |
              +-------+-------+
              |               |
     Movement resumes     Vehicle remains calm
              |               |
              v               v
           DYNAMIC          STABLE
                              |
                              | Slow baseline adaptation
                              v
                           Continue