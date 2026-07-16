#include <Wire.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

// --------------------------------------------------
// MR2 Reactive LEDs - Test 3
// Full bench system test
//
// Hardware used:
// - ESP32-C3 Super Mini
// - KY-040 rotary encoder
// - MPU6050 / GY-521 accelerometer
// - Two WS2812B LED strips
// - SN74AHCT125N level shifter
//
// Purpose of this test:
// 1. Test left and right LED strips independently.
// 2. Use the rotary encoder for brightness and mode changes.
// 3. Use the MPU6050 to react to acceleration/braking/cornering.
// 4. Confirm the full system works before building the control box.
// --------------------------------------------------


// ==================================================
// PIN ASSIGNMENTS
// ==================================================

// LED data pins.
// These ESP32-C3 pins go into the SN74AHCT125N level shifter.
// The level shifter outputs then go through 330 ohm resistors to LED DIN.
#define LEFT_LED_PIN   2
#define RIGHT_LED_PIN  3

// KY-040 rotary encoder pins.
#define ENCODER_CLK 4
#define ENCODER_DT  5
#define ENCODER_SW  6

// MPU6050 I2C pins.
#define I2C_SDA 8
#define I2C_SCL 9


// ==================================================
// LED SETUP
// ==================================================

// You ordered 2 x 1m WS2812B COB LED strips.
// Each strip is 160 LEDs.
#define NUM_LEDS_LEFT   160
#define NUM_LEDS_RIGHT  160

