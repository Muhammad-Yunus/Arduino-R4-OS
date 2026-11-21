#pragma once

#include <array>

#include <Adafruit_GFX.h>

#include "DrawingApp.h"
#include "FileBrowserApp.h"
#include "MusicPlayerApp.h"
#include "TouchInput.h"

namespace drawing {

class AppLauncher final {
 public:
  AppLauncher(Adafruit_GFX &display, TouchInput &touch, uint8_t sdChipSelect,
              uint8_t displayChipSelect);

  void begin();
  void update();

 private:
  enum class Mode : uint8_t {
    Splash,
    Calibrating,
    Launcher,
    FileBrowser,
    MusicPlayer,
    Drawing,
  };

  void drawSplash();
  void drawSplashAnimation();
  void finishStartup();
  void startCalibration();
  bool loadCalibration();
  bool saveCalibration();
  void drawCalibrationScreen();
  void drawCalibrationProgress();
  void drawCalibrationTarget(int16_t x, int16_t y);
  void handleCalibration(const TSPoint &point, bool pressed);
  void drawLauncher();
  void handleLauncherTouch(const ScreenPoint &point);

  Adafruit_GFX &_display;
  TouchInput &_touch;
  FileBrowserApp _fileBrowser;
  MusicPlayerApp _musicPlayer;
  DrawingApp _drawingApp;
  std::array<TSPoint, TouchInput::CalibrationPointCount> _calibrationRaw{};
  Mode _mode = Mode::Calibrating;
  uint8_t _calibrationIndex = 0;
  uint8_t _calibrationSampleCount = 0;
  uint8_t _calibrationReleaseSampleCount = 0;
  int32_t _calibrationSumX = 0;
  int32_t _calibrationSumY = 0;
  int32_t _calibrationSumZ = 0;
  char _status[32]{};
  bool _waitingForRelease = false;
  bool _touchHeld = false;
  bool _waitForRelease = false;
  uint32_t _splashStartedAt = 0;
  uint32_t _lastSplashFrameAt = 0;
  uint16_t _splashFrame = 0;
};

}  // namespace drawing
