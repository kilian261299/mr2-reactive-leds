#include <Wire.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

/*
  ==================================================
  MR2 Reactive LEDs — Final Firmware V3.5
  ==================================================

  Hardware:

  - ESP32-C3 Super Mini
  - MPU6050 / GY-521 accelerometer
  - KY-040 rotary encoder
  - Two WS2812B LED strips
  - SN74AHCT125N level shifter
  - 12V to 5V converter

  --------------------------------------------------
  V3.5 CHANGES
  --------------------------------------------------

  V3.5 is based directly on V3.4.

  The V3.4 interrupt-driven rotary encoder system has
  been retained.

  Improvements in V3.5:

  - Accelerometer response smoothing.
  - Forward acceleration dead zone.
  - Braking dead zone.
  - Cornering dead zone.
  - Progressive acceleration response.
  - Progressive braking response.
  - Progressive cornering response.
  - Improved blue-to-orange colour transition.
  - Cleaner accelerometer signal processing.
  - Manual RGB brightness scaling, replacing the
    library's setBrightness() for colour output, to
    fix a hue shift on theme colours during the idle
    breathing effect.

  --------------------------------------------------
  COLOUR TRANSITION
  --------------------------------------------------

  Main reactive mode:

    Low acceleration:
      Deep blue

    Medium acceleration:
      Controlled blue / violet-blue

    Strong acceleration:
      Warm orange

  The green channel is deliberately kept low during
  the transition to avoid the pale purple/white
  appearance produced by the previous RGB interpolation.

  --------------------------------------------------
  BRIGHTNESS / COLOUR SCALING
  --------------------------------------------------

  Brightness is no longer applied using the NeoPixel
  library's setBrightness(). Both strips are fixed at
  maximum library brightness (255) once, in setup().

  Instead, every effect scales its own R, G, and B
  values directly (see setStrip()) before the colour
  is ever sent to the strip.

  Reason:
  The library's setBrightness() scales each channel
  independently using integer math, which does not
  stay proportional between channels when they start
  at very different values. At low brightness during
  breathing, this produced a visible hue shift rather
  than a clean dim colour.

  Scaling all channels together, in one place, using
  the same brightness fraction keeps hue stable across
  the full breathing/reactive brightness range.

  Purple uses a clearly blue-dominant RGB ratio
  (45/0/90) rather than the previous near-even
  red/blue ratio (54/0/63). This makes the purple
  more visually deep while reducing sensitivity to
  low-brightness integer rounding.

  --------------------------------------------------
  ACCELEROMETER RESPONSE
  --------------------------------------------------

  Accelerometer signals are low-pass filtered before
  being used by the LED effects.

  Lower smoothing value:
    Faster response.
    More sensitive to noise.

  Higher smoothing value:
    Slower response.
    Smoother and more stable.

  Dead zones prevent small movements from triggering
  reactive effects.

  --------------------------------------------------
  HILL COMPENSATION
  --------------------------------------------------

  A slowly drifting accelerometer baseline compensates
  for sustained vehicle pitch changes.

  This helps prevent hills from being interpreted as
  continuous acceleration or braking.

  --------------------------------------------------
  MODES
  --------------------------------------------------

  Mode 0 — Main Reactive

    Calm:
      Dim blue breathing.

    Acceleration:
      Blue -> violet-blue -> orange.

    Braking:
      Red glow.

    Cornering:
      Left/right brightness shift.

  Mode 1 — Purple Theme

    Deep purple with full breathing depth.

  Mode 2 — Red Theme

  Mode 3 — Blue Theme

  Mode 4 — Green Theme

  Theme modes use fixed colours.

  Brightness responds to overall movement.

  Cornering shifts brightness between the two strips.

  Calm conditions produce the breathing effect.

  --------------------------------------------------
  ENCODER
  --------------------------------------------------

  Rotate:
    Adjust maximum brightness.

  Short press:
    Change mode.

  Long press:
    Recalibrate accelerometer baseline.

  --------------------------------------------------
  INSTALLATION
  --------------------------------------------------

  1. Mount MPU6050 flat and straight if possible.
  2. Turn the car on.
  3. Keep the car still on flat ground.
  4. Long press encoder to calibrate.
  5. Wait for green confirmation flash.

  --------------------------------------------------
  POWER
  --------------------------------------------------

  - All grounds must be common.
  - LED strips powered from 5V converter.
  - Do not power LED strips from ESP32.
  - Use fuse on 12V input.
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


// Fixed calibration baseline.

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


void updateDriftingBaseline(
  float rawX,
  float rawY,
  float rawZ
) {

#if HILL_COMPENSATION

  driftBaseX =
    driftBaseX *
    (1.0 - baselineDriftRate) +
    rawX *
    baselineDriftRate;

  driftBaseY =
    driftBaseY *
    (1.0 - baselineDriftRate) +
    rawY *
    baselineDriftRate;

  driftBaseZ =
    driftBaseZ *
    (1.0 - baselineDriftRate) +
    rawZ *
    baselineDriftRate;

#endif
}


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
// USER SETTINGS
// ==================================================

int userBrightness = 80;

const int minBrightness = 0;
const int maxBrightness = 220;

int mode = 0;

const int numberOfModes = 5;


// ==================================================
// ACCELEROMETER RESPONSE SETTINGS
// ==================================================

// --------------------------------------------------
// DEAD ZONES
// --------------------------------------------------

const float accelerationDeadZone = 0.04;

const float brakingDeadZone = 0.12;

const float corneringDeadZone = 0.08;


// --------------------------------------------------
// RESPONSE RANGES
// --------------------------------------------------

const float accelerationResponseG = 0.35;

const float brakingResponseG = 0.40;

const float corneringResponseG = 0.35;


// --------------------------------------------------
// ACCELEROMETER SMOOTHING
// --------------------------------------------------

const float accelerationSmoothing = 0.15;


// ==================================================
// SMOOTHED ACCELEROMETER VALUES
// ==================================================

float smoothedForwardG = 0.0;

float smoothedSideG = 0.0;

float smoothedMovementG = 0.0;


// ==================================================
// ENCODER / BUTTON VARIABLES
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

  if (axis == AXIS_X) {

    return xG;
  }

  if (axis == AXIS_Y) {

    return yG;
  }

  return zG;
}


// ==================================================
// COLOUR HELPERS
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


  // ------------------------------------------------
  // BLUE -> CONTROLLED VIOLET-BLUE
  // ------------------------------------------------

  if (amount < 0.5) {

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


  // ------------------------------------------------
  // CONTROLLED VIOLET-BLUE -> ORANGE
  // ------------------------------------------------

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
// LED OUTPUT HELPERS
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


  int origR = (colour >> 16) & 0xFF;
  int origG = (colour >> 8) & 0xFF;
  int origB = colour & 0xFF;

  int r = (int)round((origR * brightness) / 255.0f);
  int g = (int)round((origG * brightness) / 255.0f);
  int b = (int)round((origB * brightness) / 255.0f);

  r = constrain(r, 0, 255);
  g = constrain(g, 0, 255);
  b = constrain(b, 0, 255);

  uint32_t scaledColour = strip.Color(r, g, b);


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
// STARTUP SWEEP ANIMATION
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
            ) /
            65025,

            (
              sweepG *
              fade *
              userBrightness
            ) /
            65025,

            (
              sweepB *
              fade *
              userBrightness
            ) /
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
// MPU6050 READING
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


  // ------------------------------------------------
  // UPDATE DRIFTING BASELINE
  // ------------------------------------------------

  updateDriftingBaseline(
    accel.acceleration.x,
    accel.acceleration.y,
    accel.acceleration.z
  );


  // ------------------------------------------------
  // CONVERT TO G
  // ------------------------------------------------

  float xG =
    (
      accel.acceleration.x -
      driftBaseX
    )
    /
    9.81;


  float yG =
    (
      accel.acceleration.y -
      driftBaseY
    )
    /
    9.81;


  float zG =
    (
      accel.acceleration.z -
      driftBaseZ
    )
    /
    9.81;


  // ------------------------------------------------
  // SELECT FORWARD AXIS
  // ------------------------------------------------

  float rawForwardG =
    getSelectedAxis(
      xG,
      yG,
      zG,
      FORWARD_AXIS
    )
    *
    FORWARD_SIGN;


  // ------------------------------------------------
  // SELECT SIDE AXIS
  // ------------------------------------------------

  float rawSideG =
    getSelectedAxis(
      xG,
      yG,
      zG,
      SIDE_AXIS
    )
    *
    SIDE_SIGN;


  // ------------------------------------------------
  // CALCULATE OVERALL MOVEMENT
  // ------------------------------------------------

  float rawMovementG =
    sqrt(
      (
        xG *
        xG
      )
      +
      (
        yG *
        yG
      )
      +
      (
        zG *
        zG
      )
    );


  // ------------------------------------------------
  // LOW-PASS FILTER
  // ------------------------------------------------

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


  // Initialise drifting baseline.

  driftBaseX =
    baseX;


  driftBaseY =
    baseY;


  driftBaseZ =
    baseZ;


  // Reset smoothing filters.

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
// SIDE / CORNERING BRIGHTNESS
// ==================================================

void applyCorneringBrightness(
  float baseBrightness,
  float sideG,
  float &leftBrightness,
  float &rightBrightness
) {

  // ------------------------------------------------
  // DEAD ZONE
  // ------------------------------------------------

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


  // ------------------------------------------------
  // REMOVE DEAD ZONE
  // ------------------------------------------------

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


  // Smooth cornering response.

  sideIntensity =
    sideIntensity *
    sideIntensity;


  leftBrightness =
    baseBrightness;


  rightBrightness =
    baseBrightness;


  // ------------------------------------------------
  // TURN ONE DIRECTION
  // ------------------------------------------------

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


  // ------------------------------------------------
  // TURN OTHER DIRECTION
  // ------------------------------------------------

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
// IDLE BREATHING EFFECT
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

  // ------------------------------------------------
  // LOW AMBIENT BRIGHTNESS
  // ------------------------------------------------

  float ambientBrightness =
    userBrightness *
    0.25 *
    breathingMultiplier();


  // ------------------------------------------------
  // MOVEMENT INTENSITY
  // ------------------------------------------------

  float intensity =
    movementG /
    0.50;


  intensity =
    constrain(
      intensity,
      0.0,
      1.0
    );


  // Smooth movement response.

  intensity =
    intensity *
    intensity;


  // ------------------------------------------------
  // CALCULATE THEME BRIGHTNESS
  // ------------------------------------------------

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


  // ------------------------------------------------
  // APPLY CORNERING
  // ------------------------------------------------

  float leftBrightness;

  float rightBrightness;


  applyCorneringBrightness(
    themeBrightness,
    sideG,
    leftBrightness,
    rightBrightness
  );


  // ------------------------------------------------
  // OUTPUT
  // ------------------------------------------------

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

  // ------------------------------------------------
  // AMBIENT BREATHING
  // ------------------------------------------------

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


    // Smooth braking response.

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
  // ACCELERATION / NORMAL DRIVING
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


    // Smooth acceleration response.

    accelIntensity =
      accelIntensity *
      accelIntensity;
  }


  // ------------------------------------------------
  // BRIGHTNESS
  // ------------------------------------------------

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


  // ------------------------------------------------
  // COLOUR
  // ------------------------------------------------

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


  // ------------------------------------------------
  // CORNERING
  // ------------------------------------------------

  float leftBrightness;

  float rightBrightness;


  applyCorneringBrightness(
    reactiveBrightness,
    sideG,
    leftBrightness,
    rightBrightness
  );


  // ------------------------------------------------
  // OUTPUT
  // ------------------------------------------------

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

  // Update filtered accelerometer values.

  readAcceleration();


  // Use filtered values.

  float forwardG =
    smoothedForwardG;


  float sideG =
    smoothedSideG;


  float movementG =
    smoothedMovementG;


  // =================================================
  // SELECT MODE
  // =================================================

  switch (
    mode
  ) {

    // ------------------------------------------------
    // MODE 0 — MAIN REACTIVE
    // ------------------------------------------------

    case 0:

      updateMainReactiveMode(
        forwardG,
        sideG
      );

      break;


    // ------------------------------------------------
    // MODE 1 — PURPLE
    // ------------------------------------------------

    case 1:

      // Deep purple.
      // Blue-dominant 45:0:90 ratio helps maintain
      // stable hue during low-brightness breathing.

      updateStaticThemeMode(
        45,
        0,
        90,
        movementG,
        sideG
      );

      break;


    // ------------------------------------------------
    // MODE 2 — RED
    // ------------------------------------------------

    case 2:

      updateStaticThemeMode(
        255,
        0,
        0,
        movementG,
        sideG
      );

      break;


    // ------------------------------------------------
    // MODE 3 — BLUE
    // ------------------------------------------------

    case 3:

      updateStaticThemeMode(
        0,
        0,
        255,
        movementG,
        sideG
      );

      break;


    // ------------------------------------------------
    // MODE 4 — GREEN
    // ------------------------------------------------

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
  // SERIAL DEBUG OUTPUT
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


    Serial.println(
      "g"
    );


    lastSerialPrint =
      millis();
  }
}


// ==================================================
// ENCODER INTERRUPT HANDLER
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
// PROCESS ENCODER MOVEMENT
// ==================================================

void processEncoderRotation() {

  int32_t accumulatedSteps;


  // Atomically copy and clear movement.

  noInterrupts();


  accumulatedSteps =
    encoderAccumulatedSteps;


  encoderAccumulatedSteps =
    0;


  interrupts();


  // ------------------------------------------------
  // CLOCKWISE
  // ------------------------------------------------

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


  // ------------------------------------------------
  // ANTICLOCKWISE
  // ------------------------------------------------

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


  // ------------------------------------------------
  // RETURN INCOMPLETE MOVEMENT
  // ------------------------------------------------

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


  // ------------------------------------------------
  // BUTTON PRESSED
  // ------------------------------------------------

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


  // ------------------------------------------------
  // LONG PRESS
  // ------------------------------------------------

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


  // ------------------------------------------------
  // BUTTON RELEASED
  // ------------------------------------------------

  if (
    !buttonDown &&
    buttonWasDown
  ) {

    buttonWasDown =
      false;


    // ------------------------------------------------
    // SHORT PRESS
    // ------------------------------------------------

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


  // =================================================
  // ENCODER PINS
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


  // Seed encoder state from actual position.

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


  // Attach interrupts to both encoder channels.

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


  // Fixed at maximum library brightness.
  // All dimming is performed manually.

  leftStrip.setBrightness(255);

  rightStrip.setBrightness(255);


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

  // Process encoder movement captured by ISR.

  processEncoderRotation();


  // Handle encoder push button.

  readEncoderButton();


  // Read accelerometer and update LEDs.

  updateLEDs();
}