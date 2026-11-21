#include "MusicPlayerApp.h"

#include <Arduino.h>
#include <stdio.h>
#include <string.h>

namespace drawing {
namespace {
constexpr uint16_t kBlack = 0x0000;
constexpr uint16_t kSilver = 0xC618;
constexpr uint16_t kLightSilver = 0xFFFF;
constexpr uint16_t kMidSilver = 0x8410;
constexpr uint16_t kDarkSilver = 0x4208;
constexpr uint16_t kTitleBlue = 0x001F;
constexpr uint16_t kWhite = 0xFFFF;
constexpr uint16_t kGreen = 0x07E0;
constexpr uint16_t kLime = 0xBFE0;
constexpr uint16_t kYellow = 0xFFE0;
constexpr uint16_t kRed = 0xF800;
constexpr uint16_t kPlaylistBlue = 0x001F;
constexpr uint16_t kPlaylistBackground = 0x0000;

constexpr const char *kTrackNames[] = {
    "Blue Monday.mp3", "Digital Dream.mp3", "Night Drive.mp3",
    "Pixel Sunset.mp3", "Retro Future.mp3"};
constexpr uint16_t kTrackDurations[] = {247, 213, 198, 226, 205};
constexpr uint8_t kSpectrumHeights[] = {
    8, 13, 18, 11, 22, 16, 9, 25, 14, 19, 7, 17, 23, 12, 20, 10};

void printTime(Adafruit_GFX &display, int16_t x, int16_t y,
               uint16_t seconds) {
  char timeLabel[6];
  snprintf(timeLabel, sizeof(timeLabel), "%02u:%02u",
           static_cast<unsigned>(seconds / 60),
           static_cast<unsigned>(seconds % 60));
  display.setCursor(x, y);
  display.print(timeLabel);
}

void drawBevel(Adafruit_GFX &display, int16_t x, int16_t y, int16_t width,
               int16_t height, bool pressed = false) {
  display.fillRect(x, y, width, height, kSilver);
  const uint16_t topLeft = pressed ? kDarkSilver : kLightSilver;
  const uint16_t bottomRight = pressed ? kLightSilver : kDarkSilver;
  display.drawFastHLine(x, y, width - 1, topLeft);
  display.drawFastVLine(x, y, height - 1, topLeft);
  display.drawFastHLine(x + 1, y + height - 1, width - 1, bottomRight);
  display.drawFastVLine(x + width - 1, y + 1, height - 1, bottomRight);
  display.drawFastHLine(x + 1, y + 1, width - 3, kMidSilver);
  display.drawFastVLine(x + 1, y + 1, height - 3, kMidSilver);
}

void drawTransportButton(Adafruit_GFX &display, int16_t x,
                         uint8_t iconType, bool active = false) {
  constexpr int16_t y = 81;
  constexpr int16_t size = 23;
  drawBevel(display, x, y, size, size, active);
  const int16_t centerX = x + size / 2;
  const int16_t centerY = y + size / 2;
  if (iconType == 0) {
    display.fillTriangle(centerX - 2, centerY, centerX + 5, centerY - 6,
                         centerX + 5, centerY + 6, kDarkSilver);
    display.fillTriangle(centerX - 8, centerY, centerX - 1, centerY - 6,
                         centerX - 1, centerY + 6, kDarkSilver);
    display.drawFastVLine(centerX - 9, centerY - 6, 13, kDarkSilver);
  } else if (iconType == 1) {
    display.fillTriangle(centerX - 4, centerY - 7, centerX + 7, centerY,
                         centerX - 4, centerY + 7, kDarkSilver);
  } else if (iconType == 2) {
    display.fillRect(centerX - 6, centerY - 7, 4, 14, kDarkSilver);
    display.fillRect(centerX + 2, centerY - 7, 4, 14, kDarkSilver);
  } else if (iconType == 3) {
    display.fillRect(centerX - 6, centerY - 6, 12, 12, kDarkSilver);
  } else if (iconType == 4) {
    display.fillTriangle(centerX - 5, centerY - 6, centerX + 2, centerY,
                         centerX - 5, centerY + 6, kDarkSilver);
    display.fillTriangle(centerX + 1, centerY - 6, centerX + 8, centerY,
                         centerX + 1, centerY + 6, kDarkSilver);
    display.drawFastVLine(centerX + 9, centerY - 6, 13, kDarkSilver);
  } else {
    display.fillTriangle(centerX - 7, centerY + 3, centerX + 7, centerY + 3,
                         centerX, centerY - 5, kDarkSilver);
    display.drawFastHLine(centerX - 7, centerY + 5, 15, kDarkSilver);
  }
}

void drawSlider(Adafruit_GFX &display, int16_t x, int16_t y, int16_t width,
                const char *label, uint8_t value) {
  display.setTextColor(kDarkSilver);
  display.setTextSize(1);
  display.setCursor(x, y - 6);
  display.print(label);
  display.fillRect(x, y + 3, width, 4, kDarkSilver);
  display.drawFastHLine(x, y + 2, width, kLightSilver);
  const int16_t thumbX =
      x + static_cast<int16_t>((width - 5) * value / 100);
  drawBevel(display, thumbX, y, 5, 10);
}
}  // namespace

MusicPlayerApp::MusicPlayerApp(Adafruit_GFX &display, TouchInput &touch)
    : _display(display), _touch(touch) {}

void MusicPlayerApp::begin() {
  _touchHeld = false;
  _exitRequested = false;
  _playing = false;
  _elapsedSeconds = 0;
  _trackIndex = 0;
  _lastTick = millis();
  drawPlayer();
}

void MusicPlayerApp::update() {
  const TSPoint rawPoint = _touch.read();
  const bool pressed = TouchInput::isPressed(rawPoint);
  if (pressed && !_touchHeld) {
    ScreenPoint point{};
    if (_touch.mapToScreen(rawPoint, _display.width(), _display.height(),
                           point)) {
      handleTouch(point);
    }
  }
  _touchHeld = pressed;

  const uint32_t now = millis();
  if (_playing && now - _lastTick >= 1000) {
    _lastTick += 1000;
    ++_elapsedSeconds;
    if (_elapsedSeconds >= kTrackDurations[_trackIndex]) {
      moveTrack(1);
    } else {
      drawPlaybackStatus();
    }
  } else if (!_playing) {
    _lastTick = now;
  }
  delay(1);
}

bool MusicPlayerApp::consumeExitRequested() {
  const bool requested = _exitRequested;
  _exitRequested = false;
  return requested;
}

void MusicPlayerApp::drawPlaybackStatus() {
  // Playback ticks update only dynamic fields; the static player frame is
  // drawn once so a full-screen refresh does not flash every second.
  _display.fillRect(11, 24, 76, 19, kBlack);
  _display.setTextColor(kGreen);
  _display.setTextSize(2);
  printTime(_display, 13, 25, _elapsedSeconds);

  _display.fillRect(117, 24, 28, 12, kBlack);
  _display.setTextColor(_playing ? kLime : kYellow);
  _display.setTextSize(1);
  _display.setCursor(119, 26);
  _display.print(_playing ? "PLAY" : "STOP");

  drawSpectrum();
  drawProgress();

  _display.fillRect(263, 217, 50, 16, kSilver);
  _display.setTextColor(kDarkSilver);
  _display.setCursor(265, 222);
  _display.print(_trackIndex + 1);
  _display.print("/5");
}

void MusicPlayerApp::drawTrackDetails() {
  _display.fillRect(8, 21, 138, 43, kBlack);
  _display.setTextColor(kGreen);
  _display.setTextSize(2);
  printTime(_display, 13, 25, _elapsedSeconds);
  _display.setTextSize(1);
  _display.setCursor(13, 47);
  _display.print("128 kbps  44 kHz");
  _display.setCursor(13, 56);
  _display.print(_trackIndex + 1);
  _display.print(". ");
  for (uint8_t i = 0; i < 19 && kTrackNames[_trackIndex][i] != '\0'; ++i) {
    _display.print(kTrackNames[_trackIndex][i]);
  }
}

void MusicPlayerApp::drawSpectrum() {
  _display.fillRect(154, 33, 155, 29, kBlack);
  for (uint8_t i = 0; i < sizeof(kSpectrumHeights); ++i) {
    const int16_t x = 157 + i * 9;
    const uint8_t motion = _playing
                               ? static_cast<uint8_t>(
                                     (_elapsedSeconds + i * 3) % 7)
                               : 0;
    const uint8_t height = static_cast<uint8_t>(
        3 + (kSpectrumHeights[i] + motion) % 24);
    const uint16_t color = height > 19 ? kRed : height > 12 ? kYellow : kGreen;
    _display.fillRect(x, 60 - height, 6, height, color);
  }
}

void MusicPlayerApp::drawProgress() {
  _display.fillRect(9, 69, 302, 7, kDarkSilver);
  const uint16_t duration = kTrackDurations[_trackIndex];
  const int16_t filledWidth = static_cast<int16_t>(
      (static_cast<uint32_t>(_elapsedSeconds) * 298) / duration);
  if (filledWidth > 0) {
    _display.fillRect(11, 71, filledWidth, 3, kGreen);
  }
  const int16_t seekThumbX = 10 + filledWidth;
  _display.fillRect(seekThumbX - 1, 69, 3, 7, kWhite);
  _display.drawFastVLine(seekThumbX, 69, 7, kDarkSilver);
}

void MusicPlayerApp::drawPlaylistRow(uint8_t row) {
  if (row >= TrackCount) {
    return;
  }

  const int16_t y = 135 + row * 16;
  const bool selected = row == _trackIndex;
  _display.fillRect(7, y, 306, 16, selected ? kPlaylistBlue
                                             : kPlaylistBackground);
  _display.setTextColor(selected ? kLime : kGreen);
  _display.setTextSize(1);
  _display.setCursor(11, y + 4);
  _display.print(row + 1);
  _display.print(". ");
  _display.print(kTrackNames[row]);
  printTime(_display, 275, y + 4, kTrackDurations[row]);
}

void MusicPlayerApp::drawPlayer() {
  _display.fillScreen(kSilver);

  drawBevel(_display, 2, 2, 316, 110);
  _display.fillRect(4, 4, 312, 13, kTitleBlue);
  _display.fillRect(6, 5, 35, 11, kSilver);
  _display.drawRect(6, 5, 35, 11, kDarkSilver);
  _display.setTextColor(kBlack);
  _display.setTextSize(1);
  _display.setCursor(11, 7);
  _display.print("HOME");
  _display.setTextColor(kWhite);
  _display.setCursor(47, 7);
  _display.print("WINAMP MP3 PLAYER");
  for (int16_t buttonX = 288; buttonX <= 306; buttonX += 9) {
    _display.fillRect(buttonX, 6, 8, 9, kSilver);
    _display.drawRect(buttonX, 6, 8, 9, kDarkSilver);
  }
  _display.drawFastHLine(290, 12, 4, kBlack);
  _display.drawRect(299, 8, 4, 4, kBlack);
  _display.drawLine(308, 7, 313, 13, kBlack);
  _display.drawLine(313, 7, 308, 13, kBlack);

  _display.fillRect(7, 20, 140, 45, kBlack);
  _display.drawRect(7, 20, 140, 45, kDarkSilver);

  _display.fillRect(151, 20, 161, 45, kBlack);
  _display.drawRect(151, 20, 161, 45, kDarkSilver);
  _display.setTextColor(kGreen);
  _display.setTextSize(1);
  _display.setCursor(157, 22);
  _display.print("SPECTRUM ANALYZER");

  drawBevel(_display, 8, 68, 304, 9, true);

  drawTransportButton(_display, 8, 0);
  drawTransportButton(_display, 33, 1, _playing);
  drawTransportButton(_display, 58, 2);
  drawTransportButton(_display, 83, 3);
  drawTransportButton(_display, 108, 4);
  drawTransportButton(_display, 133, 5);
  drawSlider(_display, 177, 91, 57, "VOL", _volume);
  drawSlider(_display, 249, 91, 58, "BAL", _balance);

  drawBevel(_display, 2, 118, 316, 118);
  _display.fillRect(4, 120, 312, 13, kTitleBlue);
  _display.setTextColor(kWhite);
  _display.setTextSize(1);
  _display.setCursor(9, 123);
  _display.print("WINAMP PLAYLIST");
  _display.setCursor(263, 123);
  _display.print("5 TRACKS");

  _display.fillRect(6, 135, 308, 80, kPlaylistBackground);
  for (uint8_t row = 0; row < TrackCount; ++row) {
    drawPlaylistRow(row);
  }

  _display.fillRect(6, 217, 308, 16, kSilver);
  _display.setTextColor(kDarkSilver);
  _display.setCursor(11, 222);
  _display.print("UI DEMO - NO AUDIO OUTPUT");

  drawTrackDetails();
  drawPlaybackStatus();
}

void MusicPlayerApp::handleTouch(const ScreenPoint &point) {
  if (point.y < 28 && point.x < 44) {
    _exitRequested = true;
    return;
  }

  if (point.y >= 68 && point.y < 78) {
    const int16_t seekX = point.x < 10 ? 10 : point.x > 308 ? 308 : point.x;
    _elapsedSeconds = static_cast<uint16_t>(
        static_cast<uint32_t>(seekX - 10) * kTrackDurations[_trackIndex] /
        298);
    drawPlaybackStatus();
    return;
  }

  if (point.y >= 81 && point.y < 106) {
    if (point.x < 28) {
      moveTrack(-1);
    } else if (point.x < 57) {
      _playing = !_playing;
      _lastTick = millis();
      drawTransportButton(_display, 33, 1, _playing);
      drawPlaybackStatus();
    } else if (point.x < 82) {
      _playing = false;
      drawTransportButton(_display, 33, 1, false);
      drawPlaybackStatus();
    } else if (point.x < 107) {
      _playing = false;
      _elapsedSeconds = 0;
      drawTransportButton(_display, 33, 1, false);
      drawPlaybackStatus();
    } else if (point.x < 132) {
      moveTrack(1);
    } else if (point.x >= 133 && point.x < 157) {
      _elapsedSeconds = 0;
      _playing = false;
      drawTransportButton(_display, 33, 1, false);
      drawPlaybackStatus();
    } else if (point.x >= 177 && point.x <= 234) {
      _volume = static_cast<uint8_t>((point.x - 177) * 100 / 57);
      drawSlider(_display, 177, 91, 57, "VOL", _volume);
    } else if (point.x >= 249 && point.x <= 307) {
      _balance = static_cast<uint8_t>((point.x - 249) * 100 / 58);
      drawSlider(_display, 249, 91, 58, "BAL", _balance);
    }
    return;
  }

  if (point.y >= 135 && point.y < 215) {
    const uint8_t row = static_cast<uint8_t>((point.y - 135) / 16);
    if (row < TrackCount) {
      selectTrack(row);
    }
  }
}

void MusicPlayerApp::selectTrack(uint8_t index) {
  if (index >= TrackCount) {
    return;
  }
  const uint8_t previousTrack = _trackIndex;
  _trackIndex = index;
  _elapsedSeconds = 0;
  _playing = true;
  _lastTick = millis();
  drawTransportButton(_display, 33, 1, true);
  drawTrackDetails();
  drawPlaybackStatus();
  drawPlaylistRow(previousTrack);
  drawPlaylistRow(_trackIndex);
}

void MusicPlayerApp::moveTrack(int8_t direction) {
  const uint8_t previousTrack = _trackIndex;
  if (direction < 0) {
    _trackIndex =
        _trackIndex == 0 ? TrackCount - 1 : static_cast<uint8_t>(_trackIndex - 1);
  } else {
    _trackIndex = static_cast<uint8_t>((_trackIndex + 1) % TrackCount);
  }
  _elapsedSeconds = 0;
  _lastTick = millis();
  drawTransportButton(_display, 33, 1, _playing);
  drawTrackDetails();
  drawPlaybackStatus();
  drawPlaylistRow(previousTrack);
  drawPlaylistRow(_trackIndex);
}

}  // namespace drawing
