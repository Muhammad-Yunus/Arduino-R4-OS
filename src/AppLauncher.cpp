#include "AppLauncher.h"

#include <Arduino.h>
#include <EEPROM.h>
#include <string.h>

namespace drawing {
namespace {
constexpr uint8_t kCalibrationSampleCount = 8;
constexpr uint8_t kCalibrationReleaseSampleCount = 12;
constexpr int16_t kHeaderHeight = 32;
constexpr uint16_t kWhite = 0xFFFF;
constexpr uint16_t kExplorerBackground = 0xFFFF;
constexpr uint16_t kExplorerToolbar = 0x2A91;
constexpr uint16_t kExplorerHeader = 0xEF7D;
constexpr uint16_t kExplorerAlternate = 0xF7BE;
constexpr uint16_t kExplorerLine = 0xCE79;
constexpr uint16_t kExplorerText = 0x2104;
constexpr uint16_t kExplorerSubtleText = 0x5AEB;
constexpr uint16_t kFolderBody = 0xFDC0;
constexpr uint16_t kFolderTab = 0xFFE0;
constexpr uint16_t kFolderOutline = 0xB5A0;
constexpr uint16_t kAppAccent = 0x2A91;
constexpr uint32_t kCalibrationMagic = 0x5443414C;
constexpr uint16_t kCalibrationVersion = 1;
constexpr int kCalibrationEepromAddress = 0;
constexpr uint32_t kSplashDurationMs = 2500;
constexpr uint32_t kSplashFrameIntervalMs = 90;

const std::array<ScreenPoint, TouchInput::CalibrationPointCount>
    kCalibrationTargets = {{{22, 84}, {298, 84}, {22, 218}}};

struct CalibrationRecord {
  // Version and checksum keep erased or incompatible EEPROM data from being
  // mistaken for a usable touch calibration.
  uint32_t magic;
  uint16_t version;
  int16_t rawX[TouchInput::CalibrationPointCount];
  int16_t rawY[TouchInput::CalibrationPointCount];
  int16_t rawZ[TouchInput::CalibrationPointCount];
  uint32_t checksum;
};

void addChecksumByte(uint32_t &checksum, uint8_t value) {
  checksum ^= value;
  checksum *= 16777619UL;
}

void addChecksumWord(uint32_t &checksum, uint16_t value) {
  addChecksumByte(checksum, static_cast<uint8_t>(value));
  addChecksumByte(checksum, static_cast<uint8_t>(value >> 8));
}

uint32_t calibrationChecksum(const CalibrationRecord &record) {
  uint32_t checksum = 2166136261UL;
  addChecksumWord(checksum, static_cast<uint16_t>(record.magic));
  addChecksumWord(checksum, static_cast<uint16_t>(record.magic >> 16));
  addChecksumWord(checksum, record.version);
  for (uint8_t i = 0; i < TouchInput::CalibrationPointCount; ++i) {
    addChecksumWord(checksum, static_cast<uint16_t>(record.rawX[i]));
    addChecksumWord(checksum, static_cast<uint16_t>(record.rawY[i]));
    addChecksumWord(checksum, static_cast<uint16_t>(record.rawZ[i]));
  }
  return checksum;
}

bool validCalibrationRecord(const CalibrationRecord &record) {
  if (record.magic != kCalibrationMagic ||
      record.version != kCalibrationVersion ||
      record.checksum != calibrationChecksum(record)) {
    return false;
  }

  for (uint8_t i = 0; i < TouchInput::CalibrationPointCount; ++i) {
    if (record.rawX[i] < 0 || record.rawX[i] > 1023 ||
        record.rawY[i] < 0 || record.rawY[i] > 1023 || record.rawZ[i] < 10 ||
        record.rawZ[i] >= 1000) {
      return false;
    }
  }
  return true;
}

std::array<TSPoint, TouchInput::CalibrationPointCount>
calibrationPoints(const CalibrationRecord &record) {
  std::array<TSPoint, TouchInput::CalibrationPointCount> points{};
  for (uint8_t i = 0; i < TouchInput::CalibrationPointCount; ++i) {
    points[i] = TSPoint(record.rawX[i], record.rawY[i], record.rawZ[i]);
  }
  return points;
}

void drawCard(Adafruit_GFX &display, int16_t x, int16_t y,
              const char *line1, const char *line2, const char *subtitle) {
  const uint16_t background = y < 100 ? kExplorerBackground
                                      : kExplorerAlternate;
  display.fillRoundRect(x, y, 142, 68, 4, background);
  display.drawRoundRect(x, y, 142, 68, 4, kExplorerLine);
  display.drawFastHLine(x + 7, y + 2, 128, kExplorerHeader);
  display.setTextColor(kExplorerText);
  display.setTextSize(1);
  display.setCursor(x + 49, y + 18);
  display.print(line1);
  display.setCursor(x + 49, y + 30);
  display.print(line2);
  display.setTextColor(kExplorerSubtleText);
  display.setCursor(x + 49, y + 49);
  display.print(subtitle);
}
}  // namespace

AppLauncher::AppLauncher(Adafruit_GFX &display, TouchInput &touch,
                         uint8_t sdChipSelect, uint8_t displayChipSelect)
    : _display(display),
      _touch(touch),
      _fileBrowser(display, touch, sdChipSelect, displayChipSelect),
      _musicPlayer(display, touch),
      _drawingApp(display, touch) {}

void AppLauncher::begin() {
  _mode = Mode::Splash;
  _waitForRelease = false;
  _touchHeld = false;
  _status[0] = '\0';
  _splashStartedAt = millis();
  _lastSplashFrameAt = _splashStartedAt;
  _splashFrame = 0;
  drawSplash();
  drawSplashAnimation();
}

void AppLauncher::update() {
  if (_mode == Mode::Splash) {
    const uint32_t now = millis();
    if (now - _splashStartedAt >= kSplashDurationMs) {
      finishStartup();
    } else if (now - _lastSplashFrameAt >= kSplashFrameIntervalMs) {
      _lastSplashFrameAt = now;
      ++_splashFrame;
      drawSplashAnimation();
    }
    delay(1);
    return;
  }

  if (_waitForRelease) {
    const TSPoint rawPoint = _touch.read();
    if (!TouchInput::isPressed(rawPoint)) {
      _waitForRelease = false;
      _touchHeld = false;
    }
    delay(1);
    return;
  }

  if (_mode == Mode::FileBrowser) {
    _fileBrowser.update();
    if (_fileBrowser.consumeExitRequested()) {
      _mode = Mode::Launcher;
      _waitForRelease = true;
      drawLauncher();
    }
    return;
  }

  if (_mode == Mode::MusicPlayer) {
    _musicPlayer.update();
    if (_musicPlayer.consumeExitRequested()) {
      _mode = Mode::Launcher;
      _waitForRelease = true;
      drawLauncher();
    }
    return;
  }

  if (_mode == Mode::Drawing) {
    _drawingApp.update();
    if (_drawingApp.consumeExitRequested()) {
      _mode = Mode::Launcher;
      _waitForRelease = true;
      drawLauncher();
    }
    return;
  }

  const TSPoint rawPoint = _touch.read();
  const bool pressed = TouchInput::isPressed(rawPoint);

  if (_mode == Mode::Calibrating) {
    handleCalibration(rawPoint, pressed);
  } else if (pressed && !_touchHeld) {
    ScreenPoint point{};
    if (_touch.mapToScreen(rawPoint, _display.width(), _display.height(),
                           point)) {
      handleLauncherTouch(point);
    }
  }
  _touchHeld = pressed;
  delay(1);
}

void AppLauncher::drawSplash() {
  _display.fillScreen(kExplorerBackground);
  _display.fillRect(0, 0, _display.width(), kHeaderHeight,
                    kExplorerToolbar);
  _display.setTextColor(kWhite);
  _display.setTextSize(1);
  _display.setCursor(10, 12);
  _display.print("Arduino R4 OS");
  _display.drawFastHLine(0, kHeaderHeight, _display.width(), kExplorerLine);

  _display.setTextColor(kExplorerText);
  _display.setTextSize(2);
  _display.setCursor(92, 91);
  _display.print("Welcome");
  _display.setTextSize(1);
  _display.setCursor(119, 119);
  _display.print("Starting up...");

  _display.fillRect(56, 157, 208, 12, kExplorerAlternate);
  _display.drawRect(56, 157, 208, 12, kExplorerLine);
}

void AppLauncher::drawSplashAnimation() {
  _display.fillRect(58, 159, 204, 8, kExplorerAlternate);
  constexpr int16_t segmentWidth = 14;
  constexpr int16_t segmentGap = 5;
  constexpr int16_t maxTravel = 146;
  const int16_t phase = static_cast<int16_t>(
      (_splashFrame * 7) % static_cast<uint16_t>(maxTravel * 2));
  const int16_t offset =
      phase <= maxTravel ? phase : maxTravel * 2 - phase;
  const uint16_t colors[] = {kExplorerToolbar, 0x4D7F, 0x7DBF};

  for (uint8_t segment = 0; segment < 3; ++segment) {
    const int16_t x =
        59 + offset + segment * (segmentWidth + segmentGap);
    _display.fillRect(x, 160, segmentWidth, 6, colors[segment]);
  }
}

void AppLauncher::finishStartup() {
  _status[0] = '\0';
  if (loadCalibration()) {
    strncpy(_status, "Calibration loaded", sizeof(_status) - 1);
    _mode = Mode::Launcher;
    drawLauncher();
  } else {
    startCalibration();
  }
}

void AppLauncher::startCalibration() {
  _mode = Mode::Calibrating;
  _calibrationIndex = 0;
  _calibrationSampleCount = 0;
  _calibrationSumX = 0;
  _calibrationSumY = 0;
  _calibrationSumZ = 0;
  _calibrationReleaseSampleCount = 0;
  _waitingForRelease = false;
  _status[0] = '\0';
  drawCalibrationScreen();
}

bool AppLauncher::loadCalibration() {
  if (EEPROM.length() <
      kCalibrationEepromAddress + static_cast<int>(sizeof(CalibrationRecord))) {
    return false;
  }

  CalibrationRecord record{};
  EEPROM.get(kCalibrationEepromAddress, record);
  if (!validCalibrationRecord(record)) {
    return false;
  }

  const auto points = calibrationPoints(record);
  if (!_touch.calibrate(points, kCalibrationTargets)) {
    return false;
  }
  _calibrationRaw = points;
  return true;
}

bool AppLauncher::saveCalibration() {
  if (EEPROM.length() <
      kCalibrationEepromAddress + static_cast<int>(sizeof(CalibrationRecord))) {
    return false;
  }

  CalibrationRecord record{};
  record.magic = kCalibrationMagic;
  record.version = kCalibrationVersion;
  for (uint8_t i = 0; i < TouchInput::CalibrationPointCount; ++i) {
    record.rawX[i] = _calibrationRaw[i].x;
    record.rawY[i] = _calibrationRaw[i].y;
    record.rawZ[i] = _calibrationRaw[i].z;
  }
  record.checksum = calibrationChecksum(record);
  EEPROM.put(kCalibrationEepromAddress, record);

  CalibrationRecord storedRecord{};
  EEPROM.get(kCalibrationEepromAddress, storedRecord);
  return validCalibrationRecord(storedRecord);
}

void AppLauncher::drawCalibrationTarget(int16_t x, int16_t y) {
  _display.drawFastHLine(x - 9, y, 19, kAppAccent);
  _display.drawFastVLine(x, y - 9, 19, kAppAccent);
  _display.fillCircle(x, y, 3, kAppAccent);
  _display.drawCircle(x, y, 3, kWhite);
}

void AppLauncher::drawCalibrationScreen() {
  _display.fillScreen(kExplorerBackground);
  _display.fillRect(0, 0, _display.width(), kHeaderHeight,
                    kExplorerToolbar);
  _display.setTextColor(kWhite);
  _display.setTextSize(1);
  _display.setCursor(10, 12);
  _display.print("DISPLAY CALIBRATION");
  _display.setTextColor(kExplorerText);
  _display.setCursor(10, 47);
  _display.print("Tap the target, then lift your finger.");
  drawCalibrationProgress();
}

void AppLauncher::drawCalibrationProgress() {
  _display.fillRect(10, 63, 128, 12, kExplorerBackground);
  _display.setTextColor(kExplorerText);
  _display.setTextSize(1);
  _display.setCursor(10, 64);
  _display.print("Point ");
  _display.print(_calibrationIndex + 1);
  _display.print(" of ");
  _display.print(TouchInput::CalibrationPointCount);
  drawCalibrationTarget(kCalibrationTargets[_calibrationIndex].x,
                        kCalibrationTargets[_calibrationIndex].y);
}

void AppLauncher::handleCalibration(const TSPoint &point, bool pressed) {
  if (_waitingForRelease) {
    // Resistive touch can briefly report a release while a finger is still
    // down; require consecutive release samples before accepting another tap.
    if (pressed) {
      _calibrationReleaseSampleCount = 0;
    } else if (++_calibrationReleaseSampleCount >=
               kCalibrationReleaseSampleCount) {
      _waitingForRelease = false;
      _calibrationReleaseSampleCount = 0;
    }
    return;
  }

  if (!pressed) {
    _calibrationSampleCount = 0;
    _calibrationSumX = 0;
    _calibrationSumY = 0;
    _calibrationSumZ = 0;
    return;
  }

  _calibrationSumX += point.x;
  _calibrationSumY += point.y;
  _calibrationSumZ += point.z;
  // Average several ADC samples to reduce noise in each calibration point.
  ++_calibrationSampleCount;
  if (_calibrationSampleCount < kCalibrationSampleCount) {
    return;
  }

  _calibrationRaw[_calibrationIndex] =
      TSPoint(_calibrationSumX / _calibrationSampleCount,
              _calibrationSumY / _calibrationSampleCount,
              _calibrationSumZ / _calibrationSampleCount);
  _waitingForRelease = true;
  _calibrationReleaseSampleCount = 0;
  _calibrationSampleCount = 0;
  _calibrationSumX = 0;
  _calibrationSumY = 0;
  _calibrationSumZ = 0;
  ++_calibrationIndex;

  if (_calibrationIndex < TouchInput::CalibrationPointCount) {
    const ScreenPoint &previousTarget =
        kCalibrationTargets[_calibrationIndex - 1];
    // Redraw only the old/new target and counter instead of clearing the TFT.
    _display.fillRect(previousTarget.x - 10, previousTarget.y - 10, 21, 21,
                      kExplorerBackground);
    drawCalibrationProgress();
    return;
  }

  if (_touch.calibrate(_calibrationRaw, kCalibrationTargets)) {
    strncpy(_status, saveCalibration() ? "Calibration saved"
                                       : "Calibration save failed",
            sizeof(_status) - 1);
    _mode = Mode::Launcher;
    _waitForRelease = true;
    drawLauncher();
    return;
  }

  startCalibration();
}

void AppLauncher::drawLauncher() {
  _display.fillScreen(kExplorerBackground);
  _display.fillRect(0, 0, _display.width(), kHeaderHeight,
                    kExplorerToolbar);
  _display.fillRoundRect(4, 5, 54, 22, 5, kExplorerBackground);
  _display.drawRoundRect(4, 5, 54, 22, 5, kExplorerLine);
  _display.setTextColor(kExplorerText);
  _display.setTextSize(1);
  _display.setCursor(15, 12);
  _display.print("HOME");
  _display.setTextColor(kWhite);
  _display.setCursor(66, 12);
  _display.print("Arduino R4 OS");
  _display.setCursor(268, 12);
  _display.print("4 APPS");

  drawCard(_display, 10, 48, "File", "Browser", "SD CARD");
  drawCard(_display, 168, 48, "Music", "Player", "UI DEMO");
  drawCard(_display, 10, 128, "Drawing", "App", "TOUCH CANVAS");
  drawCard(_display, 168, 128, "Display", "Calibration", "TOUCH");

  _display.fillCircle(31, 80, 17, kExplorerHeader);
  _display.drawCircle(31, 80, 17, kExplorerToolbar);
  _display.fillRoundRect(20, 81, 23, 14, 3, kFolderBody);
  _display.fillRect(23, 77, 11, 7, kFolderTab);
  _display.drawRoundRect(20, 81, 23, 14, 3, kFolderOutline);
  _display.drawFastHLine(23, 84, 17, kFolderOutline);

  _display.fillCircle(189, 80, 17, kExplorerHeader);
  _display.drawCircle(189, 80, 17, kExplorerToolbar);
  _display.drawLine(189, 88, 189, 69, kAppAccent);
  _display.drawLine(189, 69, 202, 66, kAppAccent);
  _display.drawLine(202, 66, 202, 82, kAppAccent);
  _display.fillCircle(185, 89, 5, kAppAccent);
  _display.fillCircle(198, 83, 5, kAppAccent);

  _display.fillCircle(31, 160, 17, kExplorerHeader);
  _display.drawCircle(31, 160, 17, kExplorerToolbar);
  _display.drawLine(22, 168, 40, 150, kAppAccent);
  _display.drawLine(24, 170, 42, 152, kAppAccent);
  _display.fillTriangle(20, 171, 25, 166, 29, 174, kAppAccent);

  _display.fillCircle(189, 160, 17, kExplorerHeader);
  _display.drawCircle(189, 160, 17, kExplorerToolbar);
  _display.drawCircle(189, 160, 12, kAppAccent);
  _display.drawFastHLine(173, 160, 32, kAppAccent);
  _display.drawFastVLine(189, 144, 32, kAppAccent);
  _display.fillCircle(189, 160, 3, kWhite);

  _display.fillRect(0, 201, _display.width(), _display.height() - 201,
                    kExplorerToolbar);
  _display.setTextColor(kWhite);
  _display.setTextSize(1);
  _display.setCursor(8, 211);
  _display.print(_status[0] == '\0' ? "Touch a card to launch" : _status);
}

void AppLauncher::handleLauncherTouch(const ScreenPoint &point) {
  if (point.y >= 48 && point.y < 116) {
    if (point.x >= 10 && point.x < 152) {
      _mode = Mode::FileBrowser;
      _fileBrowser.begin();
    } else if (point.x >= 168 && point.x < 310) {
      _mode = Mode::MusicPlayer;
      _musicPlayer.begin();
    } else {
      return;
    }
  } else if (point.y >= 128 && point.y < 196) {
    if (point.x >= 10 && point.x < 152) {
      _mode = Mode::Drawing;
      _drawingApp.begin();
    } else if (point.x >= 168 && point.x < 310) {
      startCalibration();
    } else {
      return;
    }
  } else {
    return;
  }
  _waitForRelease = true;
}

}  // namespace drawing
