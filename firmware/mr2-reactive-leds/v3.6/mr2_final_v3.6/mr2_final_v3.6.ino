#include <Wire.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

/*
  ==================================================
  MR2 REACTIVE LEDs — FINAL FIRMWARE V3.6
  ==================================================

  V3.6 — FAST DYNAMIC DETECTION + SLOW HILL COMPENSATION

  Core principle:

    FAST SIGNAL
      Detects genuine vehicle movement.

    SLOW GRAVITY ESTIMATE
      Tracks long-term vehicle pitch / roll caused
      by hills and slopes.

  The two systems are deliberately separated.

  Dynamic acceleration / braking / cornering:
    - Detected from the fast acceleration component.
    - Freezes adaptive hill baseline.

  Hills / vehicle pitch:
    - Slowly absorbed by gravity estimate.
    - Does not trigger DYNAMIC indefinitely.

  State machine:

    STABLE
      Vehicle is considered settled.
      Hill baseline is allowed to slowly adapt.

    DYNAMIC
      Genuine dynamic movement detected.
      Adaptive hill baseline is frozen.

    SETTLING
      Dynamic movement has stopped.
      Baseline remains frozen for a delay.

    After settling:
      Return to STABLE.
      Slow baseline adaptation resumes.


  ==================================================
  HARDWARE
  ==================================================

  - ESP32-C3 Super Mini
  - MPU6050 / GY-521
  - KY-040 rotary encoder
  - Two WS2812B LED strips
  - SN74AHCT125N level shifter
  - 12V to 5V buck converter


  ==================================================
  FEATURES
  ==================================================

  - Fast dynamic acceleration detection
  - Fast braking detection
  - Fast cornering detection
  - Slow hill / pitch compensation
  - Adaptive gravity baseline
  - Dynamic movement freezes hill baseline
  - Settling period after movement
  - Hysteresis
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
// ADAPTIVE HILL BASELINE
// ==================================================

/*
  This represents the long-term gravity orientation.

  It is allowed to move very slowly.

  This means:

    Short acceleration:
      NOT absorbed.

    Short braking:
      NOT absorbed.

    Short cornering:
      NOT absorbed.

    Long-term hill angle:
      Slowly absorbed.
*/

float driftBaseX = 0.0;
float driftBaseY = 0.0;
float driftBaseZ = 0.0;


// ==================================================
// SLOW HILL ADAPTATION
// ==================================================

/*
  Lower value:
    Slower hill adaptation.
    Better protection against absorbing real movement.

  Higher value:
    Faster hill adaptation.
    Better response to changing slopes.

  0.0008 is intentionally slow.
*/

const float baselineDriftRate = 0.0008;


// ==================================================
// FAST DYNAMIC DETECTION
// ==================================================

/*
  These thresholds operate on the FAST dynamic
  acceleration component.

  They are NOT based on the hill-compensated
  forward/side values.

  This is important.

  A vehicle sitting still on a hill can have:

    Forward = -0.5g

  but:

    Dynamic = approximately 0g

  Therefore it should eventually become STABLE.
*/


// Forward acceleration detection.

const float dynamicAccelerationThreshold = 0.08;


// Braking detection.

const float dynamicBrakingThreshold = 0.10;


// Cornering detection.

const float dynamicCorneringThreshold = 0.08;


// ==================================================
// DYNAMIC STATE THRESHOLDS
// ==================================================

/*
  Overall fast dynamic acceleration magnitude.

  Below this:
    Considered calm.

  Above this:
    Considered dynamic.

  These values include hysteresis.
*/

const float dynamicStableThreshold = 0.045;

const float dynamicReentryThreshold = 0.075;


// ==================================================
// SETTLING
// ==================================================

/*
  After dynamic movement stops:

    DYNAMIC
       |
       v
    SETTLING
       |
       | 2 seconds of calm
       v
    STABLE
*/

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

