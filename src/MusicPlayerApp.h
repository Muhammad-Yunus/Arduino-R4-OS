#pragma once

#include <Adafruit_GFX.h>

#include "TouchInput.h"

namespace drawing {

class MusicPlayerApp final {
 public:
  MusicPlayerApp(Adafruit_GFX &display, TouchInput &touch);

  void begin();
  void update();
  bool consumeExitRequested();

 private:
  static constexpr uint8_t TrackCount = 5;

  void drawPlayer();
  void drawPlaybackStatus();
  void drawTrackDetails();
  void drawSpectrum();
  void drawProgress();
  void drawPlaylistRow(uint8_t row);
  void handleTouch(const ScreenPoint &point);
  void selectTrack(uint8_t index);
  void moveTrack(int8_t direction);

  Adafruit_GFX &_display;
  TouchInput &_touch;
  uint32_t _lastTick = 0;
  uint16_t _elapsedSeconds = 0;
  uint8_t _trackIndex = 0;
  uint8_t _volume = 72;
  uint8_t _balance = 50;
  bool _touchHeld = false;
  bool _playing = false;
  bool _exitRequested = false;
};

}  // namespace drawing
