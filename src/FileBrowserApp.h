#pragma once

#include <array>
#include <stdint.h>

#include <Adafruit_GFX.h>

#include "TouchInput.h"

namespace drawing {

class FileBrowserApp final {
 public:
  FileBrowserApp(Adafruit_GFX &display, TouchInput &touch,
                 uint8_t sdChipSelect, uint8_t displayChipSelect);

  void begin();
  void update();
  bool consumeExitRequested();

 private:
  static constexpr uint8_t VisibleEntryCount = 6;
  static constexpr uint16_t EntryNameLength = 256;
  static constexpr uint16_t PathLength = 256;
  static constexpr uint16_t TextBufferCapacity = 1024;
  static constexpr int16_t HeaderHeight = 32;
  static constexpr int16_t RowTop = 50;
  static constexpr int16_t RowHeight = 25;
  static constexpr int16_t FooterTop = 212;
  static constexpr int16_t EditorTextTop = 36;
  static constexpr int16_t EditorTextHeight = 44;
  static constexpr int16_t EditorKeyboardTop = 84;

  struct Entry {
    char name[EntryNameLength]{};
    uint32_t size = 0;
    bool isDirectory = false;
  };

  void handleTouch(const ScreenPoint &point);
  void handleTextEditorTouch(const ScreenPoint &point);
  void drawBrowser();
  void drawBrowserRows();
  void drawBrowserFooter();
  void drawTextEditor();
  void drawEditorTextArea();
  void drawEditorFooter();
  void refreshDirectory(bool redrawPage = true);
  void enterDirectory(const char *name);
  void openTextFile(const char *name);
  void saveTextFile();
  void leaveTextEditor();
  void insertTextCharacter(char character);
  void eraseTextCharacter();
  void moveTextCursor(int8_t direction);
  void navigateToParent();
  bool isRoot() const;

  Adafruit_GFX &_display;
  TouchInput &_touch;
  const uint8_t _sdChipSelect;
  const uint8_t _displayChipSelect;
  std::array<Entry, VisibleEntryCount> _entries{};
  char _currentPath[PathLength] = "/";
  char _openedFilePath[PathLength]{};
  char _textBuffer[TextBufferCapacity + 1]{};
  char _status[64]{};
  uint16_t _textLength = 0;
  uint16_t _textCursor = 0;
  uint16_t _textViewStart = 0;
  uint16_t _entryCount = 0;
  uint16_t _scrollOffset = 0;
  ScreenPoint _browserTouchStart{};
  int16_t _browserTouchLastY = 0;
  int16_t _browserScrollRemainder = 0;
  uint8_t _visibleEntryCount = 0;
  bool _touchHeld = false;
  bool _mounted = false;
  bool _textEditorActive = false;
  bool _textDirty = false;
  bool _shiftActive = false;
  bool _browserTouchPending = false;
  bool _browserTouchMoved = false;
  bool _exitRequested = false;
};

}  // namespace drawing