Adafruit_NeoPixel leftStrip(NUM_LEDS_LEFT, LEFT_LED_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel rightStrip(NUM_LEDS_RIGHT, RIGHT_LED_PIN, NEO_GRB + NEO_KHZ800);


// ==================================================
// MPU6050 SETUP
// ==================================================

Adafruit_MPU6050 mpu;

// Startup baseline readings.
// These help remove the resting position of the sensor.
float baseX = 0.0;
float baseY = 0.0;
float baseZ = 0.0;


// ==================================================
// USER SETTINGS
// ==================================================

// Rotary encoder controls this.
// It acts as the maximum brightness limit.
int userBrightness = 80;

const int minBrightness = 0;
const int maxBrightness = 220;

// Encoder button changes mode.
int mode = 0;
const int numberOfModes = 5;


// ==================================================
// ROTARY ENCODER VARIABLES
// ==================================================

int lastCLKState;

unsigned long lastButtonPress = 0;
const unsigned long debounceDelay = 250;


// ==================================================
// TIMING VARIABLES
// ==================================================

unsigned long lastSerialPrint = 0;
const unsigned long serialPrintInterval = 250;


// ==================================================
// HELPER: COLOUR FADE FROM BLUE TO ORANGE
// ==================================================

// amount:
// 0.0 = blue
// 1.0 = orange
uint32_t blueToOrange(float amount, Adafruit_NeoPixel &strip) {
  amount = constrain(amount, 0.0, 1.0);

  int blueR = 0;
  int blueG = 0;
  int blueB = 255;

  int orangeR = 255;
  int orangeG = 80;
  int orangeB = 0;

  int r = blueR + (orangeR - blueR) * amount;
  int g = blueG + (orangeG - blueG) * amount;
  int b = blueB + (orangeB - blueB) * amount;

  return strip.Color(r, g, b);
}


// ==================================================
// HELPER: SET ONE STRIP TO ONE COLOUR
// ==================================================

void setStrip(Adafruit_NeoPixel &strip, int ledCount, uint32_t colour, int brightness) {
  brightness = constrain(brightness, 0, 255);

  strip.setBrightness(brightness);

  for (int i = 0; i < ledCount; i++) {
    strip.setPixelColor(i, colour);
  }

  strip.show();
}


// ==================================================
// HELPER: SET BOTH STRIPS
// ==================================================

void setBothStrips(uint32_t leftColour, uint32_t rightColour, int leftBrightness, int rightBrightness) {
  setStrip(leftStrip, NUM_LEDS_LEFT, leftColour, leftBrightness);
  setStrip(rightStrip, NUM_LEDS_RIGHT, rightColour, rightBrightness);
}


// ==================================================
// READ MPU6050 ACCELERATION
// ==================================================

void readAcceleration(float &xG, float &yG, float &zG, float &movementG) {
  sensors_event_t accel;
  sensors_event_t gyro;
  sensors_event_t temp;

  mpu.getEvent(&accel, &gyro, &temp);

  // Subtract baseline and convert from m/s^2 to g.
  xG = (accel.acceleration.x - baseX) / 9.81;
  yG = (accel.acceleration.y - baseY) / 9.81;
  zG = (accel.acceleration.z - baseZ) / 9.81;

  // Overall movement strength.
  // This reacts to any movement direction.
  movementG = sqrt((xG * xG) + (yG * yG) + (zG * zG));
}


// ==================================================
// UPDATE LEDS BASED ON MODE + MPU6050 READINGS
// ==================================================

void updateLEDs() {
  float xG, yG, zG, movementG;
  readAcceleration(xG, yG, zG, movementG);

  // Convert movement to intensity between 0.0 and 1.0.
  // 0.50g is treated as strong movement for this bench test.
  float intensity = movementG / 0.50;
  intensity = constrain(intensity, 0.0, 1.0);

  // Reactive brightness:
  // - At rest: about 25% of user brightness
  // - Strong movement: up to user brightness
  int reactiveBrightness = userBrightness * (0.25 + 0.75 * intensity);
  reactiveBrightness = constrain(reactiveBrightness, 0, userBrightness);

  // Base colours.
  uint32_t blueLeft = leftStrip.Color(0, 0, 255);
  uint32_t blueRight = rightStrip.Color(0, 0, 255);

  uint32_t orangeLeft = leftStrip.Color(255, 80, 0);
  uint32_t orangeRight = rightStrip.Color(255, 80, 0);

  uint32_t redLeft = leftStrip.Color(255, 0, 0);
  uint32_t redRight = rightStrip.Color(255, 0, 0);

  uint32_t purpleLeft = leftStrip.Color(120, 0, 255);
  uint32_t purpleRight = rightStrip.Color(120, 0, 255);

  switch (mode) {

    case 0:
      // --------------------------------------------------
      // Mode 0: Main reactive mode
      //
      // Movement strength controls:
      // - colour fade from blue to orange
      // - brightness increase
      // --------------------------------------------------
      setBothStrips(
        blueToOrange(intensity, leftStrip),
        blueToOrange(intensity, rightStrip),
        reactiveBrightness,
        reactiveBrightness
      );
      break;


    case 1:
      // --------------------------------------------------
      // Mode 1: Acceleration / braking axis test
      //
      // Uses X axis as a placeholder forward/back axis.
      // Positive X = orange
      // Negative X = red
      // Near zero = blue
      // --------------------------------------------------
      if (xG > 0.08) {
        setBothStrips(orangeLeft, orangeRight, reactiveBrightness, reactiveBrightness);
      } else if (xG < -0.08) {
        setBothStrips(redLeft, redRight, reactiveBrightness, reactiveBrightness);
      } else {
        setBothStrips(blueLeft, blueRight, reactiveBrightness, reactiveBrightness);
      }
      break;


    case 2:
      // --------------------------------------------------
      // Mode 2: Cornering / side-to-side test
      //
      // Uses Y axis as a placeholder left/right axis.
      // This mode tests separate left and right LED behaviour.
      //
      // Y positive = left brighter
      // Y negative = right brighter
      // --------------------------------------------------

      int leftBrightness;
      int rightBrightness;

      if (yG > 0.08) {
        leftBrightness = userBrightness;
        rightBrightness = userBrightness * 0.25;
      } else if (yG < -0.08) {
        leftBrightness = userBrightness * 0.25;
        rightBrightness = userBrightness;
      } else {
        leftBrightness = reactiveBrightness;
        rightBrightness = reactiveBrightness;
      }

      setBothStrips(purpleLeft, purpleRight, leftBrightness, rightBrightness);
      break;


    case 3:
      // --------------------------------------------------
      // Mode 3: Left/right output test
      //
      // Left strip = blue
      // Right strip = orange
      //
      // Useful to confirm left and right wiring is correct.
      // --------------------------------------------------
      setBothStrips(blueLeft, orangeRight, userBrightness, userBrightness);
      break;


    case 4:
      // --------------------------------------------------
      // Mode 4: White brightness / current test
      //
      // Movement controls brightness.
      // White uses red, green, and blue together, so it draws the most current.
      // Do not run this at high brightness for long during early testing.
      // --------------------------------------------------
      setBothStrips(
        leftStrip.Color(255, 255, 255),
        rightStrip.Color(255, 255, 255),
        reactiveBrightness,
        reactiveBrightness
      );
      break;
  }

  // Serial debug output.
  if (millis() - lastSerialPrint > serialPrintInterval) {
    Serial.print("Mode: ");
    Serial.print(mode);

    Serial.print(" | User brightness: ");
    Serial.print(userBrightness);

    Serial.print(" | X: ");
    Serial.print(xG, 2);

    Serial.print("g | Y: ");
    Serial.print(yG, 2);

    Serial.print("g | Z: ");
    Serial.print(zG, 2);

    Serial.print("g | Movement: ");
    Serial.print(movementG, 2);

    Serial.print("g | Reactive brightness: ");
    Serial.println(reactiveBrightness);

    lastSerialPrint = millis();
  }
}


// ==================================================
// READ ROTARY ENCODER ROTATION
// ==================================================

void readEncoderRotation() {
  int currentCLKState = digitalRead(ENCODER_CLK);

  if (currentCLKState != lastCLKState) {

    // Compare DT with CLK to detect direction.
    if (digitalRead(ENCODER_DT) != currentCLKState) {
      userBrightness += 5;
    } else {
      userBrightness -= 5;
    }

    userBrightness = constrain(userBrightness, minBrightness, maxBrightness);

    Serial.print("User brightness set to: ");
    Serial.println(userBrightness);
  }

  lastCLKState = currentCLKState;
}


// ==================================================
// READ ROTARY ENCODER BUTTON
// ==================================================

void readEncoderButton() {
  if (digitalRead(ENCODER_SW) == LOW) {
    unsigned long now = millis();

    if (now - lastButtonPress > debounceDelay) {
      mode++;

      if (mode >= numberOfModes) {
        mode = 0;
      }

      Serial.print("Changed mode to: ");
      Serial.println(mode);

      lastButtonPress = now;
    }
  }
}


// ==================================================
// CALIBRATE MPU6050 BASELINE
// ==================================================

void calibrateMPU6050() {
  Serial.println("Calibrating MPU6050 baseline...");
  Serial.println("Keep the sensor still.");

  const int samples = 100;

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

  Serial.println("Calibration complete.");
  Serial.print("Base X: ");
  Serial.println(baseX);
  Serial.print("Base Y: ");
  Serial.println(baseY);
  Serial.print("Base Z: ");
  Serial.println(baseZ);
}


// ==================================================
// SETUP
// ==================================================

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println("MR2 Reactive LEDs - Test 3");
  Serial.println("Full bench system test");

  // Encoder setup.
  pinMode(ENCODER_CLK, INPUT_PULLUP);
  pinMode(ENCODER_DT, INPUT_PULLUP);
  pinMode(ENCODER_SW, INPUT_PULLUP);
  lastCLKState = digitalRead(ENCODER_CLK);

  // LED setup.
  leftStrip.begin();
  rightStrip.begin();

  leftStrip.clear();
  rightStrip.clear();

  leftStrip.show();
  rightStrip.show();

  // I2C setup.
  Wire.begin(I2C_SDA, I2C_SCL);

  // MPU6050 setup.
  if (!mpu.begin()) {
    Serial.println("ERROR: MPU6050 not found.");
    Serial.println("Check VCC, GND, SDA, and SCL wiring.");

    // Flash both strips red if MPU6050 is not detected.
    while (1) {
      setBothStrips(leftStrip.Color(255, 0, 0), rightStrip.Color(255, 0, 0), 40, 40);
      delay(300);
      setBothStrips(leftStrip.Color(0, 0, 0), rightStrip.Color(0, 0, 0), 0, 0);
      delay(300);
    }
  }

  Serial.println("MPU6050 found.");

  // Sensor configuration.
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

  // Small delay keeps movement response smooth.
  delay(20);
}