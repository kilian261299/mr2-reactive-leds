#include <Adafruit_NeoPixel.h>

// --------------------------------------------------
// MR2 Reactive LEDs - Test 4
// Audio-reactive LED bar visualizer test (bench only)
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
// 2. Smooth that envelope in firmware and map it to a growing/
//    shrinking bar of lit LEDs, with a bouncing peak-hold marker --
//    a level-meter/visualizer look, not just uniform brightness.
// 3. Give a live Serial readout of raw vs smoothed values, so the
//    conditioning circuit (R1/R2, C1 smoothing cap) can be tuned
//    by watching real numbers, not just by eye on the LED.
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
#define NUM_LEDS 60

Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);


// ==================================================
// AUDIO ENVELOPE SETTINGS
// ==================================================

// ESP32's analogRead() returns 0-4095 (12-bit) by default.
//
// audioFloor: the raw ADC reading with no audio playing (just the
// conditioning circuit's resting voltage). Below this, treat it as
// silence.
//
// audioCeiling: the raw ADC reading during loud audio. Above this,
// treat it as a full bar.
//
// Both of these were placeholders -- now set from real Serial
// readings on the bench (Phase 2, Stage A): idle settled around
// 950, loud content peaked around 1200. Floor/ceiling are set with
// a bit of margin either side of those two observed points (rather
// than snugly on top of them) so a quieter idle moment doesn't
// clip the bar to fully-off, and a louder transient than what was
// captured doesn't clip it to fully-on.
int audioFloor = 900;
int audioCeiling = 1300;

// Asymmetric smoothing rather than one single rate: a fast attack
// lets the bar jump up quickly on a transient, a slow release lets
// it sink back down gradually -- that's what gives the "bounce"
// look rather than a mushy blob that just fades in and out.
float attackSmoothing = 0.5;
float releaseSmoothing = 0.05;

float smoothedAudio = 0.0;

// Peak-hold marker: a single pixel riding ahead of the bar that
// snaps up instantly whenever the bar catches up to or passes it,
// then falls back down under its own slower "gravity" -- the
// classic VU-meter peak indicator. Speed is in LEDs per second, so
// the fall looks the same regardless of how fast the main loop runs.
float peakPositionLEDs = 0.0;
const float peakFallSpeed = 15.0;
unsigned long lastPeakUpdate = 0;


// ==================================================
// USER-ADJUSTABLE BRIGHTNESS CAP
// ==================================================

// A fixed ceiling on how bright any lit pixel can be -- unrelated
// to audio level now, since the audio level controls how many
// LEDs are lit, not how bright they are. Same reasoning as the
// encoder tests before it: keeps the bench strip comfortable to
// look at, never runs at full 255.
const int maxBrightness = 180;


// ==================================================
// SERIAL DEBUG TIMING
// ==================================================

unsigned long lastSerialPrint = 0;
const unsigned long serialPrintInterval = 100;


// ==================================================
// UPDATE THE PEAK-HOLD MARKER'S POSITION
// ==================================================

void updatePeak(float barHeightLEDs) {
  unsigned long now = millis();
  float deltaSeconds = (now - lastPeakUpdate) / 1000.0;
  lastPeakUpdate = now;

  if (barHeightLEDs >= peakPositionLEDs) {
    // Bar has caught up to (or passed) the peak -- snap the peak
    // up to match, no lag on the way up.
    peakPositionLEDs = barHeightLEDs;
  } else {
    peakPositionLEDs -= peakFallSpeed * deltaSeconds;
    // Never let the peak marker fall below the bar itself -- it
    // rides on top of/ahead of the bar, not behind it.
    if (peakPositionLEDs < barHeightLEDs) {
      peakPositionLEDs = barHeightLEDs;
    }
  }
}


// ==================================================
// DRAW THE BAR + PEAK MARKER
// ==================================================

void drawVisualizer(float barHeightLEDs) {
  strip.clear();

  // Dark cyan-blue bar colour -- deliberately different from any
  // colour used by the main firmware (blue/orange/red), so it's
  // obvious on the bench which system is driving the strip.
  const uint8_t barR = 0;
  const uint8_t barG = 90;
  const uint8_t barB = 160;

  int fullLit = (int)barHeightLEDs;
  float fraction = barHeightLEDs - fullLit;

  for (int i = 0; i < fullLit && i < NUM_LEDS; i++) {
    strip.setPixelColor(i, strip.Color(barR, barG, barB));
  }

  // The LED right at the top of the bar gets a partial (fractional)
  // brightness instead of a hard on/off cutoff -- gives noticeably
  // smoother sub-pixel resolution with only NUM_LEDS steps to work with.
  if (fullLit < NUM_LEDS) {
    strip.setPixelColor(fullLit, strip.Color(
      (uint8_t)(barR * fraction),
      (uint8_t)(barG * fraction),
      (uint8_t)(barB * fraction)
    ));
  }

  // Peak marker in white, so it stands out against the bar colour.
  int peakPixel = (int)peakPositionLEDs;
  if (peakPixel >= 0 && peakPixel < NUM_LEDS) {
    strip.setPixelColor(peakPixel, strip.Color(255, 255, 255));
  }

  strip.show();
}


// ==================================================
// READ AND SMOOTH THE AUDIO ENVELOPE
// ==================================================

void updateAudioReactiveLEDs() {
  int rawAudio = analogRead(AUDIO_PIN);

  // Rising faster than falling is what gives the bounce its shape --
  // see the attackSmoothing/releaseSmoothing note above.
  float rate = (rawAudio > smoothedAudio) ? attackSmoothing : releaseSmoothing;
  smoothedAudio = (smoothedAudio * (1.0 - rate)) + (rawAudio * rate);

  // Map the smoothed reading onto 0-1 using the floor/ceiling
  // above, then clamp -- constrain() alone won't reorder floor
  // and ceiling for you, so keep audioCeiling > audioFloor.
  float level = (smoothedAudio - audioFloor) / (float)(audioCeiling - audioFloor);
  level = constrain(level, 0.0, 1.0);

  float barHeightLEDs = level * NUM_LEDS;

  updatePeak(barHeightLEDs);
  drawVisualizer(barHeightLEDs);

  // Print raw + smoothed values regularly so the conditioning
  // circuit (R1/R2, C1) can be tuned against real numbers, per
  // Phase 2 Stage A of the build plan.
  unsigned long now = millis();
  if (now - lastSerialPrint > serialPrintInterval) {
    Serial.print("Raw: ");
    Serial.print(rawAudio);
    Serial.print(" | Smoothed: ");
    Serial.print(smoothedAudio, 1);
    Serial.print(" | Level: ");
    Serial.print(level, 2);
    Serial.print(" | Bar LEDs: ");
    Serial.print(barHeightLEDs, 1);
    Serial.print(" | Peak: ");
    Serial.println(peakPositionLEDs, 1);

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
  Serial.println("Audio-reactive LED bar visualizer test (bench only, spare ESP32-C3 module)");
  Serial.println("Not the ESP32-C3 installed in the car -- this board is bench-only.");

  // No pinMode() call needed for an ADC read on the ESP32 --
  // analogRead() configures the pin itself.

  strip.begin();
  strip.setBrightness(maxBrightness);
  strip.clear();
  strip.show();

  lastPeakUpdate = millis();

  Serial.println("Setup complete. Play audio into the conditioning circuit input.");
}


// ==================================================
// MAIN LOOP
// ==================================================

void loop() {
  updateAudioReactiveLEDs();
}
