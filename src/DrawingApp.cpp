#include "DrawingApp.h"

#include <Arduino.h>

namespace drawing {
namespace {
constexpr uint8_t kHeaderHeight = 26;
constexpr uint8_t kToolbarHeight = 38;
constexpr uint8_t kPaletteCount = 6;
constexpr uint8_t kPaletteStepX = 40;
constexpr uint16_t kPaletteAreaWidth = kPaletteCount * kPaletteStepX;

constexpr uint8_t kClearButtonY = 4;
constexpr uint8_t kClearButtonWidth = 52;
constexpr uint8_t kClearButtonHeight = 18;
constexpr int32_t kBrushDabSpacingSquared = 16;

constexpr uint16_t kBlack = 0x0000;
constexpr uint16_t kNavy = 0x000F;
constexpr uint16_t kDarkGrey = 0x7BEF;
constexpr uint16_t kBlue = 0x001F;
constexpr uint16_t kGreen = 0x07E0;
constexpr uint16_t kCyan = 0x07FF;
constexpr uint16_t kRed = 0xF800;
constexpr uint16_t kYellow = 0xFFE0;
constexpr uint16_t kWhite = 0xFFFF;

const uint16_t kPalette[kPaletteCount] = {
    kBlack, kRed, kGreen, kBlue, kYellow, kWhite};
}  // namespace

DrawingApp::DrawingApp(Adafruit_GFX &display, TouchInput &touch)
    : _display(display), _touch(touch) {}

void DrawingApp::begin() {
  _selectedPaletteIndex = 0;
  _penDown = false;
  _strokeActive = false;
  _exitRequested = false;
  drawCanvas();
}

void DrawingApp::update() {
  const TSPoint point = _touch.read();
  handleDrawing(point, TouchInput::isPressed(point));
  delay(1);
}

void DrawingApp::drawToolbar() {
  const int16_t toolbarY = _display.height() - kToolbarHeight;
  _display.fillRect(0, toolbarY, _display.width(), kToolbarHeight,
                    kDarkGrey);

  for (uint8_t i = 0; i < kPaletteCount; ++i) {
    const int16_t x = i * kPaletteStepX;
    const int16_t y = toolbarY + 5;
    _display.fillRect(x + 6, y + 2, 28, 26, kPalette[i]);
    _display.drawRect(x + 1, y - 1, kPaletteStepX - 2, kToolbarHeight - 2,
                      i == _selectedPaletteIndex ? kCyan : kBlack);
  }
}

void DrawingApp::drawCanvas() {
  _display.fillScreen(kWhite);
  _display.fillRect(0, 0, _display.width(), kHeaderHeight, kNavy);
  _display.setTextColor(kWhite);
  _display.setTextSize(1);
  _display.fillRoundRect(4, 4, 44, 18, 3, kWhite);
  _display.setTextColor(kNavy);
  _display.setCursor(13, 10);
  _display.print("HOME");
  _display.setTextColor(kWhite);
  _display.setCursor(55, 9);
  _display.print("FREE DRAW - choose color below");

  _display.fillRect(colorPreviewX(), 4, 18, 18,
                    kPalette[_selectedPaletteIndex]);
  _display.drawRect(colorPreviewX() - 1, 3, 20, 20, kWhite);
  _display.fillRoundRect(clearButtonX(), kClearButtonY, kClearButtonWidth,
                         kClearButtonHeight, 3, kRed);
  _display.setCursor(clearButtonX() + 11, kClearButtonY + 6);
  _display.print("CLEAR");
  drawToolbar();
}

void DrawingApp::handleToolbarTouch(int16_t x) {
  if (x < 0 || x >= kPaletteAreaWidth) {
    return;
  }

  const uint8_t index = x / kPaletteStepX;
  if (_selectedPaletteIndex == index) {
    return;
  }

  _selectedPaletteIndex = index;
  _display.fillRect(colorPreviewX(), 4, 18, 18, kPalette[index]);
  _display.drawRect(colorPreviewX() - 1, 3, 20, 20, kWhite);
  drawToolbar();
}

int16_t DrawingApp::clearButtonX() const {
  return _display.width() - kClearButtonWidth - 4;
}

int16_t DrawingApp::colorPreviewX() const {
  return clearButtonX() - 28;
}

bool DrawingApp::consumeExitRequested() {
  const bool requested = _exitRequested;
  _exitRequested = false;
  return requested;
}

void DrawingApp::handleDrawing(const TSPoint &point, bool pressed) {
  if (!pressed) {
    _penDown = false;
    _strokeActive = false;
    return;
  }

  ScreenPoint mappedPoint{};
  if (!_touch.mapToScreen(point, _display.width(), _display.height(),
                          mappedPoint)) {
    return;
  }

  const int16_t toolbarY = _display.height() - kToolbarHeight;
  if (mappedPoint.y >= toolbarY) {
    handleToolbarTouch(mappedPoint.x);
    _penDown = true;
    _strokeActive = false;
    return;
  }

  if (mappedPoint.y < kHeaderHeight) {
    if (!_penDown && mappedPoint.x < 52) {
      _exitRequested = true;
    } else if (!_penDown && mappedPoint.x >= clearButtonX() &&
        mappedPoint.x < clearButtonX() + kClearButtonWidth &&
        mappedPoint.y >= kClearButtonY &&
        mappedPoint.y < kClearButtonY + kClearButtonHeight) {
      drawCanvas();
    }
    _penDown = true;
    _strokeActive = false;
    return;
  }

  if (!_strokeActive) {
    _display.fillCircle(mappedPoint.x, mappedPoint.y, 2,
                        kPalette[_selectedPaletteIndex]);
    _lastDabX = mappedPoint.x;
    _lastDabY = mappedPoint.y;
    _strokeActive = true;
  } else {
    _display.drawLine(_lastX, _lastY, mappedPoint.x, mappedPoint.y,
                      kPalette[_selectedPaletteIndex]);

    const int32_t dabDx = mappedPoint.x - _lastDabX;
    const int32_t dabDy = mappedPoint.y - _lastDabY;
    if (dabDx * dabDx + dabDy * dabDy >= kBrushDabSpacingSquared) {
      _display.fillCircle(mappedPoint.x, mappedPoint.y, 2,
                          kPalette[_selectedPaletteIndex]);
      _lastDabX = mappedPoint.x;
      _lastDabY = mappedPoint.y;
    }
  }

  _lastX = mappedPoint.x;
  _lastY = mappedPoint.y;
  _penDown = true;
}

}  // namespace drawing
