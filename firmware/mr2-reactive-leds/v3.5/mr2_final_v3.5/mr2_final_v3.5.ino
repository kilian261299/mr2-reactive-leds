#include <Wire.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

/*
  ==================================================
  MR2 Reactive LEDs — Final Firmware V3.5
  ==================================================

  Hardware used:

  - ESP32-C3 Super Mini
  - MPU6050 / GY-521 accelerometer
  - KY-040 rotary encoder
  - Two WS2812B LED strips
  - SN74AHCT125N level shifter
  - 12V to 5V converter

  --------------------------------------------------
  WHAT'S NEW IN V3.5
  --------------------------------------------------

  V3.5 focuses on refining the reactive lighting behaviour
  following bench testing of V3.4.

  The main improvements are:

  - Improved blue-to-orange acceleration colour transition.
  - Reduced unwanted colour washout during acceleration.
  - Added accelerometer low-pass filtering.
  - Increased acceleration dead zone.
  - Refined braking threshold.
  - Increased cornering threshold.
  - Added movement hysteresis to reduce flickering.
  - Smoothed reactive brightness changes.
  - Reduced sensitivity to small movements and sensor noise.
  - Maintained hill compensation from previous versions.
  - Maintained V3.4 rotary encoder responsiveness improvements.

  The aim is to make the system more suitable for real vehicle use.

  Small movements, road vibration and sensor noise should produce
  little or no visible reaction.

  Genuine acceleration, braking and cornering should still produce
  clear and responsive lighting effects.

  --------------------------------------------------
  V3.5 REACTIVE RESPONSE
  --------------------------------------------------

  The accelerometer signal now passes through a low-pass filter
  before being used for reactive lighting.

  This reduces rapid fluctuations caused by:

  - Road vibration
  - Suspension movement
  - Sensor noise
  - Small vehicle movements

  Acceleration and braking also use configurable dead zones.

  This prevents tiny accelerometer readings from immediately
  changing the LED brightness.

  Cornering also has a configurable threshold to reduce unwanted
  left/right brightness changes during small movements.

  Reactive brightness is smoothed separately so LED brightness
  changes gradually rather than jumping between values.

  --------------------------------------------------
  BLUE TO ORANGE TRANSITION
  --------------------------------------------------

  The acceleration colour transition was refined in V3.5.

  The previous direct RGB interpolation could produce a washed-out
  purple or white appearance during the transition.

  V3.5 uses a multi-stage colour transition:

    Blue
      ↓
    Deep blue-violet
      ↓
    Purple-red
      ↓
    Red-orange
      ↓
    Orange

  This produces a more visually controlled transition while
  maintaining the original blue-to-orange concept.

  --------------------------------------------------
  WHAT'S IN HILL COMPENSATION
  --------------------------------------------------

  A slowly-drifting accelerometer baseline is used to compensate
  for sustained vehicle pitch changes.

  This helps prevent false acceleration or braking effects when
  driving on hills.

  The baseline slowly follows sustained changes while remaining
  responsive to genuine short-term acceleration and braking.

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
  MODES
  --------------------------------------------------

  Mode 0 — Reactive

    Calm / idle:
      Dim blue, slowly breathing.

    Acceleration:
      Blue transitions through violet/red tones toward orange
      while brightness increases.

    Braking:
      Red glow, stronger braking = brighter red.

    Cornering:
      One side becomes brighter than the other.

    Acceleration around a corner:
      Orange/brighter + left/right brightness shift.

    Braking around a corner:
      Red + left/right brightness shift.

  --------------------------------------------------

  Mode 1 — Purple, themed reactive
  Mode 2 — Red, themed reactive
  Mode 3 — Blue, themed reactive
  Mode 4 — Green, themed reactive

    Colour stays fixed, but brightness reacts to overall movement
    and cornering.

    When calm, the brightness gently breathes.

  --------------------------------------------------
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
// REACTIVE RESPONSE SETTINGS
// ==================================================

// --------------------------------------------------
// ACCELERATION
// --------------------------------------------------

// Acceleration below this value is ignored.
//
// Increase this value if small throttle changes or road
// vibration cause unwanted acceleration lighting.
//
// Decrease this value if the system feels unresponsive.
const float accelerationDeadZone = 0.08;

// Acceleration value at which the acceleration effect
// reaches maximum intensity.
const float accelerationFullScale = 0.35;


// --------------------------------------------------
// BRAKING
// --------------------------------------------------

// Braking must exceed this negative value before
// the braking effect is activated.
//
// Example:
// -0.05g = ignored
// -0.10g = begins reacting
// -0.40g = maximum braking intensity
const float brakingDeadZone = 0.12;

const float brakingFullScale = 0.40;


// --------------------------------------------------
// CORNERING
// --------------------------------------------------

// Sideways acceleration below this value is ignored.
//
// This helps prevent small road movements from constantly
// shifting brightness between the left and right strips.
const float corneringDeadZone = 0.10;

// Side acceleration at which maximum cornering effect
// is reached.
const float corneringFullScale = 0.35;


// --------------------------------------------------
// ACCELEROMETER FILTERING
// --------------------------------------------------

// Lower value = smoother and less sensitive to vibration.
//
// Higher value = faster response but more sensitive to noise.
//
// 0.15 is a moderate starting point for vehicle testing.
const float accelerometerFilterRate = 0.15;


// --------------------------------------------------
// LED BRIGHTNESS SMOOTHING
// --------------------------------------------------

// Controls how quickly reactive brightness follows
// the calculated target brightness.
//
// Lower = smoother/slower.
// Higher = faster/more responsive.
const float brightnessSmoothingRate = 0.20;


// --------------------------------------------------
// MOVEMENT HYSTERESIS
// --------------------------------------------------

// Prevents the reactive system from rapidly switching
// between "movement" and "calm" around the threshold.

const float movementStartThreshold = 0.08;
const float movementStopThreshold  = 0.05;

bool movementActive = false;


// ==================================================
// FILTERED ACCELEROMETER VALUES
// ==================================================

float filteredForwardG = 0.0;
float filteredSideG = 0.0;
float filteredMovementG = 0.0;


// ==================================================
// SMOOTHED BRIGHTNESS VALUES
// ==================================================

float smoothedReactiveBrightness = 0.0;


// ==================================================
// ENCODER / BUTTON VARIABLES
// ==================================================

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

// V3.5 improved blue-to-orange transition.
//
// The transition uses several controlled colour stages
// rather than direct RGB interpolation.
//
// This prevents the colour from becoming washed out
// or appearing white during acceleration.

uint32_t blueToOrange(
  float amount,
  Adafruit_NeoPixel &strip
) {

  amount = constrain(amount, 0.0, 1.0);

  int r;
  int g;
  int b;

  if (amount < 0.25) {

    // Blue → Deep blue-violet

    float t = amount / 0.25;

    r = 0 + (40 * t);
    g = 0;
    b = 255 - (35 * t);

  } else if (amount < 0.50) {

    // Deep blue-violet → Purple-red

    float t = (amount - 0.25) / 0.25;

    r = 40 + (130 - 40) * t;
    g = 0;
    b = 220 - (80 * t);

  } else if (amount < 0.75) {

    // Purple-red → Red-orange

    float t = (amount - 0.50) / 0.25;

    r = 130 + (220 - 130) * t;
    g = 0 + (20 * t);
    b = 140 - (90 * t);

  } else {

    // Red-orange → Orange

    float t = (amount - 0.75) / 0.25;

    r = 220 + (255 - 220) * t;
    g = 20 + (40 - 20) * t;
    b = 50 - (50 * t);

  }

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

      int distanceBehindHead =
        head - i;

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

        leftStrip.setPixelColor(
          i,
          colour
        );

        rightStrip.setPixelColor(
          i,
          colour
        );

      } else {

        leftStrip.setPixelColor(
          i,
          0
        );

        rightStrip.setPixelColor(
          i,
          0
        );

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


  // --------------------------------------------------
  // UPDATE HILL COMPENSATION BASELINE
  // --------------------------------------------------

  updateDriftingBaseline(
    accel.acceleration.x,
    accel.acceleration.y,
    accel.acceleration.z
  );


  // --------------------------------------------------
  // CONVERT TO G
  // --------------------------------------------------

  xG =
    (accel.acceleration.x - driftBaseX)
    / 9.81;

  yG =
    (accel.acceleration.y - driftBaseY)
    / 9.81;

  zG =
    (accel.acceleration.z - driftBaseZ)
    / 9.81;


  // --------------------------------------------------
  // SELECT VEHICLE AXES
  // --------------------------------------------------

  float rawForwardG =
    getSelectedAxis(
      xG,
      yG,
      zG,
      FORWARD_AXIS
    ) * FORWARD_SIGN;

  float rawSideG =
    getSelectedAxis(
      xG,
      yG,
      zG,
      SIDE_AXIS
    ) * SIDE_SIGN;


  // --------------------------------------------------
  // ACCELEROMETER LOW-PASS FILTER
  // --------------------------------------------------

  filteredForwardG =
    filteredForwardG *
    (1.0 - accelerometerFilterRate)
    +
    rawForwardG *
    accelerometerFilterRate;


  filteredSideG =
    filteredSideG *
    (1.0 - accelerometerFilterRate)
    +
    rawSideG *
    accelerometerFilterRate;


  // Calculate overall movement from the filtered
  // forward and side acceleration values.

  filteredMovementG =
    sqrt(
      (filteredForwardG * filteredForwardG)
      +
      (filteredSideG * filteredSideG)
    );


  // Return the filtered values.

  xG = filteredForwardG;

  yG = filteredSideG;

  zG = 0.0;

  movementG =
    filteredMovementG;

}


// ==================================================
// MPU6050 CALIBRATION
// ==================================================

void calibrateMPU6050() {

  Serial.println(
    "Calibrating MPU6050..."
  );

  Serial.println(
    "Keep the car/box still."
  );


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


    totalX +=
      accel.acceleration.x;

    totalY +=
      accel.acceleration.y;

    totalZ +=
      accel.acceleration.z;


    delay(10);

  }


  baseX =
    totalX / samples;

  baseY =
    totalY / samples;

  baseZ =
    totalZ / samples;


  driftBaseX =
    baseX;

  driftBaseY =
    baseY;

  driftBaseZ =
    baseZ;


  // Reset filtering after calibration.

  filteredForwardG = 0.0;

  filteredSideG = 0.0;

  filteredMovementG = 0.0;

  smoothedReactiveBrightness =
    userBrightness * 0.25;

  movementActive =
    false;


  Serial.println(
    "Calibration complete."
  );


  Serial.print(
    "Base X: "
  );

  Serial.println(
    baseX
  );


  Serial.print(
    "Base Y: "
  );

  Serial.println(
    baseY
  );


  Serial.print(
    "Base Z: "
  );

  Serial.println(
    baseZ
  );


  flashConfirmation(
    leftStrip.Color(
      0,
      255,
      0
    )
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


  // Remove small sideways movements.

  if (
    abs(sideG)
    < corneringDeadZone
  ) {

    sideG = 0.0;

  }


  float sideIntensity =
    abs(sideG)
    / corneringFullScale;


  sideIntensity =
    constrain(
      sideIntensity,
      0.0,
      1.0
    );


  leftBrightness =
    baseBrightness;

  rightBrightness =
    baseBrightness;


  if (
    sideG
    > corneringDeadZone
  ) {

    leftBrightness =
      baseBrightness
      +
      (
        userBrightness
        * 0.4
        * sideIntensity
      );


    rightBrightness =
      baseBrightness
      *
      (
        1.0
        -
        0.4
        * sideIntensity
      );

  }


  else if (
    sideG
    < -corneringDeadZone
  ) {

    rightBrightness =
      baseBrightness
      +
      (
        userBrightness
        * 0.4
        * sideIntensity
      );


    leftBrightness =
      baseBrightness
      *
      (
        1.0
        -
        0.4
        * sideIntensity
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
    (
      millis()
      % 4000
    )
    /
    4000.0
    *
    2.0
    *
    PI;


  float wave =
    (
      sin(phase)
      +
      1.0
    )
    /
    2.0;


  return
    0.6
    +
    (
      0.4
      * wave
    );

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
    userBrightness
    * 0.25
    * breathingMultiplier();


  float intensity =
    movementG
    /
    0.50;


  intensity =
    constrain(
      intensity,
      0.0,
      1.0
    );


  int themeBrightness =
    ambientBrightness
    +
    (
      (
        userBrightness
        -
        ambientBrightness
      )
      *
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

    leftStrip.Color(
      r,
      g,
      b
    ),

    rightStrip.Color(
      r,
      g,
      b
    ),

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


  // --------------------------------------------------
  // MOVEMENT HYSTERESIS
  // --------------------------------------------------

  if (!movementActive) {

    if (
      movementG
      >
      movementStartThreshold
    ) {

      movementActive =
        true;

    }

  }

  else {

    if (
      movementG
      <
      movementStopThreshold
    ) {

      movementActive =
        false;

    }

  }


  // --------------------------------------------------
  // AMBIENT BRIGHTNESS
  // --------------------------------------------------

  int ambientBrightness =
    userBrightness
    * 0.25
    * breathingMultiplier();


  // --------------------------------------------------
  // BRAKING
  // --------------------------------------------------

  if (
    forwardG
    <
    -brakingDeadZone
  ) {


    float brakeIntensity =
      (
        abs(forwardG)
        -
        brakingDeadZone
      )
      /
      (
        brakingFullScale
        -
        brakingDeadZone
      );


    brakeIntensity =
      constrain(
        brakeIntensity,
        0.0,
        1.0
      );


    int targetBrightness =
      ambientBrightness
      +
      (
        (
          userBrightness
          -
          ambientBrightness
        )
        *
        brakeIntensity
      );


    targetBrightness =
      constrain(
        targetBrightness,
        0,
        userBrightness
      );


    // Smooth the brightness response.

    smoothedReactiveBrightness =
      smoothedReactiveBrightness
      +
      (
        targetBrightness
        -
        smoothedReactiveBrightness
      )
      *
      brightnessSmoothingRate;


    int leftBrightness;

    int rightBrightness;


    applyCorneringBrightness(
      smoothedReactiveBrightness,
      sideG,
      leftBrightness,
      rightBrightness
    );


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

      leftBrightness,

      rightBrightness

    );


    return;

  }


  // --------------------------------------------------
  // ACCELERATION
  // --------------------------------------------------

  float accelIntensity =
    0.0;


  if (
    movementActive
    &&
    forwardG
    >
    accelerationDeadZone
  ) {


    float effectiveAcceleration =
      forwardG
      -
      accelerationDeadZone;


    accelIntensity =
      effectiveAcceleration
      /
      (
        accelerationFullScale
        -
        accelerationDeadZone
      );


    accelIntensity =
      constrain(
        accelIntensity,
        0.0,
        1.0
      );

  }


  // --------------------------------------------------
  // REACTIVE BRIGHTNESS
  // --------------------------------------------------

  int targetBrightness =
    ambientBrightness
    +
    (
      (
        userBrightness
        -
        ambientBrightness
      )
      *
      accelIntensity
    );


  targetBrightness =
    constrain(
      targetBrightness,
      0,
      userBrightness
    );


  // Smooth brightness.

  smoothedReactiveBrightness =
    smoothedReactiveBrightness
    +
    (
      targetBrightness
      -
      smoothedReactiveBrightness
    )
    *
    brightnessSmoothingRate;


  // --------------------------------------------------
  // ACCELERATION COLOUR
  // --------------------------------------------------

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


  // --------------------------------------------------
  // CORNERING
  // --------------------------------------------------

  int leftBrightness;

  int rightBrightness;


  applyCorneringBrightness(

    smoothedReactiveBrightness,

    sideG,

    leftBrightness,

    rightBrightness

  );


  // --------------------------------------------------
  // OUTPUT
  // --------------------------------------------------

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
    xG;


  float sideG =
    yG;


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


  // --------------------------------------------------
  // SERIAL DEBUG
  // --------------------------------------------------

  if (
    millis()
    -
    lastSerialPrint
    >
    serialPrintInterval
  ) {


    Serial.print(
      "Mode: "
    );

    Serial.print(
      mode
    );


    Serial.print(
      " | Brightness max: "
    );

    Serial.print(
      userBrightness
    );


    Serial.print(
      " | Forward: "
    );

    Serial.print(
      forwardG,
      3
    );


    Serial.print(
      "g | Side: "
    );

    Serial.print(
      sideG,
      3
    );


    Serial.print(
      "g | Movement: "
    );

    Serial.print(
      movementG,
      3
    );


    Serial.print(
      "g | Movement active: "
    );

    Serial.println(
      movementActive
    );


    lastSerialPrint =
      millis();

  }

}


// ==================================================
// ENCODER HANDLING
// ==================================================


// Full quadrature state-transition decoding.
//
// This version retains the V3.4 encoder implementation.

const int8_t encoderTransitionTable[16] = {

   0, -1,  1,  0,

   1,  0,  0, -1,

  -1,  0,  0,  1,

   0,  1, -1,  0

};


uint8_t encoderState =
  0;


int8_t encoderAccumulatedSteps =
  0;


// Most KY-040 encoders produce 4 valid quarter-steps
// per physical detent.
//
// V3.4 used 2 to provide a practical balance between
// responsiveness and control resolution.

const int8_t stepsPerDetent =
  2;


void readEncoderRotation() {


  int clk =
    digitalRead(
      ENCODER_CLK
    );


  int dt =
    digitalRead(
      ENCODER_DT
    );


  uint8_t newState =
    (
      clk << 1
    )
    |
    dt;


  encoderState =
    (
      (
        encoderState
        << 2
      )
      |
      newState
    )
    &
    0x0F;


  int8_t movement =
    encoderTransitionTable[
      encoderState
    ];


  if (
    movement
    !=
    0
  ) {


    encoderAccumulatedSteps
      +=
      movement;


    if (
      encoderAccumulatedSteps
      >=
      stepsPerDetent
    ) {


      userBrightness
        +=
        5;


      encoderAccumulatedSteps =
        0;


      userBrightness =
        constrain(

          userBrightness,

          minBrightness,

          maxBrightness

        );


      Serial.print(
        "Brightness max set to: "
      );


      Serial.println(
        userBrightness
      );

    }


    else if (
      encoderAccumulatedSteps
      <=
      -stepsPerDetent
    ) {


      userBrightness
        -=
        5;


      encoderAccumulatedSteps =
        0;


      userBrightness =
        constrain(

          userBrightness,

          minBrightness,

          maxBrightness

        );


      Serial.print(
        "Brightness max set to: "
      );


      Serial.println(
        userBrightness
      );

    }

  }

}


// ==================================================
// ENCODER BUTTON
// ==================================================

void readEncoderButton() {


  bool buttonDown =
    digitalRead(
      ENCODER_SW
    )
    ==
    LOW;


  unsigned long now =
    millis();


  if (
    buttonDown
    &&
    !buttonWasDown
  ) {


    buttonWasDown =
      true;


    longPressHandled =
      false;


    buttonDownTime =
      now;

  }


  if (
    buttonDown
    &&
    !longPressHandled
  ) {


    if (
      now
      -
      buttonDownTime
      >
      longPressTime
    ) {


      longPressHandled =
        true;


      calibrateMPU6050();

    }

  }


  if (
    !buttonDown
    &&
    buttonWasDown
  ) {


    buttonWasDown =
      false;


    if (
      !longPressHandled
      &&
      now
      -
      lastButtonEvent
      >
      debounceDelay
    ) {


      mode++;


      if (
        mode
        >=
        numberOfModes
      ) {

        mode =
          0;

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


  Serial.begin(
    115200
  );


  delay(
    500
  );


  Serial.println(
    "MR2 Reactive LEDs — Final Firmware V3.5"
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


  Serial.println(
    "Reactive response refinement: ENABLED"
  );


  Serial.print(
    "Acceleration dead zone: "
  );


  Serial.println(
    accelerationDeadZone
  );


  Serial.print(
    "Braking dead zone: "
  );


  Serial.println(
    brakingDeadZone
  );


  Serial.print(
    "Cornering dead zone: "
  );


  Serial.println(
    corneringDeadZone
  );


  // --------------------------------------------------
  // ENCODER SETUP
  // --------------------------------------------------

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


  // Seed quadrature state.

  encoderState =
    (
      digitalRead(
        ENCODER_CLK
      )
      << 1
    )
    |
    digitalRead(
      ENCODER_DT
    );


  // --------------------------------------------------
  // LED SETUP
  // --------------------------------------------------

  leftStrip.begin();

  rightStrip.begin();


  leftStrip.clear();

  rightStrip.clear();


  leftStrip.show();

  rightStrip.show();


  // --------------------------------------------------
  // STARTUP ANIMATION
  // --------------------------------------------------

  startupSweep();


  // --------------------------------------------------
  // MPU6050 SETUP
  // --------------------------------------------------

  Wire.begin(

    I2C_SDA,

    I2C_SCL

  );


  if (
    !mpu.begin()
  ) {


    Serial.println(
      "ERROR: MPU6050 not found."
    );


    Serial.println(
      "Check VCC, GND, SDA, and SCL wiring."
    );


    while (
      1
    ) {


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


      delay(
        300
      );


      allOff();


      delay(
        300
      );

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


  // --------------------------------------------------
  // INITIAL CALIBRATION
  // --------------------------------------------------

  calibrateMPU6050();


  Serial.println(
    "Setup complete."
  );

}


// ==================================================
// MAIN LOOP
// ==================================================

void loop() {


  // Encoder is handled first so brightness and
  // button input remain responsive.

  readEncoderRotation();


  readEncoderButton();


  // Update accelerometer and LED behaviour.

  updateLEDs();

}