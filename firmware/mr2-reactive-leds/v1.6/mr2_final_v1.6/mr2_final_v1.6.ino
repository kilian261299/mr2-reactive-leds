#include <Wire.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

/*
  ==================================================
  MR2 Reactive LEDs — Final Firmware V3.4
  ==================================================

  Hardware used:

  - ESP32-C3 Super Mini
  - MPU6050 / GY-521 accelerometer
  - KY-040 rotary encoder
  - Two WS2812B LED strips
  - SN74AHCT125N level shifter
  - 12V to 5V converter

  --------------------------------------------------
  WHAT'S NEW IN V3.4
  --------------------------------------------------

  Improved rotary encoder responsiveness.

  V3.3 used full quadrature decoding, which fixed the
  previous brightness jitter and incorrect direction
  behaviour. However, the encoder was still being
  polled from the main loop.

  The main loop also performs several relatively slow
  operations, including updating 320 WS2812B LEDs.

  Because WS2812B data transmission temporarily keeps
  the processor busy, rapid encoder movements could
  still result in missed transitions.

  V3.4 moves rotary encoder quadrature detection onto
  GPIO interrupts.

  The encoder CLK and DT signals are now monitored
  asynchronously by the ESP32-C3. Valid quadrature
  transitions are accumulated by the interrupt handler,
  allowing rapid knob movement to be captured even
  while the main loop is updating the LED strips.

  The interrupt handler performs only very small,
  time-critical operations:

  - Read CLK and DT.
  - Decode the quadrature transition.
  - Accumulate movement.

  LED updates, brightness limiting and Serial output
  remain in the normal main loop.

  This should provide:

  - Better response during rapid encoder rotation.
  - Fewer missed encoder transitions.
  - Reliable direction detection.
  - Stable brightness control.
  - Clean movement towards zero brightness.

  The brightness step remains 5 units.

  If the encoder feels too sensitive or not sensitive
  enough, adjust stepsPerDetent below.

  --------------------------------------------------
  WHAT'S NEW IN V3.3
  --------------------------------------------------

  Fixed a jittery/unreliable brightness knob.

  The old rotation-reading method reacted to any single
  change on the CLK pin, trusting DT to tell it which
  direction. That's fragile — contact bounce or a
  slower loop can catch a transition mid-bounce or
  miss one, misreading direction.

  Replaced with full quadrature decoding.

  CLK and DT are tracked together as a state, and only
  valid quadrature transitions count as movement.

  --------------------------------------------------
  WHAT'S IN V3
  --------------------------------------------------

  Axis orientation is confirmed working.

  Diagnostic modes and one-off test modes were removed.

  Power is now handled by a physical switch, so the
  old software "Off" mode was removed.

  Modes are now:

    Mode 0 — Reactive
    Mode 1 — Static Purple
    Mode 2 — Static Red
    Mode 3 — Static Blue
    Mode 4 — Static Green

  Short-press cycles through all five modes.

  Long-press recalibrates the accelerometer baseline.

  The brightness knob sets the maximum brightness
  ceiling for all modes.

  --------------------------------------------------
  EFFECTS
  --------------------------------------------------

  Startup sweep:

  A one-off chase animation runs across both strips
  during startup.

  Idle breathing:

  Mode 0 gently pulses while the car is calm.

  The static theme modes also use the breathing effect.

  --------------------------------------------------
  HILL COMPENSATION
  --------------------------------------------------

  A slowly drifting accelerometer baseline is used to
  compensate for sustained vehicle pitch changes.

  This helps prevent hills from being interpreted as
  continuous acceleration or braking.

  Set HILL_COMPENSATION to false to disable this.

  --------------------------------------------------
  ROTARY ENCODER
  --------------------------------------------------

  Turn knob:

    Adjust maximum LED brightness.

  Short press:

    Change LED mode.

  Long press:

    Recalibrate accelerometer baseline.

  --------------------------------------------------
  IMPORTANT BRIGHTNESS NOTE
  --------------------------------------------------

  The knob sets the maximum brightness.

  The LEDs will not go brighter than userBrightness.

  Example:

    userBrightness = 80

    Smooth driving:
      dim ambient brightness

    Hard acceleration:
      brightness rises up to 80

    Hard braking:
      brightness rises up to 80

  --------------------------------------------------
  MODES
  --------------------------------------------------

  Mode 0 — Reactive

    Calm / idle:
      dim blue, slowly breathing

    Acceleration:
      blue fades toward orange

    Braking:
      red glow

    Cornering:
      left/right brightness shift

  Mode 1 — Purple reactive theme
  Mode 2 — Red reactive theme
  Mode 3 — Blue reactive theme
  Mode 4 — Green reactive theme

    Colour remains fixed.

    Brightness reacts to overall movement.

    Cornering creates left/right brightness shifts.

    Calm conditions produce a breathing effect.

  --------------------------------------------------
  INSTALLATION
  --------------------------------------------------

  1. Mount the MPU6050 flat and straight if possible.
  2. Turn the car on.
  3. Keep the car still on flat ground.
  4. Long press the encoder to calibrate.
  5. Wait for green confirmation flash.

  --------------------------------------------------
  POWER NOTES
  --------------------------------------------------

  - All grounds must be common.
  - LED strips must be powered from the 5V converter.
  - Do not power LED strips from the ESP32.
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

Adafruit_NeoPixel leftStrip(
  NUM_LEDS_LEFT,
  LEFT_LED_PIN,
  NEO_GRB + NEO_KHZ800
);

Adafruit_NeoPixel rightStrip(
  NUM_LEDS_RIGHT,
  RIGHT_LED_PIN,
  NEO_GRB + NEO_KHZ800
);


// ==================================================
// MPU6050 SETUP
// ==================================================

Adafruit_MPU6050 mpu;

float baseX = 0.0;
float baseY = 0.0;
float baseZ = 0.0;


// ==================================================
// HILL COMPENSATION
// ==================================================

#define HILL_COMPENSATION true

const float baselineDriftRate = 0.004;

float driftBaseX = 0.0;
float driftBaseY = 0.0;
float driftBaseZ = 0.0;


void updateDriftingBaseline(float rawX, float rawY, float rawZ) {

#if HILL_COMPENSATION

  driftBaseX =
    driftBaseX * (1.0 - baselineDriftRate) +
    rawX * baselineDriftRate;

  driftBaseY =
    driftBaseY * (1.0 - baselineDriftRate) +
    rawY * baselineDriftRate;

  driftBaseZ =
    driftBaseZ * (1.0 - baselineDriftRate) +
    rawZ * baselineDriftRate;

#endif
}


// ==================================================
// AXIS CONFIGURATION
// ==================================================

#define AXIS_X 0
#define AXIS_Y 1
#define AXIS_Z 2

const int FORWARD_AXIS = AXIS_X;
const int FORWARD_SIGN = -1;

const int SIDE_AXIS = AXIS_Y;
const int SIDE_SIGN = -1;


// ==================================================
// USER SETTINGS
// ==================================================

int userBrightness = 80;

const int minBrightness = 0;
const int maxBrightness = 220;

int mode = 0;
const int numberOfModes = 5;


// ==================================================
// ENCODER / BUTTON VARIABLES
// ==================================================

// Full quadrature transition table.
//
// The encoder state is represented by two bits:
//
// CLK DT
//
// The previous state and current state are combined
// into a four-bit lookup value.

const int8_t encoderTransitionTable[16] = {
   0, -1,  1,  0,
   1,  0,  0, -1,
  -1,  0,  0,  1,
   0,  1, -1,  0
};


// Current encoder state.
volatile uint8_t encoderState = 0;


// Accumulated valid quadrature movement.
//
// This is modified inside the interrupt handler and
// read by the main loop.

volatile int32_t encoderAccumulatedSteps = 0;


// Number of valid quadrature transitions required
// for one brightness adjustment.
//
// Most KY-040 encoders produce 4 transitions per
// physical detent, but some modules behave differently.
//
// V3.3 used 2 and this is retained for V3.4.

const int8_t stepsPerDetent = 2;


// Amount brightness changes per encoder step.

const int brightnessStep = 5;


// Button state variables.

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

float getSelectedAxis(
  float xG,
  float yG,
  float zG,
  int axis
) {

  if (axis == AXIS_X) return xG;
  if (axis == AXIS_Y) return yG;

  return zG;
}


// ==================================================
// COLOUR HELPERS
// ==================================================

uint32_t blueToOrange(
  float amount,
  Adafruit_NeoPixel &strip
) {

  amount = constrain(amount, 0.0, 1.0);

  const int orangeR = 255;
  const int orangeG = 40;
  const int orangeB = 0;

  int r = (orangeR - 0) * amount;
  int g = (orangeG - 0) * amount;
  int b = 255 + (orangeB - 255) * amount;

  return strip.Color(r, g, b);
}


// ==================================================
// LED OUTPUT HELPERS
// ==================================================

void setStrip(
  Adafruit_NeoPixel &strip,
  int ledCount,
  uint32_t colour,
  int brightness
) {

  brightness = constrain(brightness, 0, 255);

  strip.setBrightness(brightness);

  for (int i = 0; i < ledCount; i++) {
    strip.setPixelColor(i, colour);
  }

  strip.show();
}


void setBothStrips(
  uint32_t leftColour,
  uint32_t rightColour,
  int leftBrightness,
  int rightBrightness
) {

  setStrip(
    leftStrip,
    NUM_LEDS_LEFT,
    leftColour,
    leftBrightness
  );

  setStrip(
    rightStrip,
    NUM_LEDS_RIGHT,
    rightColour,
    rightBrightness
  );
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

    setBothStrips(
      colour,
      colour,
      70,
      70
    );

    delay(150);

    allOff();

    delay(150);
  }
}


// ==================================================
// STARTUP SWEEP ANIMATION
// ==================================================

void startupSweep() {

  const int sweepR = 0;
  const int sweepG = 120;
  const int sweepB = 255;

  const int tailLength = 14;
  const int sweepDelay = 3;

  leftStrip.setBrightness(userBrightness);
  rightStrip.setBrightness(userBrightness);

  for (
    int head = 0;
    head < NUM_LEDS_LEFT + tailLength;
    head++
  ) {

    for (
      int i = 0;
      i < NUM_LEDS_LEFT;
      i++
    ) {

      int distanceBehindHead = head - i;

      if (
        distanceBehindHead >= 0 &&
        distanceBehindHead < tailLength
      ) {

        int fade =
          map(
            distanceBehindHead,
            0,
            tailLength,
            255,
            0
          );

        uint32_t colour =
          leftStrip.Color(
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

void readAcceleration(
  float &xG,
  float &yG,
  float &zG,
  float &movementG
) {

  sensors_event_t accel;
  sensors_event_t gyro;
  sensors_event_t temp;

  mpu.getEvent(
    &accel,
    &gyro,
    &temp
  );

  updateDriftingBaseline(
    accel.acceleration.x,
    accel.acceleration.y,
    accel.acceleration.z
  );

  xG =
    (accel.acceleration.x - driftBaseX)
    / 9.81;

  yG =
    (accel.acceleration.y - driftBaseY)
    / 9.81;

  zG =
    (accel.acceleration.z - driftBaseZ)
    / 9.81;

  movementG =
    sqrt(
      (xG * xG) +
      (yG * yG) +
      (zG * zG)
    );
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

  for (
    int i = 0;
    i < samples;
    i++
  ) {

    sensors_event_t accel;
    sensors_event_t gyro;
    sensors_event_t temp;

    mpu.getEvent(
      &accel,
      &gyro,
      &temp
    );

    totalX += accel.acceleration.x;
    totalY += accel.acceleration.y;
    totalZ += accel.acceleration.z;

    delay(10);
  }

  baseX = totalX / samples;
  baseY = totalY / samples;
  baseZ = totalZ / samples;

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

  flashConfirmation(
    leftStrip.Color(0, 255, 0)
  );
}


// ==================================================
// SIDE / CORNERING BRIGHTNESS
// ==================================================

void applyCorneringBrightness(
  int baseBrightness,
  float sideG,
  int &leftBrightness,
  int &rightBrightness
) {

  float sideIntensity =
    abs(sideG) / 0.35;

  sideIntensity =
    constrain(
      sideIntensity,
      0.0,
      1.0
    );

  leftBrightness = baseBrightness;
  rightBrightness = baseBrightness;

  if (sideG > 0.06) {

    leftBrightness =
      baseBrightness +
      (
        userBrightness *
        0.4 *
        sideIntensity
      );

    rightBrightness =
      baseBrightness *
      (
        1.0 -
        0.4 *
        sideIntensity
      );

  } else if (sideG < -0.06) {

    rightBrightness =
      baseBrightness +
      (
        userBrightness *
        0.4 *
        sideIntensity
      );

    leftBrightness =
      baseBrightness *
      (
        1.0 -
        0.4 *
        sideIntensity
      );
  }

  leftBrightness =
    constrain(
      leftBrightness,
      0,
      userBrightness
    );

  rightBrightness =
    constrain(
      rightBrightness,
      0,
      userBrightness
    );
}


// ==================================================
// IDLE BREATHING EFFECT
// ==================================================

float breathingMultiplier() {

  float phase =
    (millis() % 4000)
    / 4000.0
    * 2.0
    * PI;

  float wave =
    (sin(phase) + 1.0)
    / 2.0;

  return 0.6 + (0.4 * wave);
}


// ==================================================
// STATIC THEME MODES
// ==================================================

void updateStaticThemeMode(
  int r,
  int g,
  int b,
  float movementG,
  float sideG
) {

  int ambientBrightness =
    userBrightness *
    0.25 *
    breathingMultiplier();

  float intensity =
    movementG / 0.50;

  intensity =
    constrain(
      intensity,
      0.0,
      1.0
    );

  int themeBrightness =
    ambientBrightness +
    (
      (
        userBrightness -
        ambientBrightness
      ) *
      intensity
    );

  themeBrightness =
    constrain(
      themeBrightness,
      0,
      userBrightness
    );

  int leftBrightness;
  int rightBrightness;

  applyCorneringBrightness(
    themeBrightness,
    sideG,
    leftBrightness,
    rightBrightness
  );

  setBothStrips(
    leftStrip.Color(r, g, b),
    rightStrip.Color(r, g, b),
    leftBrightness,
    rightBrightness
  );
}


// ==================================================
// MAIN REACTIVE MODE
// ==================================================

void updateMainReactiveMode(
  float forwardG,
  float sideG,
  float movementG
) {

  int ambientBrightness =
    userBrightness *
    0.25 *
    breathingMultiplier();


  // --------------------------------------------------
  // BRAKING
  // --------------------------------------------------

  if (forwardG < -0.10) {

    float brakeIntensity =
      abs(forwardG) / 0.40;

    brakeIntensity =
      constrain(
        brakeIntensity,
        0.0,
        1.0
      );

    int brakeBrightness =
      ambientBrightness +
      (
        (
          userBrightness -
          ambientBrightness
        ) *
        brakeIntensity
      );

    brakeBrightness =
      constrain(
        brakeBrightness,
        0,
        userBrightness
      );

    int leftBrightness;
    int rightBrightness;

    applyCorneringBrightness(
      brakeBrightness,
      sideG,
      leftBrightness,
      rightBrightness
    );

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

    accelIntensity =
      forwardG / 0.35;
  }

  accelIntensity =
    constrain(
      accelIntensity,
      0.0,
      1.0
    );

  int reactiveBrightness =
    ambientBrightness +
    (
      (
        userBrightness -
        ambientBrightness
      ) *
      accelIntensity
    );

  reactiveBrightness =
    constrain(
      reactiveBrightness,
      0,
      userBrightness
    );

  uint32_t leftColour =
    blueToOrange(
      accelIntensity,
      leftStrip
    );

  uint32_t rightColour =
    blueToOrange(
      accelIntensity,
      rightStrip
    );

  int leftBrightness;
  int rightBrightness;

  applyCorneringBrightness(
    reactiveBrightness,
    sideG,
    leftBrightness,
    rightBrightness
  );

  setBothStrips(
    leftColour,
    rightColour,
    leftBrightness,
    rightBrightness
  );
}


// ==================================================
// UPDATE LEDS
// ==================================================

void updateLEDs() {

  float xG;
  float yG;
  float zG;
  float movementG;

  readAcceleration(
    xG,
    yG,
    zG,
    movementG
  );

  float forwardG =
    getSelectedAxis(
      xG,
      yG,
      zG,
      FORWARD_AXIS
    ) *
    FORWARD_SIGN;

  float sideG =
    getSelectedAxis(
      xG,
      yG,
      zG,
      SIDE_AXIS
    ) *
    SIDE_SIGN;


  switch (mode) {

    case 0:

      updateMainReactiveMode(
        forwardG,
        sideG,
        movementG
      );

      break;


    case 1:

      updateStaticThemeMode(
        54,
        1,
        63,
        movementG,
        sideG
      );

      break;


    case 2:

      updateStaticThemeMode(
        255,
        0,
        0,
        movementG,
        sideG
      );

      break;


    case 3:

      updateStaticThemeMode(
        0,
        0,
        255,
        movementG,
        sideG
      );

      break;


    case 4:

      updateStaticThemeMode(
        0,
        255,
        10,
        movementG,
        sideG
      );

      break;
  }


  if (
    millis() -
    lastSerialPrint >
    serialPrintInterval
  ) {

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
// ENCODER INTERRUPT HANDLER
// ==================================================

// This function is called whenever either encoder
// signal changes state.
//
// It must remain extremely fast.
//
// Do NOT:
//
// - Use Serial.print()
// - Update LEDs
// - Call delay()
// - Perform floating point calculations
//
// The ISR only reads the encoder and accumulates
// valid quadrature movement.

void IRAM_ATTR encoderISR() {

  int clk =
    digitalRead(ENCODER_CLK);

  int dt =
    digitalRead(ENCODER_DT);

  uint8_t newState =
    (clk << 1) |
    dt;

  uint8_t transition =
    (
      (encoderState << 2) |
      newState
    ) &
    0x0F;

  int8_t movement =
    encoderTransitionTable[transition];

  encoderState =
    newState;

  if (movement != 0) {

    encoderAccumulatedSteps += movement;
  }
}


// ==================================================
// PROCESS ENCODER MOVEMENT
// ==================================================

void processEncoderRotation() {

  int32_t accumulatedSteps;

  // Copy and clear the accumulated movement
  // atomically so the ISR cannot modify it halfway
  // through the operation.

  noInterrupts();

  accumulatedSteps =
    encoderAccumulatedSteps;

  encoderAccumulatedSteps =
    0;

  interrupts();


  // Process all complete encoder detents.

  while (
    accumulatedSteps >=
    stepsPerDetent
  ) {

    userBrightness +=
      brightnessStep;

    userBrightness =
      constrain(
        userBrightness,
        minBrightness,
        maxBrightness
      );

    accumulatedSteps -=
      stepsPerDetent;

    Serial.print(
      "Brightness max set to: "
    );

    Serial.println(
      userBrightness
    );
  }


  while (
    accumulatedSteps <=
    -stepsPerDetent
  ) {

    userBrightness -=
      brightnessStep;

    userBrightness =
      constrain(
        userBrightness,
        minBrightness,
        maxBrightness
      );

    accumulatedSteps +=
      stepsPerDetent;

    Serial.print(
      "Brightness max set to: "
    );

    Serial.println(
      userBrightness
    );
  }


  // Return any incomplete movement to the
  // shared accumulator.

  if (accumulatedSteps != 0) {

    noInterrupts();

    encoderAccumulatedSteps +=
      accumulatedSteps;

    interrupts();
  }
}


// ==================================================
// ENCODER BUTTON
// ==================================================

void readEncoderButton() {

  bool buttonDown =
    digitalRead(ENCODER_SW) == LOW;

  unsigned long now =
    millis();


  // Button has just been pressed.

  if (
    buttonDown &&
    !buttonWasDown
  ) {

    buttonWasDown = true;

    longPressHandled = false;

    buttonDownTime =
      now;
  }


  // Check for long press.

  if (
    buttonDown &&
    !longPressHandled
  ) {

    if (
      now -
      buttonDownTime >
      longPressTime
    ) {

      longPressHandled = true;

      calibrateMPU6050();
    }
  }


  // Button has been released.

  if (
    !buttonDown &&
    buttonWasDown
  ) {

    buttonWasDown = false;


    // Short press.

    if (
      !longPressHandled &&
      now -
      lastButtonEvent >
      debounceDelay
    ) {

      mode++;

      if (
        mode >=
        numberOfModes
      ) {

        mode = 0;
      }

      Serial.print(
        "Changed mode to: "
      );

      Serial.println(
        mode
      );


      flashConfirmation(
        leftStrip.Color(
          0,
          0,
          255
        )
      );

      lastButtonEvent =
        now;
    }
  }
}


// ==================================================
// SETUP
// ==================================================

void setup() {

  Serial.begin(115200);

  delay(500);

  Serial.println(
    "MR2 Reactive LEDs — Final Firmware V3.4"
  );


#if HILL_COMPENSATION

  Serial.println(
    "Hill compensation: ENABLED (drifting baseline)"
  );

#else

  Serial.println(
    "Hill compensation: DISABLED (fixed baseline)"
  );

#endif


  // Encoder pins.

  pinMode(
    ENCODER_CLK,
    INPUT_PULLUP
  );

  pinMode(
    ENCODER_DT,
    INPUT_PULLUP
  );

  pinMode(
    ENCODER_SW,
    INPUT_PULLUP
  );


  // Seed the encoder state with the actual
  // physical encoder position.

  encoderState =
    (
      (digitalRead(ENCODER_CLK) << 1) |
      digitalRead(ENCODER_DT)
    );


  // Attach interrupts to both encoder channels.

  attachInterrupt(
    digitalPinToInterrupt(ENCODER_CLK),
    encoderISR,
    CHANGE
  );

  attachInterrupt(
    digitalPinToInterrupt(ENCODER_DT),
    encoderISR,
    CHANGE
  );


  // Start LED strips.

  leftStrip.begin();

  rightStrip.begin();

  leftStrip.clear();

  rightStrip.clear();

  leftStrip.show();

  rightStrip.show();


  // Startup animation.

  startupSweep();


  // Start I2C.

  Wire.begin(
    I2C_SDA,
    I2C_SCL
  );


  // Start MPU6050.

  if (!mpu.begin()) {

    Serial.println(
      "ERROR: MPU6050 not found."
    );

    Serial.println(
      "Check VCC, GND, SDA, and SCL wiring."
    );


    while (1) {

      setBothStrips(
        leftStrip.Color(
          255,
          0,
          0
        ),

        rightStrip.Color(
          255,
          0,
          0
        ),

        50,
        50
      );

      delay(300);

      allOff();

      delay(300);
    }
  }


  Serial.println(
    "MPU6050 found."
  );


  mpu.setAccelerometerRange(
    MPU6050_RANGE_4_G
  );

  mpu.setGyroRange(
    MPU6050_RANGE_500_DEG
  );

  mpu.setFilterBandwidth(
    MPU6050_BAND_21_HZ
  );


  calibrateMPU6050();


  Serial.println(
    "Setup complete."
  );
}


// ==================================================
// MAIN LOOP
// ==================================================

void loop() {

  // Process any encoder movement captured by
  // the interrupt handler.

  processEncoderRotation();


  // Handle encoder push button.

  readEncoderButton();


  // Update accelerometer and LEDs.

  updateLEDs();
}