/*
  Main reactive filtering.

  Higher:
    Faster response.

  Lower:
    Smoother response.
*/

const float accelerationSmoothing = 0.15;


// ==================================================
// FAST DYNAMIC FILTER
// ==================================================

/*
  This filter tracks the fast dynamic component.

  It is deliberately much faster than the hill
  baseline.

  This allows:

    Real acceleration -> detected quickly.

    Real braking -> detected quickly.

    Real cornering -> detected quickly.

    Hill angle -> ignored as dynamic once settled.
*/

const float dynamicSmoothing = 0.15;


// ==================================================
// GRAVITY / HILL FILTER
// ==================================================

/*
  Very slow gravity tracking.

  This is the key hill compensation filter.

  It must be significantly slower than the dynamic
  filter.

  At 0.008, the gravity estimate changes slowly.
*/

const float gravitySmoothing = 0.008;


// ==================================================
// FILTERED VALUES
// ==================================================

float smoothedForwardG = 0.0;

float smoothedSideG = 0.0;

float smoothedMovementG = 0.0;


// ==================================================
// FAST DYNAMIC COMPONENT
// ==================================================

float dynamicForwardG = 0.0;

float dynamicSideG = 0.0;

float dynamicMovementG = 0.0;


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
// LED RESPONSE
// ==================================================

const float accelerationDeadZone = 0.04;

