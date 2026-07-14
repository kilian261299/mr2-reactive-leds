# Build Log

## Stage 1 — Planning

Created the initial GitHub repository and documented the project plan.

The project was simplified from a GPS/OLED speed display into an accelerometer-only reactive LED system.

Version 1 will use:

- ESP32-C3 Super Mini
- MPU6050 / GY-521 accelerometer
- WS2812B addressable LED strip
- SN74AHCT125N level shifter
- Rotary encoder
- 12V to 5V buck converter
- Fused power from the cigarette lighter circuit
- Gebildet Metal Toggle Switch
- Custom EasyEDA PCB with JST-XH connectors

## Stage 2 — Parts Ordered

Parts have been ordered.

Ordered components:

- ESP32-C3 Super Mini
- MPU6050 / GY-521 accelerometer
- 2 × 1m WS2812B addressable LED strips
- SN74AHCT125N level shifter
- Rotary encoder
- 12V to 5V buck converter
- Inline fuse holder
- JST-XH connector kit
- 18AWG red/black wire
- 30AWG wire in various colours
- VHB mounting tape
- Gebildet Metal Toggle Switch
- Custom EasyEDA PCB

## Stage 3 — Bench Testing Plan

The first bench tests will focus on proving each part of the system works before building the final control box or installing anything in the car.

### Test 1 — Rotary Encoder + LEDs

Goal: confirm that the ESP32-C3 can read the rotary encoder and control the WS2812B LED strips through the SN74AHCT125N level shifter.

Planned behaviour:

- Encoder rotation adjusts LED brightness
- Encoder button changes LED mode
- LEDs receive data through the SN74AHCT125N level shifter
- Test one LED strip first, then both left and right outputs

### Test 2 — Add Accelerometer

Goal: add MPU6050 accelerometer input once the LED and encoder system is working.

Planned behaviour:

- Acceleration increases LED brightness/intensity
- Braking triggers a red pulse
- Cornering creates left/right LED effects

### Test 3 — Full Bench System

Goal: test the complete system outside the car before building the final control box.

Planned setup:

- ESP32-C3
- SN74AHCT125N level shifter
- Rotary encoder
- MPU6050
- Left and right LED strips
- 5V power supply or buck converter

## Stage 4 — Bench Testing Results

Bench testing results will be recorded here as each test is completed.

### Test 1 — Rotary Encoder + LEDs

**Date:** Pending  
**Status:** Pending

**Setup:**

- ESP32-C3 powered by USB
- Rotary encoder connected to ESP32-C3
- WS2812B LED strip connected through SN74AHCT125N level shifter
- LED strip powered from 5V supply

**Goal:**

Confirm that the rotary encoder can control LED brightness and modes.

**Result:**

Pending.

**Notes:**

Pending.

### Test 2 — Add Accelerometer

**Date:** Pending  
**Status:** Pending

**Setup:**

- ESP32-C3
- MPU6050 / GY-521 accelerometer
- Rotary encoder
- WS2812B LED strip
- SN74AHCT125N level shifter

**Goal:**

Confirm that acceleration, braking, and cornering inputs can affect the LED behaviour.

**Result:**

Pending.

**Notes:**

Pending.

### Test 3 — Full Bench System

**Date:** Pending  
**Status:** Pending

**Setup:**

- ESP32-C3
- MPU6050
- Rotary encoder
- Left and right LED strips
- SN74AHCT125N level shifter
- 5V power supply or buck converter

**Goal:**

Confirm that the full system works outside the car before building the final control box.

**Result:**

Pending.

**Notes:**

Pending.

## Stage 5 — Control Box Build

Goal: transfer the tested circuit into the project box.

Planned work:

- Mount ESP32-C3, level shifter, and MPU6050
- Add connectors for power, left LED strip, right LED strip, and rotary encoder
- Keep wiring short and secure
- Confirm the MPU6050 is fixed firmly to the box

**Status:** Pending

**Build notes:**

Pending.
  
## Stage 6 — Car Installation

Goal: install the finished control box and LED strips in the MR2.

Planned work:

- Tap fused power from the rear of the cigarette lighter circuit
- Mount the control box behind the centre dash
- Route left and right LED strips
- Mount rotary encoder and toggle switch
- Test system with low brightness first

**Status:** Pending

**Installation notes:**

Pending.
