#pragma once

#include <array>
#include <stdint.h>

#include <TouchScreen.h>

namespace drawing {

struct ScreenPoint {
  int16_t x;
  int16_t y;
};

class TouchInput final {
 public:
  static constexpr uint8_t CalibrationPointCount = 3;

  TouchInput(uint8_t xp, uint8_t yp, uint8_t xm, uint8_t ym,
             uint16_t plateResistance);

  TSPoint read();
  static bool isPressed(const TSPoint &point);

  bool calibrate(
      const std::array<TSPoint, CalibrationPointCount> &rawPoints,
      const std::array<ScreenPoint, CalibrationPointCount> &screenPoints);
  bool mapToScreen(const TSPoint &rawPoint, int16_t screenWidth,
                   int16_t screenHeight, ScreenPoint &screenPoint) const;

 private:
  TouchScreen _touchScreen;
  uint8_t _xm;
  uint8_t _yp;
  bool _isCalibrated = false;
  TSPoint _rawOrigin;
  std::array<ScreenPoint, CalibrationPointCount> _screenPoints{};
  float _rawVxX = 0.0f;
  float _rawVxY = 0.0f;
  float _rawVyX = 0.0f;
  float _rawVyY = 0.0f;
  float _determinant = 0.0f;
};

}  // namespace drawing
