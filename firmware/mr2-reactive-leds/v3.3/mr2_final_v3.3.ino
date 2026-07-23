#include <Wire.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

/*
  ==================================================
  MR2 Reactive LEDs — Final Firmware V3.3
  ==================================================

  Hardware used:

  - ESP32-C3 Super Mini
  - MPU6050 / GY-521 accelerometer
  - KY-040 rotary encoder
  - Two WS2812B LED strips
  - SN74AHCT125N level shifter
  - 12V to 5V converter

  --------------------------------------------------
  WHAT'S NEW IN V3.3
  --------------------------------------------------

  Fixed a jittery/unreliable brightness knob (would drift up and down
  instead of moving cleanly one direction, especially near zero).

  The old rotation-reading method reacted to any single change on the
  CLK pin, trusting DT to tell it which direction. That's fragile —
  contact bounce or a slower loop (this sketch redraws 320 LEDs every
  cycle) can catch a transition mid-bounce or miss one, misreading
  direction.

  Replaced with full quadrature decoding: CLK and DT are tracked
  together as a state, and only a complete, valid 4-step sequence
  counts as one "click" of the knob. Invalid/bounced in-between states
  are ignored rather than misread, which should make brightness
  control far more reliable, including reaching a clean, stable zero.

  If your particular encoder feels like it needs two physical clicks
  per brightness step (or the reverse), adjust stepsPerDetent near
  readEncoderRotation() — most KY-040s are 4, some are 2 or 1.

  --------------------------------------------------
  WHAT'S NEW IN V3
  --------------------------------------------------

  Axis orientation is confirmed working (forward/back and left/right
  both checked), so the diagnostic modes and one-off test modes have
  been removed.

  Power is now handled by a physical switch, so the old "Off" mode
  has been removed too. Modes are now:

    Mode 0 — Reactive (main driving mode, with idle breathing)
    Mode 1 — Static: Purple
    Mode 2 — Static: Red
    Mode 3 — Static: Blue
    Mode 4 — Static: Green

  Short-press cycles through all five. Long-press still recalibrates
  the baseline, same as before. The brightness knob still sets the
  ceiling on all modes, including the static colours.

  Two new effects have been added:

  - Startup sweep: a one-off chase animation that plays down both
    strips as soon as they're powered on, before anything else runs
    (MPU detection, calibration, etc). Purely a "waking up" flourish —
    it doesn't depend on the accelerometer at all.

  - Idle breathing: while Mode 0 (Reactive) is calm — no meaningful
    acceleration, braking, or cornering — the ambient brightness now
    slowly pulses instead of sitting at a flat level. As soon as a
    real driving event happens, the reactive brightness takes over
    and the breathing effect is naturally overridden.

  If axes ever need re-checking in the future (e.g. after remounting
  or resoldering), the previous version (V2) still has the X/Y/Z
  diagnostic modes and can be reflashed temporarily for that.

  --------------------------------------------------
  WHAT'S IN HILL COMPENSATION (carried over from V2)
  --------------------------------------------------

  Added HILL_COMPENSATION: a slowly-drifting accelerometer baseline.

  Problem it solves:
  An accelerometer can't tell the difference between "tilted" and
  "genuinely accelerating" — both produce the same reading. On a hill,
  the car's constant pitch would otherwise show up as constant fake
  acceleration/braking colour, even at a steady speed.

  How it works:
  Instead of a single fixed baseline captured once at calibration, the
  baseline now slowly drifts to follow sustained changes (like a hill),
  while leaving quick changes (real acceleration/braking/cornering)
  alone. Think of it as: "if this reading has been roughly steady for
  a few seconds, treat it as the new normal."

  Tuning knob: baselineDriftRate (near the top of the code).
    Smaller = slower to follow a hill, but better at ignoring genuine
              sustained acceleration events.
    Larger  = settles into a hill's tilt faster, but starts treating
              longer real acceleration (like a long motorway on-ramp)
              as "normal" too, which mutes the effect.

  Set HILL_COMPENSATION to false to fully disable this and go back to
  the original fixed-baseline behaviour, for comparison.

  --------------------------------------------------
  WHAT THIS CODE DOES
  --------------------------------------------------

  This code controls two LED strips in the MR2.

  The LEDs react to movement measured by the MPU6050 accelerometer.

  Rotary encoder controls:

  - Turn knob:
      Adjust maximum LED brightness.

  - Short press:
      Change LED mode.

  - Long press:
      Recalibrate the accelerometer baseline.

  --------------------------------------------------
  IMPORTANT BRIGHTNESS NOTE
  --------------------------------------------------

  The knob sets the maximum brightness.

  The LEDs will not go brighter than the userBrightness value.

  Example:

    userBrightness = 80

    Smooth driving:
      dim ambient brightness, roughly 25% of 80

    Hard acceleration:
      brightness rises up to 80

    Hard braking:
      brightness rises up to 80

  So acceleration and braking do not exceed the brightness set by the knob.

  --------------------------------------------------
  MODES
  --------------------------------------------------

  Mode 0 — Reactive (main driving mode)

    Calm / idle:
      dim blue, slowly breathing

    Acceleration:
      blue fades toward orange and gets brighter

    Braking:
      red glow, stronger braking = brighter red

    Cornering:
      one side gets brighter than the other

    Acceleration around a corner:
      orange/brighter + left/right brightness shift

    Braking around a corner:
      red + left/right brightness shift

  --------------------------------------------------

  Mode 1 — Purple, themed reactive
  Mode 2 — Red, themed reactive
  Mode 3 — Blue, themed reactive
  Mode 4 — Green, themed reactive

    Colour stays fixed, but brightness reacts to overall movement
    (not direction — just "moving" vs "calm") and cornering (left/
    right brightness shift, same as Mode 0), and gently breathes
    when the car is still. Knob still sets the overall brightness
    ceiling.

  --------------------------------------------------
  HOW TO SET UP AFTER INSTALLING
  --------------------------------------------------

  1. Mount the MPU6050 flat and straight if possible.
  2. Turn the car on.
  3. Keep the car still, on flat/level ground.
  4. Long press the encoder to calibrate.
  5. Wait for green confirmation flash.

  Axis orientation (FORWARD_AXIS/SIGN, SIDE_AXIS/SIGN below) was
  confirmed during bench + tilt testing. If the car is remounted or
  resoldered later, reflash V2 temporarily to re-check axes using its
  diagnostic modes, then come back to this version.

  --------------------------------------------------
  POWER NOTES
  --------------------------------------------------

  - All grounds must be common.
  - LED strips must be powered from the 5V converter.
  - Do not power the LED strips from the ESP32.
  - Use a fuse on the 12V input side.
  - Keep brightness low during first tests.
*/


