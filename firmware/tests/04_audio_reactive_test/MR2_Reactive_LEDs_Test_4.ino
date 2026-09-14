#include <Adafruit_NeoPixel.h>

// --------------------------------------------------
// MR2 Reactive LEDs - Test 4
// Audio-reactive LED test (bench only)
//
// Hardware used:
// - Full ESP32 dev board (NOT the ESP32-C3 installed in the car)
// - Audio conditioning circuit (see docs/audio-reactive-led-plan.md)
// - One WS2812B LED strip
//
// IMPORTANT: this test runs on a full ESP32 dev board, not the
// ESP32-C3 that's already installed as the production controller.
// GPIO34 in particular does NOT exist on the ESP32-C3 -- do not
// reuse these pin numbers when this feature eventually moves into
// the real firmware (Phase 4). This board is bench-only and is
// never installed in the car.
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
// GPIO34 is input-only and sits on ADC1, which avoids the
// conflicts ADC2 can have with WiFi on the ESP32. Input-only
// also means no internal pull-up/pull-down is available on this
// pin, which is fine here since the conditioning circuit's own
// bias network (R5/R6) already sets the resting voltage.
#define AUDIO_PIN 34

// LED data pin, into the strip's DIN directly (or through a
// level shifter if you're using one on the bench -- the main
// firmware always does, this test may or may not depending on
// what you've got wired up).
#define LED_PIN 5


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

  // Cyan test colour -- deliberately different from any colour
  // used by the main firmware (blue/orange/red), so it's obvious
  // on the bench which system is driving the strip.
  setAllPixels(0, 200, 200, brightness);

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
  Serial.println("Audio-reactive LED test (bench only, full ESP32 dev board)");
  Serial.println("Not the production ESP32-C3 -- pin numbers here do not carry over.");

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
