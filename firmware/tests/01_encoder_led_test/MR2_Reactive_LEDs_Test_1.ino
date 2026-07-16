#include <Adafruit_NeoPixel.h>

// --------------------------------------------------
// MR2 Reactive LEDs - Test 1
// KY-040 rotary encoder + WS2812B LED test
//
// Purpose of this test:
// 1. Confirm the ESP32-C3 can control a WS2812B LED strip.
// 2. Confirm the LED data signal works through the SN74AHCT125N level shifter.
// 3. Confirm the KY-040 rotary encoder can adjust brightness.
// 4. Confirm the KY-040 push button can change LED modes.
//
// This test does NOT use the MPU6050 accelerometer yet.
// This test uses ONE 1m LED strip.
// --------------------------------------------------


// ==================================================
// PIN ASSIGNMENTS
// ==================================================

// This ESP32-C3 pin sends LED data.
// Wiring path:
// ESP32-C3 GPIO 2
// → SN74AHCT125N input
// → SN74AHCT125N output
// → 330 ohm resistor
// → WS2812B LED strip DIN
#define LED_PIN 2

// KY-040 rotary encoder pins.
// CLK and DT are used to detect rotation direction.
// SW is the push button built into the encoder.
#define ENCODER_CLK 4
#define ENCODER_DT  5
#define ENCODER_SW  6


// ==================================================
// LED SETUP
// ==================================================

// LED strip is 1m long with 160 LEDs.
// For this test, we are using one full strip.
#define NUM_LEDS 160

// Create the LED strip object.
// NEO_GRB is the colour order used by most WS2812B strips.
// NEO_KHZ800 is the standard WS2812B data speed.
Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);


// ==================================================
// BRIGHTNESS AND MODE SETTINGS
// ==================================================

// Starting brightness.
// 40 is low enough to be safe for first testing.
int brightness = 40;

// Minimum and maximum brightness limits.
// Maximum is kept below 255 so the strip does not run at absolute full power
// during early testing.
const int minBrightness = 0;
const int maxBrightness = 180;

// Current LED mode.
// Pressing the encoder button moves to the next mode.
int mode = 0;

// Total number of LED modes in this test.
const int numberOfModes = 4;


// ==================================================
// ROTARY ENCODER VARIABLES
// ==================================================

// Stores the previous CLK pin state so we can detect when the encoder turns.
int lastCLKState;

// Used to debounce the encoder push button.
// Without this, one press can be detected multiple times.
unsigned long lastButtonPress = 0;
const unsigned long debounceDelay = 250;


// ==================================================
// SET ALL LEDS TO ONE COLOUR
// ==================================================

void setAllPixels(uint32_t colour) {
  // Apply the current brightness setting.
  strip.setBrightness(brightness);

  // Set every LED in the strip to the same colour.
  for (int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, colour);
  }

  // Send the updated colour data to the LED strip.
  strip.show();
}


// ==================================================
// UPDATE LEDS BASED ON CURRENT MODE
// ==================================================

void updateLEDs() {
  // If brightness is zero, turn the strip off.
  if (brightness <= 0) {
    setAllPixels(strip.Color(0, 0, 0));
    return;
  }

  // Choose LED colour depending on the current mode.
  switch (mode) {
    case 0:
      // Mode 0: Blue ambient glow.
      setAllPixels(strip.Color(0, 0, 255));
      break;

    case 1:
      // Mode 1: Orange.
      // This is close to the colour we may use later for acceleration.
      setAllPixels(strip.Color(255, 80, 0));
      break;

    case 2:
      // Mode 2: Purple.
      // This gives a middle colour between blue and orange.
      setAllPixels(strip.Color(120, 0, 255));
      break;

    case 3:
      // Mode 3: White test mode.
      // Useful for checking that all red, green, and blue LED channels work.
      // Keep brightness limited during this mode because white uses the most current.
      setAllPixels(strip.Color(255, 255, 255));
      break;
  }
}


// ==================================================
// READ ROTARY ENCODER ROTATION
// ==================================================

void readEncoderRotation() {
  // Read current state of CLK pin.
  int currentCLKState = digitalRead(ENCODER_CLK);

  // If CLK changed, the encoder has moved.
  if (currentCLKState != lastCLKState) {

    // Compare DT with CLK to determine direction.
    if (digitalRead(ENCODER_DT) != currentCLKState) {
      // One direction: increase brightness.
      brightness += 5;
    } else {
      // Other direction: decrease brightness.
      brightness -= 5;
    }

    // Keep brightness inside safe limits.
    brightness = constrain(brightness, minBrightness, maxBrightness);

    // Print brightness to Serial Monitor for debugging.
    Serial.print("Brightness: ");
    Serial.println(brightness);

    // Apply new brightness to LEDs.
    updateLEDs();
  }

  // Store CLK state for next loop.
  lastCLKState = currentCLKState;
}


// ==================================================
// READ ROTARY ENCODER BUTTON
// ==================================================

void readEncoderButton() {
  // KY-040 button usually reads LOW when pressed.
  if (digitalRead(ENCODER_SW) == LOW) {
    unsigned long now = millis();

    // Debounce check.
    // Only accept a press if enough time has passed since the last press.
    if (now - lastButtonPress > debounceDelay) {

      // Move to the next mode.
      mode++;

      // If we go past the last mode, return to mode 0.
      if (mode >= numberOfModes) {
        mode = 0;
      }

      // Print current mode to Serial Monitor.
      Serial.print("Mode: ");
      Serial.println(mode);

      // Apply new mode to LEDs.
      updateLEDs();

      // Save button press time.
      lastButtonPress = now;
    }
  }
}


// ==================================================
// SETUP
// ==================================================

void setup() {
  // Start Serial Monitor for debugging.
  Serial.begin(115200);
  delay(500);

  Serial.println("MR2 Reactive LEDs - Test 1");
  Serial.println("KY-040 rotary encoder + WS2812B LED test");
  Serial.println("Using one 160 LED strip.");

  // Set encoder pins as inputs with internal pull-up resistors.
  // This means the pins normally read HIGH and go LOW when connected to ground.
  pinMode(ENCODER_CLK, INPUT_PULLUP);
  pinMode(ENCODER_DT, INPUT_PULLUP);
  pinMode(ENCODER_SW, INPUT_PULLUP);

  // Store initial encoder CLK state.
  lastCLKState = digitalRead(ENCODER_CLK);

  // Start LED strip.
  strip.begin();

  // Clear all LEDs at startup.
  strip.clear();
  strip.show();

  // Show initial LED mode.
  updateLEDs();

  Serial.println("Setup complete.");
}


// ==================================================
// MAIN LOOP
// ==================================================

void loop() {
  // Continuously check if the encoder has turned.
  readEncoderRotation();

  // Continuously check if the encoder button has been pressed.
  readEncoderButton();
}