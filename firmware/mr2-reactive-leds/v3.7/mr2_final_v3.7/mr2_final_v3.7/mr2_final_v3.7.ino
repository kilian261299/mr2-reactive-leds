#include <Wire.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

/*
  ==================================================
  MR2 REACTIVE LEDs — FINAL FIRMWARE V3.7
  ==================================================

  Hardware:

  - ESP32-C3 Super Mini
  - MPU6050 / GY-521
  - KY-040 rotary encoder
  - Two WS2812B LED strips
  - SN74AHCT125N level shifter
  - 12V to 5V buck converter


  ==================================================
  V3.7 CHANGES
  ==================================================

  V3.6's hill compensation used the accelerometer only:
  a slow low-pass filter tried to guess "gravity" by
  averaging recent readings, gated by a STABLE/DYNAMIC/
  SETTLING state machine so genuine acceleration didn't
  get absorbed into it. This worked, but an accelerometer
  fundamentally cannot tell "tilted" apart from
  "accelerating" — both produce an identical reading.
  Every accelerometer-only approach is ultimately just
  managing that ambiguity, not resolving it.

  V3.7 adds the MPU6050's gyroscope to actually resolve
  it. The gyro measures rotation rate, which a hill
  produces (the car pitches as it goes up/down) but
  genuine forward acceleration does not. Tracking that
  rotation gives an estimate of the car's actual pitch
  angle, which lets the firmware calculate exactly how
  much of the raw forward reading is caused by gravity
  at that angle, and subtract only that — leaving genuine
  acceleration in the signal regardless of how long the
  hill lasts.

  This is implemented as a complementary filter: the
  gyro provides fast pitch tracking, continuously nudged
  toward an independent accelerometer-based estimate so
  long-term gyro drift can't accumulate. That
  accelerometer estimate is only reliable when the
  accelerometer is measuring gravity alone — during
  strong acceleration/braking/cornering it's measuring
  gravity PLUS the vehicle's own motion, so it's
  temporarily wrong, and the filter reduces how much it's
  trusted during those moments (see ACCELEROMETER
  RELIABILITY GATING in updatePitchEstimate()) rather
  than blending it in unconditionally.

  Two issues found during code review after the initial
  V3.7 implementation, both fixed here:

  - Gyro zero-rate bias (a small constant sensor offset
    present even at rest) was being integrated directly,
    which could cause continuous phantom pitch drift and
    get the state machine stuck in DYNAMIC permanently.
    Now measured once per calibration and subtracted from
    every gyro reading (gyroYBiasRadPerSec).
  - The accelerometer correction had no protection against
    being trusted during genuine acceleration — added the
    magnitude-based gating described above.

  Specific changes:

  - Added gyroscope-based pitch angle tracking
    (updatePitchEstimate(), pitchAngleRad).
  - Complementary filter blends gyro (fast, drifts) with
    accelerometer (stable, but confused by real
    acceleration) — see pitchComplementaryAlpha, and the
    reliability gating that scales the accelerometer's
    influence based on how far total accelerometer
    magnitude is from 1g.
  - Gyro bias is measured during calibration and removed
    from every reading (gyroYBiasRadPerSec).
  - Forward axis reading now has gravity's calculated
    pitch component subtracted directly, rather than
    relying on the old slow-adapting baseline.
  - The old accelerometer-only "gravity" low-pass tracker
    (gravityX/Y/Z, gravitySmoothing) has been removed —
    the pitch estimate replaces it, and does the same job
    more accurately.
  - driftBaseX has been removed. The forward axis no
    longer needs an adaptive baseline at all, since pitch
    compensation handles hills directly. driftBaseY and
    driftBaseZ are retained unchanged, since cornering
    (side axis) wasn't part of this change.
  - The STABLE/DYNAMIC/SETTLING state machine is
    retained, gating driftBaseY/Z adaptation exactly as
    in V3.6, and still provides protection against
    transient movement being misread.
  - Removed three unused threshold constants left over
    from V3.6 (baselineAccelerationThreshold,
    baselineBrakingThreshold, baselineCorneringThreshold)
    that were declared but never actually referenced
    anywhere in the V3.6 logic.
  - Renamed baseX to calibrationAccelX to reflect what it
    actually does now (a one-time input to seed the pitch
    estimate) rather than implying it's still a baseline
    like baseY/baseZ are.
  - Serial debug now reports pitch two ways: absolute
    angle (what the gravity-compensation math actually
    uses — deliberately includes any fixed mounting
    offset, since that's physically correct for computing
    gravity's real component) and pitch relative to the
    last calibration (a more human-readable "degrees of
    hill" number for watching the debug output, display
    only — not used in the compensation math itself).

  IMPORTANT — this pitch compensation is written
  specifically for FORWARD_AXIS = AXIS_X with pitch read
  from the gyro's Y-axis, matching this vehicle's actual
  mounting. If FORWARD_AXIS is ever changed, or the
  sensor is remounted in a different orientation, the
  gyro axis used in updatePitchEstimate() (currently
  gyro.gyro.y) needs to be revisited to match.

  Everything else — LED colour/brightness handling,
  encoder, modes, breathing, startup sweep — is
  unchanged from V3.6.


  ==================================================
  V3.7 HILL COMPENSATION (GYRO + ACCELEROMETER FUSION)
  ==================================================

  1. The gyro's Y-axis rotation rate is integrated over
     time to track how much the car has pitched since
     the last reading.

  2. The accelerometer independently estimates pitch
     from the direction gravity currently appears to be
     pulling — accurate at rest, unreliable during real
     acceleration.

  3. A complementary filter blends these two estimates
     (pitchComplementaryAlpha, default 0.98): mostly the
     gyro, continuously nudged toward the accelerometer's
     estimate so gyro drift can't build up over a long
     drive.

  4. From the resulting pitch angle, the expected gravity
     component along the forward axis is calculated
     (9.81 * sin(pitchAngleRad)) and subtracted from the
     raw forward reading.

  5. What remains is the genuine dynamic (non-gravity)
     component — real acceleration or braking — regardless
     of how long the vehicle has been on a slope.

  This means:

    STEADY HILL, CONSTANT SPEED
      -> Pitch angle stabilises, gravity component is
         fully subtracted, LEDs read as calm/blue.

    GENUINE ACCELERATION (on a hill or flat road)
      -> Cannot be explained by the pitch angle, remains
         in the compensated signal, LEDs react normally.


  ==================================================
  BASELINE STATES (RETAINED FROM V3.6, Y/Z ONLY)
  ==================================================

  STABLE

    Vehicle is calm.

    driftBaseY/Z are allowed to slowly adapt.

  DYNAMIC

    Genuine dynamic movement detected.

    driftBaseY/Z are frozen.

  SETTLING

    Dynamic movement has stopped.

    driftBaseY/Z remain frozen temporarily.

    After 750 ms of calm, the system returns to
    STABLE and adaptation resumes.

  (The forward/X axis is no longer part of this system —
  see V3.7 CHANGES above.)


  ==================================================
  MODES
  ==================================================

  Mode 0 — Main Reactive

    Calm:
      Dim blue breathing.

    Acceleration:
      Blue -> violet -> orange.

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
    Recalibrate accelerometer + gyro pitch reference.


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
#define ENCODER_SW 6

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

// baseY/baseZ are genuine baselines — driftBaseY/Z
// start from them and continue adapting. baseX is
// different: it's not a baseline anymore, just the raw
// calibration-time X reading used once, to seed
// pitchAngleRad (see calibrateMPU6050()). Named
// calibrationAccelX to make that distinction clear.

float calibrationAccelX = 0.0;
float baseY = 0.0;
float baseZ = 0.0;


// ==================================================
// SMART BASELINE (SIDE AXIS ONLY — SEE V3.7 CHANGES)
// ==================================================

// Slowly changing lateral/vertical baseline. The
// forward/X axis no longer uses this — see the gyro
// pitch compensation system further down instead.

float driftBaseY = 0.0;
float driftBaseZ = 0.0;


// ==================================================
// BASELINE ADAPTATION
// ==================================================

// Slow adaptation.
//
// This controls how quickly a new hill orientation
// becomes the new baseline.
//
// Lower = slower adaptation.
// Higher = faster adaptation.

const float baselineDriftRate = 0.05;


// ==================================================
// STABILITY THRESHOLDS
// ==================================================

// Movement must fall below this level before
// settling can complete.

const float baselineStableThreshold = 0.045;


// Once stable, dynamic movement must exceed this
// level to return to DYNAMIC.

const float baselineDynamicReentryThreshold = 0.075;


// ==================================================
// SETTLING
// ==================================================

// Short protection period after dynamic movement.
//
// During this period the baseline remains frozen.
//
// This prevents the end of acceleration from being
// immediately absorbed as a new hill baseline.

const unsigned long baselineSettleTime = 750;


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

const float accelerationSmoothing = 0.25;


// ==================================================
// FILTERED VALUES
// ==================================================

float smoothedForwardG = 0.0;

float smoothedSideG = 0.0;

float smoothedMovementG = 0.0;


// Separate from smoothedMovementG above (which is for
// LED output and computed later in the loop). This one
// feeds the STABLE/DYNAMIC/SETTLING gating and needs to
// exist earlier, before driftBaseY/Z get used. Smoothing
// a signal before comparing it to a fairly tight
// threshold (0.075g) is important — a single noisy raw
// sample can spike past that threshold even at rest,
// which was likely why the state machine could get stuck
// in DYNAMIC permanently.

float smoothedGatingMovementG = 0.0;


// ==================================================
// GYRO PITCH TRACKING (V3.7)
// ==================================================

// Replaces V3.6's gravityX/Y/Z low-pass tracker. See
// the V3.7 CHANGES header comment for the reasoning.

// Current best estimate of vehicle pitch, in radians.
// Positive/negative convention isn't critical here —
// FORWARD_SIGN below still handles final direction,
// exactly like every other axis in this codebase.

float pitchAngleRad = 0.0;


// The absolute pitch angle at the moment of the last
// calibration. Used only for a more human-readable debug
// display ("pitch relative to calibration" rather than
// raw absolute angle) — never used in the actual gravity
// compensation math, which correctly needs the absolute
// angle instead. See calibrateMPU6050() and the serial
// debug output.

float calibrationPitchRad = 0.0;


// Complementary filter blend factor.
//
// Closer to 1.0 = trust the gyro more (responds fast,
//   but slowly drifts over time if left uncorrected).
// Closer to 0.0 = trust the accelerometer more (stable
//   long-term, but wrong during genuine acceleration).
//
// 0.98 is a common, reasonable starting point.

const float pitchComplementaryAlpha = 0.98;


// If the gyro's sign convention doesn't match the
// accelerometer-based pitch formula's convention, the
// gyro-integrated estimate can momentarily track pitch
// in the WRONG direction during a fast tilt — even
// though it's correct once things settle and the
// accelerometer correction pulls it back. This can show
// up as a brief, wrong-looking colour flash regardless
// of which way you tilt. If that happens, flip this to
// -1 and retest — same empirical-tuning approach as
// FORWARD_SIGN/SIDE_SIGN elsewhere in this codebase.

const int PITCH_GYRO_SIGN = 1;


// Used to compute dt between pitch updates. 0 means
// "not yet initialised" — the first reading after
// startup/calibration is used to seed this rather than
// integrating against a stale/zero timestamp.

unsigned long lastPitchUpdateMicros = 0;


// Every real gyro reads a small nonzero rate even at
// rest — a manufacturing quirk called bias. Left
// uncorrected, this integrates into a slow, continuous,
// phantom pitch drift even when the vehicle never moves,
// which can easily masquerade as constant "acceleration"
// and get the smart baseline stuck in DYNAMIC forever.
// Measured once per calibration (see calibrateMPU6050())
// and subtracted from every gyro reading afterward.

float gyroYBiasRadPerSec = 0.0;


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
// GYRO PITCH ESTIMATE (V3.7)
// ==================================================

// Complementary filter: mostly integrates the gyro's
// rotation rate (fast, no gravity-tilt confusion, but
// drifts slowly over time), continuously nudged toward
// an accelerometer-only pitch estimate (no long-term
// drift, but wrong during genuine acceleration) so that
// drift can't build up over a long drive.
//
// Written specifically for this vehicle's mounting:
// pitch is read from the gyro's Y-axis. If FORWARD_AXIS
// or the physical mounting ever changes, this needs
// revisiting.

void updatePitchEstimate(
  float rawX,
  float rawY,
  float rawZ,
  float gyroYRadPerSec
) {

  unsigned long nowMicros =
    micros();


  // First call after startup/calibration — nothing to
  // integrate against yet, just seed the clock.

  if (
    lastPitchUpdateMicros ==
    0
  ) {

    lastPitchUpdateMicros =
      nowMicros;


    return;
  }


  float dt =
    (
      nowMicros -
      lastPitchUpdateMicros
    )
    /
    1000000.0;


  lastPitchUpdateMicros =
    nowMicros;


  // Remove the measured zero-rate bias before
  // integrating — otherwise a constant sensor offset
  // integrates into continuous phantom pitch drift even
  // while genuinely stationary. Sign correction applied
  // after bias removal, since the bias itself was
  // measured in raw (unflipped) units during calibration.

  float correctedGyroYRadPerSec =
    (
      gyroYRadPerSec -
      gyroYBiasRadPerSec
    )
    *
    PITCH_GYRO_SIGN;


  // Accelerometer-only pitch estimate — accurate at
  // rest, unreliable during genuine acceleration. Only
  // ever blended in gently, never trusted outright.

  float accelPitchRad =
    atan2(
      -rawX,
      sqrt(
        (
          rawY *
          rawY
        )
        +
        (
          rawZ *
          rawZ
        )
      )
    );


  // =================================================
  // ACCELEROMETER RELIABILITY GATING
  // =================================================

  // The accelerometer only measures gravity's direction
  // correctly when it ISN'T also measuring real
  // acceleration. At rest (or steady speed), total
  // accelerometer magnitude reads ~1g regardless of
  // orientation — that's just gravity. Under real
  // acceleration/braking/cornering, the vehicle's own
  // acceleration adds to that, so total magnitude moves
  // away from 1g. The further away it is, the less the
  // accelerometer's pitch estimate can be trusted, so
  // its influence on the filter is scaled down to match
  // — down to fully ignored during a hard event, leaving
  // the filter running on gyro alone until things settle.

  float totalAccelMagG =
    sqrt(
      (
        rawX *
        rawX
      )
      +
      (
        rawY *
        rawY
      )
      +
      (
        rawZ *
        rawZ
      )
    )
    /
    9.81;


  float accelMagDeviation =
    fabs(
      totalAccelMagG -
      1.0
    );


  // How far from 1g before the accelerometer correction
  // is fully distrusted. 0.3g is a reasonable starting
  // point — noticeably harder than normal driving, but
  // not an extreme event.

  const float accelTrustFadeRangeG = 0.3;

  float accelReliability =
    1.0 -
    constrain(
      accelMagDeviation /
      accelTrustFadeRangeG,
      0.0,
      1.0
    );


  float effectiveAccelWeight =
    (
      1.0 -
      pitchComplementaryAlpha
    )
    *
    accelReliability;

  float effectiveGyroWeight =
    1.0 -
    effectiveAccelWeight;


  pitchAngleRad =
    (
      effectiveGyroWeight *
      (
        pitchAngleRad +
        (
          correctedGyroYRadPerSec *
          dt
        )
      )
    )
    +
    (
      effectiveAccelWeight *
      accelPitchRad
    );
}


// ==================================================
// SMART BASELINE UPDATE
// ==================================================

void updateSmartBaseline(
  float rawY,
  float rawZ,
  float dynamicMovementG
) {

#if HILL_COMPENSATION

  unsigned long now =
    millis();


  bool dynamicMovement =
    dynamicMovementG >
    baselineDynamicReentryThreshold;


  if (
    dynamicMovement
  ) {

    baselineState =
      BASELINE_DYNAMIC;


    dynamicMovementEndedTime =
      now;


    return;
  }


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


  if (
    baselineState ==
    BASELINE_SETTLING
  ) {

    if (
      dynamicMovementG >
      baselineDynamicReentryThreshold
    ) {

      baselineState =
        BASELINE_DYNAMIC;


      dynamicMovementEndedTime =
        now;


      return;
    }


    if (
      dynamicMovementG >
      baselineStableThreshold
    ) {

      dynamicMovementEndedTime =
        now;


      return;
    }


    if (
      now -
      dynamicMovementEndedTime <
      baselineSettleTime
    ) {

      return;
    }


    baselineState =
      BASELINE_STABLE;
  }


  if (
    baselineState ==
    BASELINE_STABLE
  ) {

    if (
      dynamicMovementG <
      baselineStableThreshold
    ) {

      // Note: driftBaseX no longer exists here. The
      // forward axis is compensated by gyro pitch
      // tracking instead — see updatePitchEstimate()
      // and readAcceleration(). Only Y/Z (side/vertical)
      // still use this slow adaptive baseline.

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


  float rawX =
    accel.acceleration.x;


  float rawY =
    accel.acceleration.y;


  float rawZ =
    accel.acceleration.z;


  // =================================================
  // GYRO PITCH UPDATE (V3.7)
  // =================================================

  // Pitch is read from the gyro's Y-axis for this
  // vehicle's mounting — see updatePitchEstimate().
  // Gated by HILL_COMPENSATION so the toggle still
  // means "no hill compensation at all" for comparison/
  // debugging, not just "the old baseline system off."

#if HILL_COMPENSATION

  updatePitchEstimate(
    rawX,
    rawY,
    rawZ,
    gyro.gyro.y
  );


  // How much of rawX is explained by gravity at the
  // current estimated pitch angle.

  float gravityForwardComponent =
    9.81 *
    sin(
      pitchAngleRad
    );


  // What's left after removing it — genuine dynamic
  // (non-gravity) forward acceleration, regardless of
  // how long the vehicle has been on a slope.

  float pitchCompensatedX =
    rawX +
    gravityForwardComponent;

#else

  // Hill compensation disabled — use the raw reading
  // directly, uncompensated, same as if pitch tracking
  // didn't exist.

  float pitchCompensatedX =
    rawX;

#endif


  // =================================================
  // FORWARD AXIS (PITCH-COMPENSATED, X ONLY)
  // =================================================

  float currentXG =
    pitchCompensatedX /
    9.81;


  // =================================================
  // SIDE / VERTICAL AXES (UNCHANGED FROM V3.6)
  // =================================================

  // Uses driftBaseY/Z as they currently stand — i.e.
  // from the end of the previous loop. updateSmartBaseline()
  // below may adjust them for next time, but this
  // iteration's output is computed first, against
  // whatever baseline was already established.

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
  // OVERALL DYNAMIC MOVEMENT (GATES THE STATE MACHINE)
  // =================================================

  float rawMovementG =
    sqrt(
      (
        currentXG *
        currentXG
      )
      +
      (
        currentYG *
        currentYG
      )
      +
      (
        currentZG *
        currentZG
      )
    );


  // Smoothed before gating — see the comment on
  // smoothedGatingMovementG's declaration for why.

  smoothedGatingMovementG =
    (
      smoothedGatingMovementG *
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


  updateSmartBaseline(
    rawY,
    rawZ,
    smoothedGatingMovementG
  );


  // =================================================
  // FORWARD / SIDE SELECTION
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


  float rawSideG =
    getSelectedAxis(
      currentXG,
      currentYG,
      currentZG,
      SIDE_AXIS
    )
    *
    SIDE_SIGN;


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


  // Accumulates gyro Y readings while the board is held
  // still, to measure its zero-rate bias (see
  // gyroYBiasRadPerSec above).

  float totalGyroY = 0.0;


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


    totalGyroY +=
      gyro.gyro.y;


    delay(10);
  }


  calibrationAccelX =
    totalX /
    samples;


  baseY =
    totalY /
    samples;


  baseZ =
    totalZ /
    samples;


  // Whatever the gyro read on average while genuinely
  // still IS the bias — real rotation should average to
  // ~0 over 120 samples if the board didn't move.

  gyroYBiasRadPerSec =
    totalGyroY /
    samples;


  driftBaseY =
    baseY;


  driftBaseZ =
    baseZ;


  // Seed the pitch estimate directly from the
  // calibration averages, using the same formula as
  // updatePitchEstimate()'s accelerometer-only estimate.
  // Without this, pitch tracking would start assuming
  // "perfectly level" even if the sensor is actually
  // mounted at a slight angle, and would take a while
  // to converge via the complementary filter alone.
  //
  // This is the TRUE absolute tilt angle (mounting
  // offset included), which is deliberately what the
  // gravity-compensation math needs — see the header
  // comment on pitchAngleRad for why. calibrationPitchRad
  // stores this same value separately, purely so the
  // debug output can also show pitch RELATIVE to this
  // calibration position (e.g. "10 degrees of hill"
  // instead of "17 degrees absolute, which includes
  // whatever angle the sensor happens to be bolted at").

  pitchAngleRad =
    atan2(
      -calibrationAccelX,
      sqrt(
        (
          baseY *
          baseY
        )
        +
        (
          baseZ *
          baseZ
        )
      )
    );


  calibrationPitchRad =
    pitchAngleRad;


  // Reset so the next pitch update seeds its own
  // timestamp fresh, rather than integrating across the
  // gap the calibration routine's own delays created.

  lastPitchUpdateMicros =
    0;


  baselineState =
    BASELINE_STABLE;


  dynamicMovementEndedTime =
    millis();


  smoothedForwardG =
    0.0;


  smoothedSideG =
    0.0;


  smoothedMovementG =
    0.0;


  smoothedGatingMovementG =
    0.0;


  Serial.println(
    "Calibration complete."
  );


  Serial.print(
    "Base X: "
  );


  Serial.println(
    calibrationAccelX
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


  Serial.print(
    "Gyro Y bias: "
  );

  Serial.print(
    gyroYBiasRadPerSec *
    180.0 /
    PI,
    3
  );

  Serial.println(
    " deg/s"
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

  float ambientBrightness =
    userBrightness *
    0.25 *
    breathingMultiplier();


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


  float leftBrightness;

  float rightBrightness;


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

  readAcceleration();


  float forwardG =
    smoothedForwardG;


  float sideG =
    smoothedSideG;


  float movementG =
    smoothedMovementG;


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

      Serial.print(
        "STABLE"
      );
    }

    else if (
      baselineState ==
      BASELINE_DYNAMIC
    ) {

      Serial.print(
        "DYNAMIC"
      );
    }

    else {

      Serial.print(
        "SETTLING"
      );
    }


    Serial.print(
      " | Pitch (abs): "
    );

    Serial.print(
      pitchAngleRad *
      180.0 /
      PI,
      1
    );

    Serial.print(
      " deg | Pitch (vs cal): "
    );

    Serial.print(
      (
        pitchAngleRad -
        calibrationPitchRad
      )
      *
      180.0 /
      PI,
      1
    );

    Serial.println(
      " deg"
    );


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


  if (
    !buttonDown &&
    buttonWasDown
  ) {

    buttonWasDown =
      false;


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
    "MR2 Reactive LEDs — FINAL FIRMWARE V3.7"
  );


#if HILL_COMPENSATION

  Serial.println(
    "Hill compensation: ENABLED (gyro + accelerometer pitch fusion)"
  );

#else

  Serial.println(
    "Hill compensation: DISABLED"
  );

#endif


  Serial.print(
    "Baseline settling time: "
  );

  Serial.print(
    baselineSettleTime
  );

  Serial.println(
    " ms"
  );


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


  startupSweep();


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

  processEncoderRotation();


  readEncoderButton();


  updateLEDs();
}
