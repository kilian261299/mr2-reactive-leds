#include <Wire.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

// --------------------------------------------------
// MR2 Reactive LEDs - Test 2
// KY-040 rotary encoder + MPU6050 accelerometer + WS2812B LEDs
//
// Purpose of this test:
// 1. Keep the Test 1 LED + encoder behaviour working.
// 2. Add MPU6050 accelerometer readings.
// 3. Use movement/acceleration to change LED colour and brightness.
// 4. Confirm the LED data still works through the SN74AHCT125N level shifter.
//
// This test uses ONE 1m LED strip.
// --------------------------------------------------


// ==================================================
// PIN ASSIGNMENTS
// ==================================================

// LED data pin.
// Wiring path:
// ESP32-C3 GPIO 2
// → SN74AHCT125N input
// → SN74AHCT125N output
// → 330 ohm resistor
// → WS2812B LED strip DIN
#define LED_PIN 2

// KY-040 rotary encoder pins.
#define ENCODER_CLK 4
#define ENCODER_DT  5
#define ENCODER_SW  6

// MPU6050 I2C pins.
// These are defined manually so the ESP32-C3 uses the pins we expect.
#define I2C_SDA 8
#define I2C_SCL 9


// ==================================================
// LED SETUP
// ==================================================

// LED strip is 1m long with 160 LEDs.
#define NUM_LEDS 160

Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);


// ==================================================
// MPU6050 SETUP
// ==================================================

Adafruit_MPU6050 mpu;

// These store the accelerometer reading when the system starts.
// uses them as a simple baseline so the LEDs react to movement
// rather than the sensor's resting position.
float baseX = 0.0;
float baseY = 0.0;
float baseZ = 0.0;


// ==================================================
// BRIGHTNESS AND MODE SETTINGS
// ==================================================

// User-adjustable brightness from the rotary encoder.
// This acts like the maximum brightness limit.
int userBrightness = 80;

const int minBrightness = 0;
const int maxBrightness = 220;

// Current mode.
// Encoder button changes this.
int mode = 0;
const int numberOfModes = 4;


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
// HELPER: FADE BETWEEN TWO COLOURS
// ==================================================

// This blends between blue and orange.
// amount should be from 0.0 to 1.0.
// 0.0 = blue
// 1.0 = orange
uint32_t blueToOrange(float amount) {
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
// HELPER: SET ALL LEDS TO ONE COLOUR
// ==================================================

void setAllPixels(uint32_t colour, int brightness) {
  brightness = constrain(brightness, 0, 255);

  strip.setBrightness(brightness);

  for (int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, colour);
  }

  strip.show();
}


// ==================================================
// READ MPU6050 ACCELERATION
// ==================================================

void readAcceleration(float &xG, float &yG, float &zG, float &movementG) {
  sensors_event_t accel;
  sensors_event_t gyro;
  sensors_event_t temp;

  // Get latest sensor readings.
  mpu.getEvent(&accel, &gyro, &temp);

  // Subtract startup baseline and convert from m/s^2 to g.
  xG = (accel.acceleration.x - baseX) / 9.81;
  yG = (accel.acceleration.y - baseY) / 9.81;
  zG = (accel.acceleration.z - baseZ) / 9.81;

  // Overall movement amount.
  // This is useful for a simple bench test because it reacts to shaking,
  // tilting, and moving the sensor.
  movementG = sqrt((xG * xG) + (yG * yG) + (zG * zG));
}


// ==================================================
// UPDATE LEDS BASED ON MODE + ACCELERATION
// ==================================================

void updateLEDs() {
  float xG, yG, zG, movementG;
  readAcceleration(xG, yG, zG, movementG);

  // Convert movement into a 0.0 to 1.0 intensity value.
  // Around 0.00g = calm
  // Around 0.50g or more = strong movement
  float intensity = movementG / 0.50;
  intensity = constrain(intensity, 0.0, 1.0);

  // Reactive brightness.
  // At rest, brightness is around 25% of the user's brightness setting.
  // With movement, brightness increases up to the user's selected brightness.
  int reactiveBrightness = userBrightness * (0.25 + 0.75 * intensity);
  reactiveBrightness = constrain(reactiveBrightness, 0, userBrightness);

  switch (mode) {
    case 0:
      // Mode 0:
      // Blue to orange based on movement strength.
      // More movement = more orange and brighter.
      setAllPixels(blueToOrange(intensity), reactiveBrightness);
      break;

    case 1:
      // Mode 1:
      // Forward/back axis test using X axis.
      // Positive X = orange.
      // Negative X = red/purple.
      // Near zero = blue.
      if (xG > 0.08) {
        setAllPixels(strip.Color(255, 80, 0), reactiveBrightness);
      } else if (xG < -0.08) {
        setAllPixels(strip.Color(255, 0, 0), reactiveBrightness);
      } else {
        setAllPixels(strip.Color(0, 0, 255), reactiveBrightness);
      }
      break;

    case 2:
  // Mode 2:
  // Y-axis side-to-side test.
  //
  // This is mainly for checking the accelerometer's side/cornering axis.
  //
  // Y positive:
  //   green/turquoise
  //
  // Y negative:
  //   purple
  //
  // Y near zero:
  //   blue
  //
  // On the bench:
  //   tilt/move the MPU6050 left/right and the colour should change.
  //
  // In the car later:
  //   this kind of reading will be used for cornering effects.
  if (yG > 0.08) {
    setAllPixels(strip.Color(0, 255, 80), reactiveBrightness);
  } else if (yG < -0.08) {
    setAllPixels(strip.Color(120, 0, 255), reactiveBrightness);
  } else {
    setAllPixels(strip.Color(0, 0, 255), reactiveBrightness);
  }
  break;

    case 3:
      // Mode 3:
      // White brightness test.
      // Movement controls brightness only.
      setAllPixels(strip.Color(255, 255, 255), reactiveBrightness);
      break;
  }

  // Print readings occasionally so the Serial Monitor is readable.
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

  Serial.println("MR2 Reactive LEDs - Test 2");
  Serial.println("KY-040 + MPU6050 + WS2812B LED test");

  // Encoder setup.
  pinMode(ENCODER_CLK, INPUT_PULLUP);
  pinMode(ENCODER_DT, INPUT_PULLUP);
  pinMode(ENCODER_SW, INPUT_PULLUP);
  lastCLKState = digitalRead(ENCODER_CLK);

  // LED setup.
  strip.begin();
  strip.clear();
  strip.show();

  // I2C setup for ESP32-C3.
  Wire.begin(I2C_SDA, I2C_SCL);

  // MPU6050 setup.
  if (!mpu.begin()) {
    Serial.println("ERROR: MPU6050 not found.");
    Serial.println("Check VCC, GND, SDA, and SCL wiring.");

    // Flash red repeatedly if MPU6050 is not detected.
    while (1) {
      setAllPixels(strip.Color(255, 0, 0), 40);
      delay(300);
      setAllPixels(strip.Color(0, 0, 0), 0);
      delay(300);
    }
  }

  Serial.println("MPU6050 found.");

  // Set accelerometer range.
  // 4G is enough for this project and gives decent sensitivity.
  mpu.setAccelerometerRange(MPU6050_RANGE_4_G);

  // Set gyro range, even though this test mostly uses acceleration.
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);

  // Set filter bandwidth to smooth readings slightly.
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

  // Small delay keeps updates smooth and avoids spamming the sensor.
  delay(20);
}