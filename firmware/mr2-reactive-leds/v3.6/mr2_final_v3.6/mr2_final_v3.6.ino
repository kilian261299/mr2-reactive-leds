#include <Wire.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

/*
  ==================================================
  MR2 REACTIVE LEDs — FINAL FIRMWARE V3.6
  ==================================================

  Hardware:

  - ESP32-C3 Super Mini
  - MPU6050 / GY-521
  - KY-040 rotary encoder
  - Two WS2812B LED strips
  - SN74AHCT125N level shifter
  - 12V to 5V buck converter


  ==================================================
  V3.6 FINAL FEATURES
  ==================================================

  - Smart hill compensation
  - Slow gravity / pitch baseline tracking
  - Fast dynamic movement detection
  - Baseline freezes during acceleration
  - Baseline freezes during braking
  - Baseline freezes during cornering
  - Baseline freezes during sustained dynamic movement
  - Dynamic movement must stop before baseline can adapt
  - Settling delay after dynamic movement
  - Slow baseline adaptation
  - Hysteresis prevents state chatter
  - Accelerometer low-pass filtering
  - Acceleration dead zone
  - Braking dead zone
  - Cornering dead zone
  - Progressive acceleration response
  - Progressive braking response
  - Progressive cornering response
  - Blue -> violet-blue -> orange acceleration colour
  - Red braking
  - Left/right cornering brightness bias
  - Five lighting modes
  - Rotary encoder brightness control
  - Short press changes mode
  - Long press recalibrates MPU6050
  - Startup sweep
  - Calibration confirmation flash


  ==================================================
  V3.6 SMART HILL COMPENSATION
  ==================================================

  The MPU6050 measures gravity and dynamic acceleration.

  A vehicle travelling up or down a hill changes the
  orientation of the gravity vector relative to the
  sensor.

  This can appear as forward or backward acceleration.

  V3.6 therefore separates:

    FAST component
      Used to detect real dynamic acceleration,
      braking and cornering.

    SLOW component
      Used to estimate long-term gravity / hill
      orientation.

  The slow baseline is only allowed to adapt when
  the vehicle is considered stable.

  During dynamic movement:

    Baseline FREEZES.

  After dynamic movement:

    SETTLING period begins.

  After settling:

    Baseline slowly adapts.

  This prevents the system from immediately learning
  genuine acceleration as a new baseline.


  ==================================================
  BASELINE STATES
  ==================================================

  STABLE

    Vehicle is calm.
    Slow baseline may adapt.

  DYNAMIC

    Acceleration, braking or cornering detected.
    Baseline is frozen.

  SETTLING

    Dynamic movement has stopped.
    Baseline remains frozen temporarily.

    This prevents the end of an acceleration event
    from immediately becoming the new baseline.


  ==================================================
  MODES
  ==================================================

  Mode 0 — Main Reactive

    Calm:
      Dim blue breathing.

    Acceleration:
      Blue -> violet-blue -> orange.

    Braking:
      Red.

    Cornering:
      Left/right brightness shift.


  Mode 1 — Purple

  Mode 2 — Red

  Mode 3 — Blue

  Mode 4 — Green


  ==================================================
  ENCODER
  ==================================================

  Rotate:
    Adjust maximum brightness.

  Short press:
    Change mode.

  Long press:
    Recalibrate accelerometer.


  ==================================================
  INSTALLATION
  ==================================================

  1. Mount MPU6050 securely.
  2. Keep sensor orientation consistent.
  3. Turn car on.
  4. Keep car stationary during calibration.
  5. Long press encoder to recalibrate if required.


  ==================================================
  POWER
  ==================================================

  - LED strips powered from 5V converter.
  - ESP32 powered appropriately.
  - All grounds common.
  - Use fuse on 12V input.
  - Do not power LED strips from ESP32.
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
// MPU6050
// ==================================================

Adafruit_MPU6050 mpu;


// ==================================================
// HILL COMPENSATION
// ==================================================

#define HILL_COMPENSATION true


// ==================================================
// AXIS CONFIGURATION
// ==================================================

#define AXIS_X 0
#define AXIS_Y 1
#define AXIS_Z 2


// Forward acceleration axis.

const int FORWARD_AXIS = AXIS_X;

const int FORWARD_SIGN = -1;


// Sideways acceleration axis.

const int SIDE_AXIS = AXIS_Y;

const int SIDE_SIGN = -1;


// ==================================================
// CALIBRATION BASELINE
// ==================================================

float baseX = 0.0;
float baseY = 0.0;
float baseZ = 0.0;


// ==================================================
// SMART BASELINE
// ==================================================

// The baseline represents the slowly changing gravity
// vector caused by vehicle pitch / hill angle.

float driftBaseX = 0.0;
float driftBaseY = 0.0;
float driftBaseZ = 0.0;


// ==================================================
// BASELINE ADAPTATION
// ==================================================

// Very slow adaptation.
//
// Lower = stronger protection against absorbing
// genuine acceleration.
//
// Higher = faster hill adaptation.

const float baselineDriftRate = 0.0008;


// ==================================================
// DYNAMIC MOVEMENT THRESHOLDS
// ==================================================

// Acceleration threshold.

const float baselineAccelerationThreshold = 0.08;


// Braking threshold.

const float baselineBrakingThreshold = 0.10;


// Cornering threshold.

const float baselineCorneringThreshold = 0.08;


// ==================================================
// STABILITY THRESHOLDS
// ==================================================

// Dynamic movement must be below this level before
// the system can eventually become stable.

const float baselineStableThreshold = 0.045;


// Hysteresis threshold.
//
// Once stable, movement must exceed this level to
// return to dynamic behaviour.

const float baselineDynamicReentryThreshold = 0.075;


// ==================================================
// SETTLING
// ==================================================

const unsigned long baselineSettleTime = 2000;


// ==================================================
// BASELINE STATE
// ==================================================

enum BaselineState {

  BASELINE_STABLE,

  BASELINE_DYNAMIC,

  BASELINE_SETTLING
};


BaselineState baselineState =
  BASELINE_STABLE;


// ==================================================
// BASELINE TIMING
// ==================================================

unsigned long dynamicMovementEndedTime = 0;


// ==================================================
// ACCELEROMETER FILTERING
// ==================================================

// Main reactive smoothing.

const float accelerationSmoothing = 0.15;


// Slow gravity / hill tracking.
//
// This must be significantly slower than the main
// reactive filter.

const float gravitySmoothing = 0.008;


// ==================================================
// FILTERED VALUES
// ==================================================

float smoothedForwardG = 0.0;

float smoothedSideG = 0.0;

float smoothedMovementG = 0.0;


// ==================================================
// GRAVITY ESTIMATE
// ==================================================

float gravityX = 0.0;

float gravityY = 0.0;

float gravityZ = 0.0;


// ==================================================
// USER SETTINGS
// ==================================================

int userBrightness = 80;

const int minBrightness = 0;

const int maxBrightness = 220;

int mode = 0;

const int numberOfModes = 5;


// ==================================================
// ACCELEROMETER RESPONSE
// ==================================================

// Acceleration dead zone.

const float accelerationDeadZone = 0.04;


// Braking dead zone.

const float brakingDeadZone = 0.12;


// Cornering dead zone.

const float corneringDeadZone = 0.08;


// ==================================================
// RESPONSE RANGES
// ==================================================

const float accelerationResponseG = 0.35;

const float brakingResponseG = 0.40;

const float corneringResponseG = 0.35;


// ==================================================
// ENCODER
// ==================================================

const int8_t encoderTransitionTable[16] = {

   0, -1,  1,  0,

   1,  0,  0, -1,

  -1,  0,  0,  1,

   0,  1, -1,  0
};


volatile uint8_t encoderState = 0;

volatile int32_t encoderAccumulatedSteps = 0;


const int8_t stepsPerDetent = 2;

const int brightnessStep = 5;


// ==================================================
// BUTTON
// ==================================================

bool buttonWasDown = false;

bool longPressHandled = false;

unsigned long buttonDownTime = 0;

unsigned long lastButtonEvent = 0;

const unsigned long debounceDelay = 80;

const unsigned long longPressTime = 1200;


// ==================================================
// SERIAL DEBUG
// ==================================================

unsigned long lastSerialPrint = 0;

const unsigned long serialPrintInterval = 300;


// ==================================================
// AXIS HELPER
// ==================================================

float getSelectedAxis(
  float xG,
  float yG,
  float zG,
  int axis
) {

  if (
    axis ==
    AXIS_X
  ) {

    return xG;
  }


  if (
    axis ==
    AXIS_Y
  ) {

    return yG;
  }


  return zG;
}


// ==================================================
// SMART BASELINE UPDATE
// ==================================================

void updateSmartBaseline(
  float rawX,
  float rawY,
  float rawZ,
  float forwardG,
  float sideG,
  float movementG
) {

#if HILL_COMPENSATION

  unsigned long now =
    millis();


  // =================================================
  // DETECT DYNAMIC MOVEMENT
  // =================================================

  bool accelerating =
    forwardG >
    baselineAccelerationThreshold;


  bool braking =
    forwardG <
    -baselineBrakingThreshold;


  bool cornering =
    abs(sideG) >
    baselineCorneringThreshold;


  bool dynamicMovement =
    accelerating ||
    braking ||
    cornering;


  // =================================================
  // DYNAMIC STATE
  // =================================================

  if (
    dynamicMovement
  ) {

    baselineState =
      BASELINE_DYNAMIC;


    dynamicMovementEndedTime =
      now;


    return;
  }


  // =================================================
  // DYNAMIC -> SETTLING
  // =================================================

  if (
    baselineState ==
    BASELINE_DYNAMIC
  ) {

    baselineState =
      BASELINE_SETTLING;


    dynamicMovementEndedTime =
      now;


    return;
  }


  // =================================================
  // SETTLING STATE
  // =================================================

  if (
    baselineState ==
    BASELINE_SETTLING
  ) {

    // If meaningful movement starts again,
    // immediately return to dynamic.

    if (
      movementG >
      baselineDynamicReentryThreshold
    ) {

      baselineState =
        BASELINE_DYNAMIC;


      dynamicMovementEndedTime =
        now;


      return;
    }


    // If the vehicle has not remained sufficiently
    // calm, restart settling.

    if (
      movementG >
      baselineStableThreshold
    ) {

      dynamicMovementEndedTime =
        now;


      return;
    }


    // Wait for the complete settling period.

    if (
      now -
      dynamicMovementEndedTime <
      baselineSettleTime
    ) {

      return;
    }


    // Settling complete.

    baselineState =
      BASELINE_STABLE;
  }


  // =================================================
  // STABLE STATE
  // =================================================

  if (
    baselineState ==
    BASELINE_STABLE
  ) {

    // Only adapt when the vehicle is very calm.

    if (
      movementG <
      baselineStableThreshold
    ) {

      driftBaseX =
        driftBaseX *
        (
          1.0 -
          baselineDriftRate
        )
        +
        rawX *
        baselineDriftRate;


      driftBaseY =
        driftBaseY *
        (
          1.0 -
          baselineDriftRate
        )
        +
        rawY *
        baselineDriftRate;


      driftBaseZ =
        driftBaseZ *
        (
          1.0 -
          baselineDriftRate
        )
        +
        rawZ *
        baselineDriftRate;
    }
  }

#endif
}


// ==================================================
// COLOUR — BLUE TO ORANGE
// ==================================================

uint32_t blueToOrange(
  float amount,
  Adafruit_NeoPixel &strip
) {

  amount =
    constrain(
      amount,
      0.0,
      1.0
    );


  int r;

  int g;

  int b;


  // =================================================
  // BLUE -> CONTROLLED VIOLET-BLUE
  // =================================================

  if (
    amount <
    0.5
  ) {

    float t =
      amount /
      0.5;


    const int startR = 0;

    const int startG = 0;

    const int startB = 255;


    const int midR = 80;

    const int midG = 0;

    const int midB = 220;


    r =
      startR +
      (
        midR -
        startR
      ) *
      t;


    g =
      startG +
      (
        midG -
        startG
      ) *
      t;


    b =
      startB +
      (
        midB -
        startB
      ) *
      t;
  }


  // =================================================
  // VIOLET-BLUE -> ORANGE
  // =================================================

  else {

    float t =
      (
        amount -
        0.5
      )
      /
      0.5;


    const int startR = 80;

    const int startG = 0;

    const int startB = 220;


    const int endR = 255;

    const int endG = 40;

    const int endB = 0;


    r =
      startR +
      (
        endR -
        startR
      ) *
      t;


    g =
      startG +
      (
        endG -
        startG
      ) *
      t;


    b =
      startB +
      (
        endB -
        startB
      ) *
      t;
  }


  return strip.Color(
    r,
    g,
    b
  );
}


// ==================================================
// LED OUTPUT
// ==================================================

void setStrip(
  Adafruit_NeoPixel &strip,
  int ledCount,
  uint32_t colour,
  float brightness
) {

  brightness =
    constrain(
      brightness,
      0.0f,
      255.0f
    );


  int origR =
    (
      colour >>
      16
    )
    &
    0xFF;


  int origG =
    (
      colour >>
      8
    )
    &
    0xFF;


  int origB =
    colour
    &
    0xFF;


  int r =
    (
      int
    )
    round(
      (
        origR *
        brightness
      )
      /
      255.0f
    );


  int g =
    (
      int
    )
    round(
      (
        origG *
        brightness
      )
      /
      255.0f
    );


  int b =
    (
      int
    )
    round(
      (
        origB *
        brightness
      )
      /
      255.0f
    );


  r =
    constrain(
      r,
      0,
      255
    );


  g =
    constrain(
      g,
      0,
      255
    );


  b =
    constrain(
      b,
      0,
      255
    );


  uint32_t scaledColour =
    strip.Color(
      r,
      g,
      b
    );


  for (
    int i = 0;
    i < ledCount;
    i++
  ) {

    strip.setPixelColor(
      i,
      scaledColour
    );
  }


  strip.show();
}


// ==================================================
// BOTH STRIPS
// ==================================================

void setBothStrips(
  uint32_t leftColour,
  uint32_t rightColour,
  float leftBrightness,
  float rightBrightness
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


// ==================================================
// ALL OFF
// ==================================================

void allOff() {

  setBothStrips(
    leftStrip.Color(
      0,
      0,
      0
    ),

    rightStrip.Color(
      0,
      0,
      0
    ),

    0,
    0
  );
}


// ==================================================
// CONFIRMATION FLASH
// ==================================================

void flashConfirmation(
  uint32_t colour
) {

  for (
    int i = 0;
    i < 2;
    i++
  ) {

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
// STARTUP SWEEP
// ==================================================

void startupSweep() {

  const int sweepR = 0;

  const int sweepG = 120;

  const int sweepB = 255;


  const int tailLength = 14;

  const int sweepDelay = 3;


  for (
    int head = 0;
    head <
    NUM_LEDS_LEFT +
    tailLength;
    head++
  ) {

    for (
      int i = 0;
      i < NUM_LEDS_LEFT;
      i++
    ) {

      int distanceBehindHead =
        head -
        i;


      if (
        distanceBehindHead >= 0 &&
        distanceBehindHead <
        tailLength
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
            (
              sweepR *
              fade *
              userBrightness
            )
            /
            65025,

            (
              sweepG *
              fade *
              userBrightness
            )
            /
            65025,

            (
              sweepB *
              fade *
              userBrightness
            )
            /
            65025
          );


        leftStrip.setPixelColor(
          i,
          colour
        );


        rightStrip.setPixelColor(
          i,
          colour
        );
      }


      else {

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


    delay(
      sweepDelay
    );
  }


  allOff();


  delay(200);
}


// ==================================================
// READ ACCELEROMETER
// ==================================================

void readAcceleration() {

  sensors_event_t accel;

  sensors_event_t gyro;

  sensors_event_t temp;


  mpu.getEvent(
    &accel,
    &gyro,
    &temp
  );


  // =================================================
  // RAW ACCELERATION
  // =================================================

  float rawX =
    accel.acceleration.x;


  float rawY =
    accel.acceleration.y;


  float rawZ =
    accel.acceleration.z;


  // =================================================
  // INITIALISE GRAVITY ESTIMATE
  // =================================================

  if (
    gravityX == 0.0 &&
    gravityY == 0.0 &&
    gravityZ == 0.0
  ) {

    gravityX =
      rawX;


    gravityY =
      rawY;


    gravityZ =
      rawZ;
  }


  // =================================================
  // SLOW GRAVITY / HILL FILTER
  // =================================================

  gravityX =
    gravityX *
    (
      1.0 -
      gravitySmoothing
    )
    +
    rawX *
    gravitySmoothing;


  gravityY =
    gravityY *
    (
      1.0 -
      gravitySmoothing
    )
    +
    rawY *
    gravitySmoothing;


  gravityZ =
    gravityZ *
    (
      1.0 -
      gravitySmoothing
    )
    +
    rawZ *
    gravitySmoothing;


  // =================================================
  // DYNAMIC COMPONENT
  // =================================================

  float dynamicX =
    rawX -
    gravityX;


  float dynamicY =
    rawY -
    gravityY;


  float dynamicZ =
    rawZ -
    gravityZ;


  // =================================================
  // UPDATE SMART BASELINE
  // =================================================

  updateSmartBaseline(
    rawX,
    rawY,
    rawZ,
    smoothedForwardG,
    smoothedSideG,
    smoothedMovementG
  );


  // =================================================
  // BASELINE-CORRECTED VALUES
  // =================================================

  float currentXG =
    (
      rawX -
      driftBaseX
    )
    /
    9.81;


  float currentYG =
    (
      rawY -
      driftBaseY
    )
    /
    9.81;


  float currentZG =
    (
      rawZ -
      driftBaseZ
    )
    /
    9.81;


  // =================================================
  // FORWARD
  // =================================================

  float rawForwardG =
    getSelectedAxis(
      currentXG,
      currentYG,
      currentZG,
      FORWARD_AXIS
    )
    *
    FORWARD_SIGN;


  // =================================================
  // SIDE
  // =================================================

  float rawSideG =
    getSelectedAxis(
      currentXG,
      currentYG,
      currentZG,
      SIDE_AXIS
    )
    *
    SIDE_SIGN;


  // =================================================
  // DYNAMIC MOVEMENT MAGNITUDE
  // =================================================

  float dynamicXG =
    dynamicX /
    9.81;


  float dynamicYG =
    dynamicY /
    9.81;


  float dynamicZG =
    dynamicZ /
    9.81;


  float rawMovementG =
    sqrt(
      (
        dynamicXG *
        dynamicXG
      )
      +
      (
        dynamicYG *
        dynamicYG
      )
      +
      (
        dynamicZG *
        dynamicZG
      )
    );


  // =================================================
  // LOW-PASS FILTER
  // =================================================

  smoothedForwardG =
    (
      smoothedForwardG *
      (
        1.0 -
        accelerationSmoothing
      )
    )
    +
    (
      rawForwardG *
      accelerationSmoothing
    );


  smoothedSideG =
    (
      smoothedSideG *
      (
        1.0 -
        accelerationSmoothing
      )
    )
    +
    (
      rawSideG *
      accelerationSmoothing
    );


  smoothedMovementG =
    (
      smoothedMovementG *
      (
        1.0 -
        accelerationSmoothing
      )
    )
    +
    (
      rawMovementG *
      accelerationSmoothing
    );
}


// ==================================================
// CALIBRATION
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
    totalX /
    samples;


  baseY =
    totalY /
    samples;


  baseZ =
    totalZ /
    samples;


  // Initialise adaptive baseline.

  driftBaseX =
    baseX;


  driftBaseY =
    baseY;


  driftBaseZ =
    baseZ;


  // Initialise gravity estimate.

  gravityX =
    baseX;


  gravityY =
    baseY;


  gravityZ =
    baseZ;


  // Reset state.

  baselineState =
    BASELINE_STABLE;


  dynamicMovementEndedTime =
    millis();


  // Reset filters.

  smoothedForwardG =
    0.0;


  smoothedSideG =
    0.0;


  smoothedMovementG =
    0.0;


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
// CORNERING BRIGHTNESS
// ==================================================

void applyCorneringBrightness(
  float baseBrightness,
  float sideG,
  float &leftBrightness,
  float &rightBrightness
) {

  // =================================================
  // DEAD ZONE
  // =================================================

  if (
    abs(sideG) <
    corneringDeadZone
  ) {

    leftBrightness =
      baseBrightness;


    rightBrightness =
      baseBrightness;


    return;
  }


  // =================================================
  // REMOVE DEAD ZONE
  // =================================================

  float effectiveSideG;


  if (
    sideG >
    0
  ) {

    effectiveSideG =
      sideG -
      corneringDeadZone;
  }

  else {

    effectiveSideG =
      sideG +
      corneringDeadZone;
  }


  float availableRange =
    corneringResponseG -
    corneringDeadZone;


  float sideIntensity =
    abs(
      effectiveSideG
    )
    /
    availableRange;


  sideIntensity =
    constrain(
      sideIntensity,
      0.0,
      1.0
    );


  // Smooth response.

  sideIntensity =
    sideIntensity *
    sideIntensity;


  leftBrightness =
    baseBrightness;


  rightBrightness =
    baseBrightness;


  // =================================================
  // ONE DIRECTION
  // =================================================

  if (
    sideG >
    corneringDeadZone
  ) {

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
  }


  // =================================================
  // OTHER DIRECTION
  // =================================================

  else if (
    sideG <
    -corneringDeadZone
  ) {

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
      0.0f,
      (float)userBrightness
    );


  rightBrightness =
    constrain(
      rightBrightness,
      0.0f,
      (float)userBrightness
    );
}


// ==================================================
// BREATHING
// ==================================================

float breathingMultiplier() {

  float phase =
    (
      millis() %
      4000
    )
    /
    4000.0
    *
    2.0
    *
    PI;


  float wave =
    (
      sin(
        phase
      )
      +
      1.0
    )
    /
    2.0;


  return
    0.6 +
    (
      0.4 *
      wave
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

  float ambientBrightness =
    userBrightness *
    0.25 *
    breathingMultiplier();


  float intensity =
    movementG /
    0.50;


  intensity =
    constrain(
      intensity,
      0.0,
      1.0
    );


  intensity =
    intensity *
    intensity;


  float themeBrightness =
    ambientBrightness +
    (
      (
        userBrightness -
        ambientBrightness
      )
      *
      intensity
    );


  themeBrightness =
    constrain(
      themeBrightness,
      0.0f,
      (float)userBrightness
    );


  float leftBrightness;

  float rightBrightness;


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
  float sideG
) {

  // =================================================
  // AMBIENT
  // =================================================

  float ambientBrightness =
    userBrightness *
    0.25 *
    breathingMultiplier();


  // =================================================
  // BRAKING
  // =================================================

  if (
    forwardG <
    -brakingDeadZone
  ) {

    float effectiveBrakeG =
      abs(
        forwardG
      )
      -
      brakingDeadZone;


    float availableRange =
      brakingResponseG -
      brakingDeadZone;


    float brakeIntensity =
      effectiveBrakeG /
      availableRange;


    brakeIntensity =
      constrain(
        brakeIntensity,
        0.0,
        1.0
      );


    brakeIntensity =
      brakeIntensity *
      brakeIntensity;


    float brakeBrightness =
      ambientBrightness +
      (
        (
          userBrightness -
          ambientBrightness
        )
        *
        brakeIntensity
      );


    brakeBrightness =
      constrain(
        brakeBrightness,
        0.0f,
        (float)userBrightness
      );


    float leftBrightness;

    float rightBrightness;


    applyCorneringBrightness(
      brakeBrightness,
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


  // =================================================
  // ACCELERATION
  // =================================================

  float accelIntensity =
    0.0;


  if (
    forwardG >
    accelerationDeadZone
  ) {

    float effectiveAccelG =
      forwardG -
      accelerationDeadZone;


    float availableRange =
      accelerationResponseG -
      accelerationDeadZone;


    accelIntensity =
      effectiveAccelG /
      availableRange;


    accelIntensity =
      constrain(
        accelIntensity,
        0.0,
        1.0
      );


    accelIntensity =
      accelIntensity *
      accelIntensity;
  }


  // =================================================
  // BRIGHTNESS
  // =================================================

  float reactiveBrightness =
    ambientBrightness +
    (
      (
        userBrightness -
        ambientBrightness
      )
      *
      accelIntensity
    );


  reactiveBrightness =
    constrain(
      reactiveBrightness,
      0.0f,
      (float)userBrightness
    );


  // =================================================
  // COLOUR
  // =================================================

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


  // =================================================
  // CORNERING
  // =================================================

  float leftBrightness;

  float rightBrightness;


  applyCorneringBrightness(
    reactiveBrightness,
    sideG,
    leftBrightness,
    rightBrightness
  );


  // =================================================
  // OUTPUT
  // =================================================

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

  readAcceleration();


  float forwardG =
    smoothedForwardG;


  float sideG =
    smoothedSideG;


  float movementG =
    smoothedMovementG;


  // =================================================
  // MODE SELECT
  // =================================================

  switch (
    mode
  ) {

    case 0:

      updateMainReactiveMode(
        forwardG,
        sideG
      );

      break;


    case 1:

      updateStaticThemeMode(
        54,
        0,
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
        0,
        movementG,
        sideG
      );

      break;
  }


  // =================================================
  // SERIAL DEBUG
  // =================================================

  if (
    millis() -
    lastSerialPrint >
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
      2
    );


    Serial.print(
      "g | Side: "
    );

    Serial.print(
      sideG,
      2
    );


    Serial.print(
      "g | Movement: "
    );

    Serial.print(
      movementG,
      2
    );


    Serial.print(
      "g | Baseline: "
    );


    if (
      baselineState ==
      BASELINE_STABLE
    ) {

      Serial.println(
        "STABLE"
      );
    }

    else if (
      baselineState ==
      BASELINE_DYNAMIC
    ) {

      Serial.println(
        "DYNAMIC"
      );
    }

    else {

      Serial.println(
        "SETTLING"
      );
    }


    lastSerialPrint =
      millis();
  }
}


// ==================================================
// ENCODER ISR
// ==================================================

void IRAM_ATTR encoderISR() {

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
      clk <<
      1
    )
    |
    dt;


  uint8_t transition =
    (
      (
        encoderState <<
        2
      )
      |
      newState
    )
    &
    0x0F;


  int8_t movement =
    encoderTransitionTable[
      transition
    ];


  encoderState =
    newState;


  if (
    movement !=
    0
  ) {

    encoderAccumulatedSteps +=
      movement;
  }
}


// ==================================================
// PROCESS ENCODER
// ==================================================

void processEncoderRotation() {

  int32_t accumulatedSteps;


  noInterrupts();


  accumulatedSteps =
    encoderAccumulatedSteps;


  encoderAccumulatedSteps =
    0;


  interrupts();


  // =================================================
  // CLOCKWISE
  // =================================================

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


  // =================================================
  // ANTICLOCKWISE
  // =================================================

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


  // =================================================
  // RETURN INCOMPLETE MOVEMENT
  // =================================================

  if (
    accumulatedSteps !=
    0
  ) {

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
    digitalRead(
      ENCODER_SW
    )
    ==
    LOW;


  unsigned long now =
    millis();


  // =================================================
  // BUTTON PRESSED
  // =================================================

  if (
    buttonDown &&
    !buttonWasDown
  ) {

    buttonWasDown =
      true;


    longPressHandled =
      false;


    buttonDownTime =
      now;
  }


  // =================================================
  // LONG PRESS
  // =================================================

  if (
    buttonDown &&
    !longPressHandled
  ) {

    if (
      now -
      buttonDownTime >
      longPressTime
    ) {

      longPressHandled =
        true;


      calibrateMPU6050();
    }
  }


  // =================================================
  // BUTTON RELEASED
  // =================================================

  if (
    !buttonDown &&
    buttonWasDown
  ) {

    buttonWasDown =
      false;


    // =================================================
    // SHORT PRESS
    // =================================================

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


  delay(500);


  Serial.println(
    "MR2 Reactive LEDs — FINAL FIRMWARE V3.6"
  );


#if HILL_COMPENSATION

  Serial.println(
    "Smart hill compensation: ENABLED"
  );

#else

  Serial.println(
    "Hill compensation: DISABLED"
  );

#endif


  // =================================================
  // ENCODER
  // =================================================

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


  // Seed encoder state.

  encoderState =
    (
      (
        digitalRead(
          ENCODER_CLK
        )
        <<
        1
      )
      |
      digitalRead(
        ENCODER_DT
      )
    );


  // Attach interrupts.

  attachInterrupt(
    digitalPinToInterrupt(
      ENCODER_CLK
    ),
    encoderISR,
    CHANGE
  );


  attachInterrupt(
    digitalPinToInterrupt(
      ENCODER_DT
    ),
    encoderISR,
    CHANGE
  );


  // =================================================
  // LED STRIPS
  // =================================================

  leftStrip.begin();

  rightStrip.begin();


  leftStrip.setBrightness(
    255
  );


  rightStrip.setBrightness(
    255
  );


  leftStrip.clear();

  rightStrip.clear();


  leftStrip.show();

  rightStrip.show();


  // Startup animation.

  startupSweep();


  // =================================================
  // I2C
  // =================================================

  Wire.begin(
    I2C_SDA,
    I2C_SCL
  );


  // =================================================
  // MPU6050
  // =================================================

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


      delay(300);


      allOff();


      delay(300);
    }
  }


  Serial.println(
    "MPU6050 found."
  );


  // =================================================
  // MPU6050 CONFIGURATION
  // =================================================

  mpu.setAccelerometerRange(
    MPU6050_RANGE_4_G
  );


  mpu.setGyroRange(
    MPU6050_RANGE_500_DEG
  );


  mpu.setFilterBandwidth(
    MPU6050_BAND_21_HZ
  );


  // =================================================
  // INITIAL CALIBRATION
  // =================================================

  calibrateMPU6050();


  Serial.println(
    "Setup complete."
  );
}


// ==================================================
// MAIN LOOP
// ==================================================

void loop() {

  // Encoder rotation.

  processEncoderRotation();


  // Encoder button.

  readEncoderButton();


  // Accelerometer and LEDs.

  updateLEDs();
}