// ==================================================
// PIN ASSIGNMENTS
// ==================================================

#define LEFT_LED_PIN   2
#define RIGHT_LED_PIN  3

#define ENCODER_CLK 4
#define ENCODER_DT  5
#define ENCODER_SW  6

#define I2C_SDA 8
#define I2C_SCL 9


// ==================================================
// LED SETUP
// ==================================================

#define NUM_LEDS_LEFT   160
#define NUM_LEDS_RIGHT  160

Adafruit_NeoPixel leftStrip(NUM_LEDS_LEFT, LEFT_LED_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel rightStrip(NUM_LEDS_RIGHT, RIGHT_LED_PIN, NEO_GRB + NEO_KHZ800);


// ==================================================
// MPU6050 SETUP
// ==================================================

Adafruit_MPU6050 mpu;

// Fixed baseline, captured once at calibration.
// Kept as a reference / fallback and as the starting point for the
// drifting baseline below.
float baseX = 0.0;
float baseY = 0.0;
float baseZ = 0.0;


// ==================================================
// HILL COMPENSATION (drifting baseline)
// ==================================================

// Set to false to go back to the original fixed-baseline behaviour.
#define HILL_COMPENSATION true

// How quickly the baseline follows a sustained change (like a hill).
// This is a "per loop" blend factor — smaller number = slower drift.
// At ~50 loops/second, 0.004 settles into a new steady tilt over
// roughly 3-5 seconds, which should be slow enough to leave real
// braking/accelerating/cornering events alone.
const float baselineDriftRate = 0.004;

// These start equal to the calibrated baseline and then drift over time.
float driftBaseX = 0.0;
float driftBaseY = 0.0;
float driftBaseZ = 0.0;

void updateDriftingBaseline(float rawX, float rawY, float rawZ) {
#if HILL_COMPENSATION
  driftBaseX = driftBaseX * (1.0 - baselineDriftRate) + rawX * baselineDriftRate;
  driftBaseY = driftBaseY * (1.0 - baselineDriftRate) + rawY * baselineDriftRate;
  driftBaseZ = driftBaseZ * (1.0 - baselineDriftRate) + rawZ * baselineDriftRate;
#endif
}


// ==================================================
// AXIS CONFIGURATION
// ==================================================

#define AXIS_X 0
#define AXIS_Y 1
#define AXIS_Z 2

// Default assumption:
// X = forward/back
// Y = left/right
//
// Since you plan to mount the MPU6050 flat under the shifter surround,
// these are likely to be close.
const int FORWARD_AXIS = AXIS_X;
const int FORWARD_SIGN = -1;

const int SIDE_AXIS = AXIS_Y;
const int SIDE_SIGN = -1;


// ==================================================
// USER SETTINGS
// ==================================================

// This is the maximum brightness limit controlled by the encoder.
// Effects will not go above this value.
int userBrightness = 80;

const int minBrightness = 0;
const int maxBrightness = 220;

int mode = 0;
const int numberOfModes = 5;


// ==================================================
// ENCODER / BUTTON VARIABLES
// ==================================================

// (Quadrature state tracking for the encoder — encoderState and
// encoderAccumulatedSteps — is declared with readEncoderRotation()
// further down.)

bool buttonWasDown = false;
bool longPressHandled = false;

unsigned long buttonDownTime = 0;
unsigned long lastButtonEvent = 0;

const unsigned long debounceDelay = 80;
const unsigned long longPressTime = 1200;


// ==================================================
// TIMING VARIABLES
// ==================================================

unsigned long lastSerialPrint = 0;
const unsigned long serialPrintInterval = 300;


// ==================================================
// BASIC HELPERS
// ==================================================

float getSelectedAxis(float xG, float yG, float zG, int axis) {
  if (axis == AXIS_X) return xG;
  if (axis == AXIS_Y) return yG;
  return zG;
}


// ==================================================
// COLOUR HELPERS
// ==================================================

uint32_t blueToOrange(float amount, Adafruit_NeoPixel &strip) {
  amount = constrain(amount, 0.0, 1.0);

  // Orange target colour. WS2812 green LEDs tend to look
  // disproportionately bright to the eye compared to their numeric
  // value, so even a fairly low green value here can visually read
  // as yellow. Lower orangeG further for a deeper/more red-leaning
  // orange, raise it for a warmer amber.
  const int orangeR = 255;
  const int orangeG = 40;
  const int orangeB = 0;

  int r = 0 + (orangeR - 0) * amount;
  int g = 0 + (orangeG - 0) * amount;
  int b = 255 + (orangeB - 255) * amount;

  return strip.Color(r, g, b);
}

// ==================================================
// LED OUTPUT HELPERS
// ==================================================

void setStrip(Adafruit_NeoPixel &strip, int ledCount, uint32_t colour, int brightness) {
  brightness = constrain(brightness, 0, 255);

  strip.setBrightness(brightness);

  for (int i = 0; i < ledCount; i++) {
    strip.setPixelColor(i, colour);
  }

  strip.show();
}

void setBothStrips(uint32_t leftColour, uint32_t rightColour, int leftBrightness, int rightBrightness) {
  setStrip(leftStrip, NUM_LEDS_LEFT, leftColour, leftBrightness);
  setStrip(rightStrip, NUM_LEDS_RIGHT, rightColour, rightBrightness);
}

void allOff() {
  setBothStrips(
    leftStrip.Color(0, 0, 0),
    rightStrip.Color(0, 0, 0),
    0,
    0
  );
}

void flashConfirmation(uint32_t colour) {
  for (int i = 0; i < 2; i++) {
    setBothStrips(colour, colour, 70, 70);
    delay(150);
    allOff();
    delay(150);
  }
}


// ==================================================
// STARTUP SWEEP ANIMATION
// ==================================================

// Plays once at power-on, before the MPU6050 or accelerometer logic
// is touched. A short fading "comet" runs down both strips together,
// then clears. Purely a visual flourish — change sweepR/G/B to pick
// a different accent colour, or sweepDelay to change the speed
// (smaller = faster).
void startupSweep() {
  const int sweepR = 0;
  const int sweepG = 120;
  const int sweepB = 255;

  const int tailLength = 14;
  const int sweepDelay = 3; // ms between steps — lower is faster

  leftStrip.setBrightness(userBrightness);
  rightStrip.setBrightness(userBrightness);

  for (int head = 0; head < NUM_LEDS_LEFT + tailLength; head++) {
    for (int i = 0; i < NUM_LEDS_LEFT; i++) {
      int distanceBehindHead = head - i;

      if (distanceBehindHead >= 0 && distanceBehindHead < tailLength) {
        int fade = map(distanceBehindHead, 0, tailLength, 255, 0);

        uint32_t colour = leftStrip.Color(
          (sweepR * fade) / 255,
          (sweepG * fade) / 255,
          (sweepB * fade) / 255
        );

        leftStrip.setPixelColor(i, colour);
        rightStrip.setPixelColor(i, colour);
      } else {
        leftStrip.setPixelColor(i, 0);
        rightStrip.setPixelColor(i, 0);
      }
    }

    leftStrip.show();
    rightStrip.show();

    delay(sweepDelay);
  }

  allOff();
  delay(200);
}


// ==================================================
// MPU6050 READING
// ==================================================

void readAcceleration(float &xG, float &yG, float &zG, float &movementG) {
  sensors_event_t accel;
  sensors_event_t gyro;
  sensors_event_t temp;

  mpu.getEvent(&accel, &gyro, &temp);

  // Slowly adjust the baseline to track gentle, sustained tilt (e.g.
  // hills), while leaving quick changes (real acceleration, braking,
  // cornering) alone. No effect if HILL_COMPENSATION is false.
  updateDriftingBaseline(accel.acceleration.x, accel.acceleration.y, accel.acceleration.z);

  xG = (accel.acceleration.x - driftBaseX) / 9.81;
  yG = (accel.acceleration.y - driftBaseY) / 9.81;
  zG = (accel.acceleration.z - driftBaseZ) / 9.81;

  movementG = sqrt((xG * xG) + (yG * yG) + (zG * zG));
}

void calibrateMPU6050() {
  Serial.println("Calibrating MPU6050...");
  Serial.println("Keep the car/box still.");

  allOff();
  delay(200);

  const int samples = 120;

  float totalX = 0.0;
  float totalY = 0.0;
  float totalZ = 0.0;

  for (int i = 0; i < samples; i++) {
    sensors_event_t accel;
    sensors_event_t gyro;
    sensors_event_t temp;

    mpu.getEvent(&accel, &gyro, &temp);

    totalX += accel.acceleration.x;
    totalY += accel.acceleration.y;
    totalZ += accel.acceleration.z;

    delay(10);
  }

  baseX = totalX / samples;
  baseY = totalY / samples;
  baseZ = totalZ / samples;

  // Reset the drifting baseline back to this fresh calibration too,
  // so recalibrating always gives an immediate, exact zero rather than
  // slowly drifting there from whatever it was tracking before.
  driftBaseX = baseX;
  driftBaseY = baseY;
  driftBaseZ = baseZ;

  Serial.println("Calibration complete.");
  Serial.print("Base X: ");
  Serial.println(baseX);
  Serial.print("Base Y: ");
  Serial.println(baseY);
  Serial.print("Base Z: ");
  Serial.println(baseZ);

  flashConfirmation(leftStrip.Color(0, 255, 0));
}


// ==================================================
// SIDE / CORNERING BRIGHTNESS HELPER
// ==================================================

// This takes a base brightness and adjusts left/right brightness
// depending on cornering force.
//
// Positive sideG makes the left strip brighter.
// Negative sideG makes the right strip brighter.
//
// If the effect feels backwards in the car,
// change SIDE_SIGN from 1 to -1 near the top of the code.
void applyCorneringBrightness(int baseBrightness, float sideG, int &leftBrightness, int &rightBrightness) {
  float sideIntensity = abs(sideG) / 0.35;
  sideIntensity = constrain(sideIntensity, 0.0, 1.0);

  leftBrightness = baseBrightness;
  rightBrightness = baseBrightness;

  if (sideG > 0.06) {
    leftBrightness = baseBrightness + (userBrightness * 0.4 * sideIntensity);
    rightBrightness = baseBrightness * (1.0 - 0.4 * sideIntensity);
  } else if (sideG < -0.06) {
    rightBrightness = baseBrightness + (userBrightness * 0.4 * sideIntensity);
    leftBrightness = baseBrightness * (1.0 - 0.4 * sideIntensity);
  }

  leftBrightness = constrain(leftBrightness, 0, userBrightness);
  rightBrightness = constrain(rightBrightness, 0, userBrightness);
}


// ==================================================
// MAIN LED BEHAVIOUR
// ==================================================

// ==================================================
// IDLE BREATHING EFFECT
// ==================================================

// Returns a value that slowly oscillates between 0.6 and 1.0 over a
// ~4 second cycle, used to gently pulse the ambient brightness when
// Mode 0 is calm. Purely cosmetic — has no effect once real
// acceleration/braking/cornering pushes brightness up, since that
// quickly outweighs this small pulse.
float breathingMultiplier() {
  float phase = (millis() % 4000) / 4000.0 * 2.0 * PI;
  float wave = (sin(phase) + 1.0) / 2.0; // 0.0 to 1.0
  return 0.6 + (0.4 * wave); // 0.6 to 1.0
}

// Used by the four static theme modes (Purple/Red/Blue/Green).
// Colour stays fixed, but brightness reacts to overall movement and
// cornering, and breathes gently when the car is still — same
// breathing floor and cornering shift as Mode 0, just without any
// hue change. Uses movementG (overall G) for the base reactive level
// since these modes don't need to distinguish forward/back, only
// "moving" vs "calm" — but sideG still drives the left/right shift
// so cornering is visible here too.
void updateStaticThemeMode(int r, int g, int b, float movementG, float sideG) {
  int ambientBrightness = userBrightness * 0.25 * breathingMultiplier();

  float intensity = movementG / 0.50;
  intensity = constrain(intensity, 0.0, 1.0);

  int themeBrightness = ambientBrightness + ((userBrightness - ambientBrightness) * intensity);
  themeBrightness = constrain(themeBrightness, 0, userBrightness);

  int leftBrightness;
  int rightBrightness;

  applyCorneringBrightness(themeBrightness, sideG, leftBrightness, rightBrightness);

  setBothStrips(
    leftStrip.Color(r, g, b),
    rightStrip.Color(r, g, b),
    leftBrightness,
    rightBrightness
  );
}

void updateMainReactiveMode(float forwardG, float sideG, float movementG) {
  // Positive forwardG = acceleration.
  // Negative forwardG = braking.
  //
  // Cornering is applied in both acceleration and braking.
  // So:
  // - accelerating around a corner = orange + side brightness shift
  // - braking around a corner = red + side brightness shift

  int ambientBrightness = userBrightness * 0.25 * breathingMultiplier();

  // --------------------------------------------------
  // BRAKING
  // --------------------------------------------------

  if (forwardG < -0.10) {
    float brakeIntensity = abs(forwardG) / 0.40;
    brakeIntensity = constrain(brakeIntensity, 0.0, 1.0);

    int brakeBrightness = ambientBrightness + ((userBrightness - ambientBrightness) * brakeIntensity);
    brakeBrightness = constrain(brakeBrightness, 0, userBrightness);

    int leftBrightness;
    int rightBrightness;

    // Apply cornering even while braking.
    applyCorneringBrightness(brakeBrightness, sideG, leftBrightness, rightBrightness);

    setBothStrips(
      leftStrip.Color(255, 0, 0),
      rightStrip.Color(255, 0, 0),
      leftBrightness,
      rightBrightness
    );

    return;
  }

  // --------------------------------------------------
  // ACCELERATION / NORMAL DRIVING
  // --------------------------------------------------

  float accelIntensity = 0.0;

  if (forwardG > 0.02) {
    accelIntensity = forwardG / 0.35;
  }

  accelIntensity = constrain(accelIntensity, 0.0, 1.0);

  int reactiveBrightness = ambientBrightness + ((userBrightness - ambientBrightness) * accelIntensity);
  reactiveBrightness = constrain(reactiveBrightness, 0, userBrightness);

  uint32_t leftColour = blueToOrange(accelIntensity, leftStrip);
  uint32_t rightColour = blueToOrange(accelIntensity, rightStrip);

  int leftBrightness;
  int rightBrightness;

  // Apply cornering during normal driving and acceleration.
  applyCorneringBrightness(reactiveBrightness, sideG, leftBrightness, rightBrightness);

  setBothStrips(leftColour, rightColour, leftBrightness, rightBrightness);
}


// ==================================================
// UPDATE LEDS FOR CURRENT MODE
// ==================================================

void updateLEDs() {
  float xG, yG, zG, movementG;
  readAcceleration(xG, yG, zG, movementG);

  float forwardG = getSelectedAxis(xG, yG, zG, FORWARD_AXIS) * FORWARD_SIGN;
  float sideG = getSelectedAxis(xG, yG, zG, SIDE_AXIS) * SIDE_SIGN;

  switch (mode) {
    case 0:
      updateMainReactiveMode(forwardG, sideG, movementG);
      break;

    case 1:
      // Purple, brightness reacts to movement, breathes when calm
      updateStaticThemeMode(54, 1, 63, movementG, sideG);
      break;

    case 2:
      // Red, brightness reacts to movement, breathes when calm
      updateStaticThemeMode(255, 0, 0, movementG, sideG);
      break;

    case 3:
      // Blue, brightness reacts to movement, breathes when calm
      updateStaticThemeMode(0, 0, 255, movementG, sideG);
      break;

    case 4:
      // Green, brightness reacts to movement, breathes when calm
      updateStaticThemeMode(0, 255, 10, movementG, sideG);
      break;
  }

  if (millis() - lastSerialPrint > serialPrintInterval) {
    Serial.print("Mode: ");
    Serial.print(mode);

    Serial.print(" | Brightness max: ");
    Serial.print(userBrightness);

    Serial.print(" | X: ");
    Serial.print(xG, 2);

    Serial.print("g | Y: ");
    Serial.print(yG, 2);

    Serial.print("g | Z: ");
    Serial.print(zG, 2);

    Serial.print("g | Forward: ");
    Serial.print(forwardG, 2);

    Serial.print("g | Side: ");
    Serial.print(sideG, 2);

    Serial.print("g | Movement: ");
    Serial.print(movementG, 2);

    Serial.println("g");

    lastSerialPrint = millis();
  }
}


// ==================================================
// ENCODER HANDLING
// ==================================================

// Full quadrature state-transition decoding.
//
// The old approach ("any time CLK changes, check DT") relies on
// catching every single transition cleanly. That's fragile — contact
// bounce or a slow/busy loop (like this one, redrawing 320 LEDs every
// cycle) can catch a pin mid-bounce or miss a transition, which
// throws off the CLK/DT pairing and misreads direction. That's what
// caused brightness to drift up and down instead of moving cleanly
// one way.
//
// This table tracks CLK+DT together as a 2-bit "state" and looks at
// the last 2 states combined (4 bits total) to decide if a full,
// valid quadrature step has occurred. Invalid/bounced transitions
// simply return 0 (ignored) instead of being misread as a direction —
// much more resistant to noise and timing variation.
const int8_t encoderTransitionTable[16] = {
   0, -1,  1,  0,
   1,  0,  0, -1,
  -1,  0,  0,  1,
   0,  1, -1,  0
};

uint8_t encoderState = 0;
int8_t encoderAccumulatedSteps = 0;

// Most KY-040 encoders produce 4 valid quarter-steps per physical
// "click" of the knob. If your knob feels like it needs two clicks
// per brightness change (or changes brightness twice per click),
// try 2 or 1 here instead.
const int8_t stepsPerDetent = 4;

void readEncoderRotation() {
  int clk = digitalRead(ENCODER_CLK);
  int dt  = digitalRead(ENCODER_DT);

  uint8_t newState = (clk << 1) | dt;
  encoderState = ((encoderState << 2) | newState) & 0x0F;

  int8_t movement = encoderTransitionTable[encoderState];

  if (movement != 0) {
    encoderAccumulatedSteps += movement;

    if (encoderAccumulatedSteps >= stepsPerDetent) {
      userBrightness += 5;
      encoderAccumulatedSteps = 0;

      userBrightness = constrain(userBrightness, minBrightness, maxBrightness);
      Serial.print("Brightness max set to: ");
      Serial.println(userBrightness);
    } else if (encoderAccumulatedSteps <= -stepsPerDetent) {
      userBrightness -= 5;
      encoderAccumulatedSteps = 0;

      userBrightness = constrain(userBrightness, minBrightness, maxBrightness);
      Serial.print("Brightness max set to: ");
      Serial.println(userBrightness);
    }
  }
}

void readEncoderButton() {
  bool buttonDown = digitalRead(ENCODER_SW) == LOW;
  unsigned long now = millis();

  if (buttonDown && !buttonWasDown) {
    buttonWasDown = true;
    longPressHandled = false;
    buttonDownTime = now;
  }

  if (buttonDown && !longPressHandled) {
    if (now - buttonDownTime > longPressTime) {
      longPressHandled = true;
      calibrateMPU6050();
    }
  }

  if (!buttonDown && buttonWasDown) {
    buttonWasDown = false;

    if (!longPressHandled && now - lastButtonEvent > debounceDelay) {
      mode++;

      if (mode >= numberOfModes) {
        mode = 0;
      }

      Serial.print("Changed mode to: ");
      Serial.println(mode);

      flashConfirmation(leftStrip.Color(0, 0, 255));

      lastButtonEvent = now;
    }
  }
}


// ==================================================
// SETUP
// ==================================================

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println("MR2 Reactive LEDs — Final Firmware V3.3");

#if HILL_COMPENSATION
  Serial.println("Hill compensation: ENABLED (drifting baseline)");
#else
  Serial.println("Hill compensation: DISABLED (fixed baseline)");
#endif

  pinMode(ENCODER_CLK, INPUT_PULLUP);
  pinMode(ENCODER_DT, INPUT_PULLUP);
  pinMode(ENCODER_SW, INPUT_PULLUP);

  // Seed the quadrature state with the encoder's actual starting
  // position, so the first read isn't compared against a fake "0".
  encoderState = (digitalRead(ENCODER_CLK) << 1) | digitalRead(ENCODER_DT);

  leftStrip.begin();
  rightStrip.begin();

  leftStrip.clear();
  rightStrip.clear();

  leftStrip.show();
  rightStrip.show();

  // Play the startup sweep as soon as the strips are ready, before
  // anything accelerometer-related happens.
  startupSweep();

  Wire.begin(I2C_SDA, I2C_SCL);

  if (!mpu.begin()) {
    Serial.println("ERROR: MPU6050 not found.");
    Serial.println("Check VCC, GND, SDA, and SCL wiring.");

    while (1) {
      setBothStrips(
        leftStrip.Color(255, 0, 0),
        rightStrip.Color(255, 0, 0),
        50,
        50
      );

      delay(300);

      allOff();

      delay(300);
    }
  }

  Serial.println("MPU6050 found.");

  mpu.setAccelerometerRange(MPU6050_RANGE_4_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  calibrateMPU6050();

  Serial.println("Setup complete.");
}


// ==================================================
// MAIN LOOP
// ==================================================

void loop() {
  readEncoderRotation();
  readEncoderButton();
  updateLEDs();

  delay(20);
}
