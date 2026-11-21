#pragma once

#include <stdint.h>

#include <Adafruit_GFX.h>

#include "TouchInput.h"

namespace drawing {

class DrawingApp final {
 public:
  DrawingApp(Adafruit_GFX &display, TouchInput &touch);

  void begin();
  void update();
  bool consumeExitRequested();

 private:
  void drawToolbar();
  void drawCanvas();
  void handleDrawing(const TSPoint &point, bool pressed);
  void handleToolbarTouch(int16_t x);
  int16_t clearButtonX() const;
  int16_t colorPreviewX() const;

  Adafruit_GFX &_display;
  TouchInput &_touch;
  uint8_t _selectedPaletteIndex = 0;
  bool _penDown = false;
  bool _strokeActive = false;
  bool _exitRequested = false;
  int16_t _lastX = 0;
  int16_t _lastY = 0;
  int16_t _lastDabX = 0;
  int16_t _lastDabY = 0;
};

}  // namespace drawing
