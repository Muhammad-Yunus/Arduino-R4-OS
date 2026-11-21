#include <Arduino.h>

#include <Adafruit_ILI9341.h>
#include <SPI.h>

#include "AppLauncher.h"
#include "TouchInput.h"

namespace {
constexpr uint8_t kTftCs = 10;
constexpr uint8_t kTftDc = 6;
constexpr uint8_t kTftBacklight = 7;
constexpr uint8_t kSdCs = 4;

constexpr uint8_t kTouchYm = A0;
constexpr uint8_t kTouchXm = A1;
constexpr uint8_t kTouchYp = A2;
constexpr uint8_t kTouchXp = A3;
constexpr uint16_t kTouchPlateResistance = 300;

Adafruit_ILI9341 display(kTftCs, kTftDc, -1);
drawing::TouchInput touch(kTouchXp, kTouchYp, kTouchXm, kTouchYm,
                          kTouchPlateResistance);
drawing::AppLauncher app(display, touch, kSdCs, kTftCs);
}

void setup() {
  pinMode(kSdCs, OUTPUT);
  digitalWrite(kSdCs, HIGH);
  pinMode(kTftBacklight, OUTPUT);
  digitalWrite(kTftBacklight, HIGH);

  SPI.begin();
  // Run the TFT faster to shorten redraws; the SD driver selects its own SPI
  // transaction clock when accessing the shared bus.
  display.begin(32000000UL);
  display.setRotation(1);
  display.setTextWrap(false);

  app.begin();
}

void loop() {
  app.update();
}