const float brakingDeadZone = 0.12;

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
  float rawZ
) {

#if HILL_COMPENSATION

  unsigned long now =
    millis();


  // =================================================
  // DYNAMIC MOVEMENT DETECTION
  // =================================================

  /*
    IMPORTANT:

    Dynamic detection uses the FAST dynamic
    acceleration signal.

    It does NOT use forwardG or sideG.

    Therefore a static hill orientation cannot
    permanently hold the system in DYNAMIC.
  */

  bool accelerating =
    dynamicForwardG >
    dynamicAccelerationThreshold;


  bool braking =
    dynamicForwardG <
    -dynamicBrakingThreshold;


  bool cornering =
    abs(dynamicSideG) >
    dynamicCorneringThreshold;


  bool dynamicMovementDetected =
    accelerating ||
    braking ||
    cornering;


  // =================================================
  // DYNAMIC STATE
  // =================================================

  if (
    dynamicMovementDetected
  ) {

    /*
      Genuine movement detected.

      Freeze adaptive hill baseline.
    */

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

    /*
      Dynamic movement has stopped.

      Do NOT immediately adapt the baseline.

      Enter settling period.
    */

    baselineState =
      BASELINE_SETTLING;


    dynamicMovementEndedTime =
      now;


    return;
  }


  // =================================================
  // SETTLING
  // =================================================

  if (
    baselineState ==
    BASELINE_SETTLING
  ) {

    /*
      If meaningful dynamic movement starts again,
      return immediately to DYNAMIC.
    */

    if (
      dynamicMovementG >
      dynamicReentryThreshold
    ) {

      baselineState =
        BASELINE_DYNAMIC;


      dynamicMovementEndedTime =
        now;


      return;
    }


    /*
      If movement is still above the stable threshold,
      restart the settling timer.

      This ensures the vehicle must genuinely settle.
    */

    if (
      dynamicMovementG >
      dynamicStableThreshold
    ) {

      dynamicMovementEndedTime =
        now;


      return;
    }


    /*
      Require a full settling period.
    */

    if (
      now -
      dynamicMovementEndedTime <
      baselineSettleTime
    ) {

      return;
    }


    /*
      Settling complete.
    */

    baselineState =
      BASELINE_STABLE;
  }


  // =================================================
  // STABLE
  // =================================================

  if (
    baselineState ==
    BASELINE_STABLE
  ) {

    /*
      Only slowly adapt the hill baseline when the
      FAST dynamic signal is genuinely calm.

      This slowly learns a new hill angle without
      learning short acceleration/braking events.
    */

    if (
      dynamicMovementG <
      dynamicStableThreshold
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
  // BLUE -> VIOLET-BLUE
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
  // SLOW GRAVITY / HILL TRACKING
  // =================================================

  /*
    This follows the long-term gravity vector.

    It deliberately moves much slower than the dynamic
    signal.

    Therefore:

      Hill:
        Slowly tracked.

      Acceleration:
        Appears as a fast difference.

      Braking:
        Appears as a fast difference.

      Cornering:
        Appears as a fast difference.
  */

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
  // FAST DYNAMIC COMPONENT
  // =================================================

  float fastDynamicX =
    rawX -
    gravityX;


  float fastDynamicY =
    rawY -
    gravityY;


  float fastDynamicZ =
    rawZ -
    gravityZ;


  // =================================================
  // CONVERT TO G
  // =================================================

  float fastDynamicXG =
    fastDynamicX /
    9.81;


  float fastDynamicYG =
    fastDynamicY /
    9.81;


  float fastDynamicZG =
    fastDynamicZ /
    9.81;


  // =================================================
  // FAST FORWARD DYNAMIC COMPONENT
  // =================================================

  float rawDynamicForwardG =
    getSelectedAxis(
      fastDynamicXG,
      fastDynamicYG,
      fastDynamicZG,
      FORWARD_AXIS
    )
    *
    FORWARD_SIGN;


  // =================================================
  // FAST SIDE DYNAMIC COMPONENT
  // =================================================

  float rawDynamicSideG =
    getSelectedAxis(
      fastDynamicXG,
      fastDynamicYG,
      fastDynamicZG,
      SIDE_AXIS
    )
    *
    SIDE_SIGN;


  // =================================================
  // FAST DYNAMIC MAGNITUDE
  // =================================================

  float rawDynamicMovementG =
    sqrt(
      (
        fastDynamicXG *
        fastDynamicXG
      )
      +
      (
        fastDynamicYG *
        fastDynamicYG
      )
      +
      (
        fastDynamicZG *
        fastDynamicZG
      )
    );


  // =================================================
  // FAST DYNAMIC FILTER
  // =================================================

  dynamicForwardG =
    (
      dynamicForwardG *
      (
        1.0 -
        dynamicSmoothing
      )
    )
    +
    (
      rawDynamicForwardG *
      dynamicSmoothing
    );


  dynamicSideG =
    (
      dynamicSideG *
      (
        1.0 -
        dynamicSmoothing
      )
    )
    +
    (
      rawDynamicSideG *
      dynamicSmoothing
    );


  dynamicMovementG =
    (
      dynamicMovementG *
      (
        1.0 -
        dynamicSmoothing
      )
    )
    +
    (
      rawDynamicMovementG *
      dynamicSmoothing
    );


  // =================================================
  // UPDATE SMART BASELINE STATE
  // =================================================

  updateSmartBaseline(
    rawX,
    rawY,
    rawZ
  );


  // =================================================
  // HILL-COMPENSATED REACTIVE VALUES
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
  // MAIN REACTIVE FILTER
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


  /*
    The LED system still uses the hill-compensated
    forward/side values.

    The STATE MACHINE uses the fast dynamic values.

    This separation is the key V3.6 improvement.
  */
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


  // Initialise adaptive hill baseline.

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


  // Reset reactive filters.

  smoothedForwardG =
    0.0;


  smoothedSideG =
    0.0;


  smoothedMovementG =
    0.0;


  // Reset dynamic filters.

  dynamicForwardG =
    0.0;


  dynamicSideG =
    0.0;


  dynamicMovementG =
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


  sideIntensity =
    sideIntensity *
    sideIntensity;


  leftBrightness =
    baseBrightness;


  rightBrightness =
    baseBrightness;


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
    dynamicMovementG;


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
      "g | Dynamic: "
    );

    Serial.print(
      dynamicMovementG,
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