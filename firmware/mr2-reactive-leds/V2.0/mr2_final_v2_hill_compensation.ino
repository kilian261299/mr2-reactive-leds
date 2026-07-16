#include <Wire.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

/*
  ==================================================
  MR2 Reactive LEDs — Final Firmware V2
  ==================================================

  Hardware used:

  - ESP32-C3 Super Mini
  - MPU6050 / GY-521 accelerometer
  - KY-040 rotary encoder
  - Two WS2812B LED strips
  - SN74AHCT125N level shifter
  - 12V to 5V converter

  --------------------------------------------------
  WHAT'S NEW IN V2
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

  Mode 0 — Main reactive driving mode

    Smooth driving:
      dim blue

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

  Mode 1 — X-axis diagnostic

    Shows X-axis movement using red.

    Bright red:
      X positive

    Dim/dark red:
      X negative

  --------------------------------------------------

  Mode 2 — Y-axis diagnostic

    Shows Y-axis movement using green.

    Bright green:
      Y positive

    Dim/dark green:
      Y negative

  --------------------------------------------------

  Mode 3 — Z-axis diagnostic

    Shows Z-axis movement using blue.

    Bright blue:
      Z positive

    Dim/dark blue:
      Z negative

  --------------------------------------------------

  Mode 4 — Left/right output test

    Left strip:
      blue

    Right strip:
      orange

  --------------------------------------------------

  Mode 5 — White brightness/current test

    Both strips turn white.

    Be careful:
    white uses the most current.

  --------------------------------------------------

  Mode 6 — Off

    Turns both strips off.

  --------------------------------------------------
  HOW TO SET UP AFTER INSTALLING
  --------------------------------------------------

  1. Mount the MPU6050 flat and straight if possible.
  2. Turn the car on.
  3. Keep the car still, on flat/level ground.
  4. Long press the encoder.
  5. Wait for green confirmation flash.
  6. Use Mode 1, 2, and 3 to confirm axes.

  If X reacts most to acceleration/braking:

      FORWARD_AXIS = AXIS_X

  If Y reacts most to acceleration/braking:

      FORWARD_AXIS = AXIS_Y

  If Y reacts most to left/right cornering:

      SIDE_AXIS = AXIS_Y

  If the behaviour is backwards, change the sign from 1 to -1.

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
const int numberOfModes = 7;


// ==================================================
// ENCODER / BUTTON VARIABLES
// ==================================================

int lastCLKState;

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

  int r = 0 + (255 - 0) * amount;
  int g = 0 + (80 - 0) * amount;
  int b = 255 + (0 - 255) * amount;

  return strip.Color(r, g, b);
}

uint32_t diagnosticColour(float value, Adafruit_NeoPixel &strip, char axisName) {
  // Diagnostic mode behaviour:
  //
  // value near 0:
  //   medium brightness
  //
  // positive value:
  //   brighter than baseline
  //
  // negative value:
  //   dimmer than baseline
  //
  // This makes it easier to see both directions while driving.

  float limitedValue = constrain(value, -0.50, 0.50);

  // Convert -0.50g to +0.50g into brightness range.
  //
  // -0.50g = low brightness
  //  0.00g = medium brightness
  // +0.50g = high brightness
  int scaledValue = limitedValue * 100;
  int level = map(scaledValue, -50, 50, 10, 255);

  level = constrain(level, 10, 255);

  if (axisName == 'X') {
    return strip.Color(level, 0, 0);
  }

  if (axisName == 'Y') {
    return strip.Color(0, level, 0);
  }

  return strip.Color(0, 0, level);
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

void updateMainReactiveMode(float forwardG, float sideG, float movementG) {
  // Positive forwardG = acceleration.
  // Negative forwardG = braking.
  //
  // Cornering is applied in both acceleration and braking.
  // So:
  // - accelerating around a corner = orange + side brightness shift
  // - braking around a corner = red + side brightness shift

  int ambientBrightness = userBrightness * 0.25;

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

  int calmBrightness = userBrightness * 0.35;

  switch (mode) {
    case 0:
      updateMainReactiveMode(forwardG, sideG, movementG);
      break;

    case 1:
      setBothStrips(
        diagnosticColour(xG, leftStrip, 'X'),
        diagnosticColour(xG, rightStrip, 'X'),
        calmBrightness,
        calmBrightness
      );
      break;

    case 2:
      setBothStrips(
        diagnosticColour(yG, leftStrip, 'Y'),
        diagnosticColour(yG, rightStrip, 'Y'),
        calmBrightness,
        calmBrightness
      );
      break;

    case 3:
      setBothStrips(
        diagnosticColour(zG, leftStrip, 'Z'),
        diagnosticColour(zG, rightStrip, 'Z'),
        calmBrightness,
        calmBrightness
      );
      break;

    case 4:
      setBothStrips(
        leftStrip.Color(0, 0, 255),
        rightStrip.Color(255, 80, 0),
        userBrightness,
        userBrightness
      );
      break;

    case 5:
      setBothStrips(
        leftStrip.Color(255, 255, 255),
        rightStrip.Color(255, 255, 255),
        userBrightness,
        userBrightness
      );
      break;

    case 6:
      allOff();
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

void readEncoderRotation() {
  int currentCLKState = digitalRead(ENCODER_CLK);

  if (currentCLKState != lastCLKState) {
    if (digitalRead(ENCODER_DT) != currentCLKState) {
      userBrightness += 5;
    } else {
      userBrightness -= 5;
    }

    userBrightness = constrain(userBrightness, minBrightness, maxBrightness);

    Serial.print("Brightness max set to: ");
    Serial.println(userBrightness);
  }

  lastCLKState = currentCLKState;
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

  Serial.println("MR2 Reactive LEDs — Final Firmware V2");

#if HILL_COMPENSATION
  Serial.println("Hill compensation: ENABLED (drifting baseline)");
#else
  Serial.println("Hill compensation: DISABLED (fixed baseline)");
#endif

  pinMode(ENCODER_CLK, INPUT_PULLUP);
  pinMode(ENCODER_DT, INPUT_PULLUP);
  pinMode(ENCODER_SW, INPUT_PULLUP);

  lastCLKState = digitalRead(ENCODER_CLK);

  leftStrip.begin();
  rightStrip.begin();

  leftStrip.clear();
  rightStrip.clear();

  leftStrip.show();
  rightStrip.show();

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
