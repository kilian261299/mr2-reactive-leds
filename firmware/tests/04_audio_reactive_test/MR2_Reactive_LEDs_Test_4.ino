#include <Adafruit_NeoPixel.h>

// --------------------------------------------------
// MR2 Reactive LEDs - Test 4
// Audio-reactive LED test (bench only)
//
// Hardware used:
// - A SPARE ESP32-C3 module (NOT the one installed in the car)
// - Audio conditioning circuit (see docs/audio-reactive-led-plan.md)
// - One WS2812B LED strip
//
// IMPORTANT: this is a separate, spare ESP32-C3 module for bench
// testing only -- it is never installed in the car, and is not the
// production controller. It happens to be the same CHIP as the
// production board though, which is why AUDIO_PIN below is the
// same GPIO1 the production plan already uses: no pin remapping
// needed when this moves into the real firmware in Phase 4.
// LED_PIN (GPIO7) is arbitrary -- this bench board doesn't share a
// bus with anything else, so any free, non-strapping pin works.
//
// Purpose of this test:
// 1. Confirm the conditioning circuit produces a usable 0-3.3V
//    envelope on an ADC pin from a real audio source.
// 2. Smooth that envelope in firmware and map it to LED brightness.
// 3. Give a live Serial readout of raw vs smoothed values, so the
//    conditioning circuit (R3/R4 divider, C1 smoothing cap) can be
//    tuned by watching real numbers, not just by eye on the LED.
// 4. Prove the pipeline end-to-end before any of this touches the
//    real car firmware.
//
// This test does NOT read the MPU6050 or the rotary encoder.
// This test does NOT drive two strips -- one only, for simplicity.
// --------------------------------------------------


// ==================================================
// PIN ASSIGNMENTS
// ==================================================

// Conditioning circuit output (Node B) into the ADC.
// GPIO1 is ADC1_CH1 on the ESP32-C3 -- stick to ADC1 pins (GPIO0-4)
// rather than ADC2, which is best avoided on the C3 regardless of
// WiFi use. Same pin the production plan uses on the real board,
// so this test's tuning carries straight over.
#define AUDIO_PIN 1

// LED data pin, into the strip's DIN directly (or through a
// level shifter if you're using one on the bench -- the main
// firmware always does, this test may or may not depending on
// what you've got wired up). GPIO7 is free and not a strapping
// pin -- confirm against your specific board's silkscreen.
#define LED_PIN 7


// ==================================================
// LED SETUP
// ==================================================

// Set this to the actual length of whatever test strip you're
// using on the bench. Not tied to the production strip lengths.
#define NUM_LEDS 30

Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);


// ==================================================
// AUDIO ENVELOPE SETTINGS
// ==================================================

// ESP32's analogRead() returns 0-4095 (12-bit) by default.
//
// audioFloor: the raw ADC reading with no audio playing (just the
// conditioning circuit's bias network resting voltage). Below this,
// treat it as silence.
//
// audioCeiling: the raw ADC reading during loud audio. Above this,
// treat it as maximum brightness.
//
// Both of these are placeholders -- watch the Serial output with
// real audio (Phase 2, Stage A) and set these to what you actually
// see, not what's guessed here.
int audioFloor = 200;
int audioCeiling = 3000;

// Exponential smoothing rate for the envelope, same style of
// smoothing already used throughout the main v2.x firmware
// (see gravitySmoothing / accelSmoothing there for reference).
// Higher = more responsive but jumpier; lower = smoother but
// laggier behind the actual music.
float audioSmoothing = 0.15;

float smoothedAudio = 0.0;


// ==================================================
// USER-ADJUSTABLE BRIGHTNESS CAP
// ==================================================

// Keeps the strip from ever running at full 255 during bench
// testing, same reasoning as the encoder tests before it.
const int maxBrightness = 180;


// ==================================================
// SERIAL DEBUG TIMING
// ==================================================

unsigned long lastSerialPrint = 0;
const unsigned long serialPrintInterval = 100;


// ==================================================
// SET ALL LEDS TO ONE COLOUR AT A GIVEN BRIGHTNESS
// ==================================================

void setAllPixels(uint8_t r, uint8_t g, uint8_t b, int brightness) {
  strip.setBrightness(brightness);

  uint32_t colour = strip.Color(r, g, b);

  for (int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, colour);
  }

  strip.show();
}


// ==================================================
// READ AND SMOOTH THE AUDIO ENVELOPE
// ==================================================

void updateAudioReactiveLEDs() {
  int rawAudio = analogRead(AUDIO_PIN);

  // Low-pass filter the raw reading, same exponential-smoothing
  // approach used elsewhere in this project.
  smoothedAudio = (smoothedAudio * (1.0 - audioSmoothing)) + (rawAudio * audioSmoothing);

  // Map the smoothed reading onto 0-1 using the floor/ceiling
  // above, then clamp -- constrain() alone won't reorder floor
  // and ceiling for you, so keep audioCeiling > audioFloor.
  float level = (smoothedAudio - audioFloor) / (float)(audioCeiling - audioFloor);
  level = constrain(level, 0.0, 1.0);

  int brightness = (int)(level * maxBrightness);

  // Dark cyan-blue test colour -- deliberately different from any
  // colour used by the main firmware (blue/orange/red), so it's
  // obvious on the bench which system is driving the strip. More
  // blue than green and capped well under 255 so it reads as a
  // deep cyan-blue rather than a bright neon cyan, even before
  // the brightness scaling below darkens it further. Adjust the
  // ratio here if you want more green (more cyan) or more blue.
  setAllPixels(0, 90, 160, brightness);

  // Print raw + smoothed values regularly so the conditioning
  // circuit (R3/R4, C1) can be tuned against real numbers, per
  // Phase 2 Stage A/B of the build plan.
  unsigned long now = millis();
  if (now - lastSerialPrint > serialPrintInterval) {
    Serial.print("Raw: ");
    Serial.print(rawAudio);
    Serial.print(" | Smoothed: ");
    Serial.print(smoothedAudio, 1);
    Serial.print(" | Level: ");
    Serial.print(level, 2);
    Serial.print(" | Brightness: ");
    Serial.println(brightness);

    lastSerialPrint = now;
  }
}


// ==================================================
// SETUP
// ==================================================

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println("MR2 Reactive LEDs - Test 4");
  Serial.println("Audio-reactive LED test (bench only, spare ESP32-C3 module)");
  Serial.println("Not the ESP32-C3 installed in the car -- this board is bench-only.");

  // No pinMode() call needed for an ADC read on the ESP32 --
  // analogRead() configures the pin itself.

  strip.begin();
  strip.clear();
  strip.show();

  Serial.println("Setup complete. Play audio into the conditioning circuit input.");
}


// ==================================================
// MAIN LOOP
// ==================================================

void loop() {
  updateAudioReactiveLEDs();
}
