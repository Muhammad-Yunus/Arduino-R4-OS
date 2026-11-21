#include "TouchInput.h"

#include <Arduino.h>
#include <math.h>

namespace drawing {
namespace {
constexpr int kMinPressure = 10;
constexpr int kMaxPressure = 1000;
constexpr float kMinimumCalibrationDeterminant = 1000.0f;
}  // namespace

TouchInput::TouchInput(uint8_t xp, uint8_t yp, uint8_t xm, uint8_t ym,
                       uint16_t plateResistance)
    : _touchScreen(xp, yp, xm, ym, plateResistance), _xm(xm), _yp(yp) {}

TSPoint TouchInput::read() {
  const TSPoint point = _touchScreen.getPoint();

  pinMode(_xm, OUTPUT);
  pinMode(_yp, OUTPUT);

  return point;
}

bool TouchInput::isPressed(const TSPoint &point) {
  return point.z > kMinPressure && point.z < kMaxPressure;
}

bool TouchInput::calibrate(
    const std::array<TSPoint, CalibrationPointCount> &rawPoints,
    const std::array<ScreenPoint, CalibrationPointCount> &screenPoints) {
  _rawOrigin = rawPoints[0];
  _rawVxX = rawPoints[1].x - rawPoints[0].x;
  _rawVxY = rawPoints[1].y - rawPoints[0].y;
  _rawVyX = rawPoints[2].x - rawPoints[0].x;
  _rawVyY = rawPoints[2].y - rawPoints[0].y;
  _determinant = _rawVxX * _rawVyY - _rawVyX * _rawVxY;

  if (fabsf(_determinant) <= kMinimumCalibrationDeterminant) {
    _isCalibrated = false;
    return false;
  }

  _screenPoints = screenPoints;
  _isCalibrated = true;
  return true;
}

bool TouchInput::mapToScreen(const TSPoint &rawPoint, int16_t screenWidth,
                             int16_t screenHeight,
                             ScreenPoint &screenPoint) const {
  if (!_isCalibrated || screenWidth <= 0 || screenHeight <= 0) {
    return false;
  }

  const float rawDeltaX = rawPoint.x - _rawOrigin.x;
  const float rawDeltaY = rawPoint.y - _rawOrigin.y;
  // Invert the two raw calibration vectors to map a touch into screen space.
  const float alongScreenX =
      (rawDeltaX * _rawVyY - _rawVyX * rawDeltaY) / _determinant;
  const float alongScreenY =
      (_rawVxX * rawDeltaY - rawDeltaX * _rawVxY) / _determinant;

  const float mappedX =
      _screenPoints[0].x +
      alongScreenX * (_screenPoints[1].x - _screenPoints[0].x) +
      alongScreenY * (_screenPoints[2].x - _screenPoints[0].x);
  const float mappedY =
      _screenPoints[0].y +
      alongScreenX * (_screenPoints[1].y - _screenPoints[0].y) +
      alongScreenY * (_screenPoints[2].y - _screenPoints[0].y);

  const long roundedX = lroundf(mappedX);
  const long roundedY = lroundf(mappedY);
  screenPoint.x = roundedX < 0 ? 0 :
                  roundedX >= screenWidth ? screenWidth - 1 : roundedX;
  screenPoint.y = roundedY < 0 ? 0 :
                  roundedY >= screenHeight ? screenHeight - 1 : roundedY;
  return true;
}

}  // namespace drawing
