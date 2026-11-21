#include "FileBrowserApp.h"

#include <Arduino.h>
#include <SdFat.h>
#include <SPI.h>
#include <string.h>

namespace drawing {
namespace {
constexpr uint16_t kWhite = 0xFFFF;
constexpr uint16_t kYellow = 0xFFE0;
constexpr uint16_t kRed = 0xF800;
constexpr uint16_t kExplorerBackground = 0xFFFF;
constexpr uint16_t kExplorerToolbar = 0x2A91;
constexpr uint16_t kExplorerHeader = 0xEF7D;
constexpr uint16_t kExplorerAlternate = 0xF7BE;
constexpr uint16_t kExplorerLine = 0xCE79;
constexpr uint16_t kExplorerText = 0x2104;
constexpr uint16_t kExplorerSubtleText = 0x5AEB;
constexpr uint16_t kExplorerFolder = 0xFDC0;
constexpr uint16_t kExplorerFolderTab = 0xFFE0;
constexpr uint16_t kExplorerFolderOutline = 0xB5A0;
constexpr uint16_t kExplorerFileLine = 0x4D7F;

constexpr uint8_t kTextColumns = 51;
constexpr uint8_t kTextRows = 4;
constexpr int16_t kEditorTextLeft = 6;
constexpr int16_t kEditorLineHeight = 10;

SdFat32 sdCard;
bool sdCardMounted = false;

bool hasTxtExtension(const char *name) {
  const size_t length = strlen(name);
  if (length < 4) {
    return false;
  }

  const char *extension = name + length - 4;
  return extension[0] == '.' &&
         (extension[1] == 't' || extension[1] == 'T') &&
         (extension[2] == 'x' || extension[2] == 'X') &&
         (extension[3] == 't' || extension[3] == 'T');
}

void drawEditorKey(Adafruit_GFX &display, int16_t x, int16_t y, int16_t width,
                   int16_t height, const char *label, bool active = false) {
  display.fillRoundRect(x, y, width, height, 3,
                        active ? kExplorerToolbar : kExplorerHeader);
  display.drawRoundRect(x, y, width, height, 3,
                        active ? kExplorerToolbar : kExplorerLine);
  display.setTextColor(active ? kWhite : kExplorerText);
  display.setTextSize(1);
  const int16_t labelWidth = static_cast<int16_t>(strlen(label) * 6);
  display.setCursor(x + (width - labelWidth) / 2,
                    y + (height - 8) / 2);
  display.print(label);
}

void drawFolderIcon(Adafruit_GFX &display, int16_t x, int16_t y) {
  display.fillRoundRect(x, y + 4, 18, 12, 2, kExplorerFolder);
  display.fillRect(x + 2, y + 1, 8, 5, kExplorerFolderTab);
  display.drawRoundRect(x, y + 4, 18, 12, 2, kExplorerFolderOutline);
  display.drawFastHLine(x + 2, y + 7, 14, kExplorerFolderOutline);
}

void drawFileIcon(Adafruit_GFX &display, int16_t x, int16_t y,
                  bool textFile) {
  display.fillRect(x + 2, y, 13, 18, kWhite);
  display.drawRect(x + 2, y, 13, 18, kExplorerSubtleText);
  display.drawLine(x + 10, y, x + 14, y + 4, kExplorerSubtleText);
  const uint16_t lineColor = textFile ? kExplorerFileLine : kExplorerLine;
  display.drawFastHLine(x + 5, y + 8, 7, lineColor);
  display.drawFastHLine(x + 5, y + 11, 7, lineColor);
  display.drawFastHLine(x + 5, y + 14, 5, lineColor);
}

void printClippedName(Adafruit_GFX &display, const char *name, int16_t x,
                      int16_t y, uint8_t maxCharacters) {
  const size_t length = strlen(name);
  const bool clipped = length > maxCharacters;
  const size_t charactersToPrint =
      clipped && maxCharacters > 3 ? maxCharacters - 3 : maxCharacters;
  display.setCursor(x, y);
  for (size_t i = 0; i < charactersToPrint && name[i] != '\0'; ++i) {
    display.write(name[i]);
  }
  if (clipped && maxCharacters > 3) {
    display.print("...");
  }
}
}  // namespace

FileBrowserApp::FileBrowserApp(Adafruit_GFX &display, TouchInput &touch,
                               uint8_t sdChipSelect,
                               uint8_t displayChipSelect)
    : _display(display),
      _touch(touch),
      _sdChipSelect(sdChipSelect),
      _displayChipSelect(displayChipSelect) {}

void FileBrowserApp::begin() {
  _touchHeld = false;
  _browserTouchPending = false;
  _browserTouchMoved = false;
  _textEditorActive = false;
  _textDirty = false;
  _exitRequested = false;
  _status[0] = '\0';

  if (!sdCardMounted) {
    digitalWrite(_displayChipSelect, HIGH);
    pinMode(_sdChipSelect, OUTPUT);
    digitalWrite(_sdChipSelect, HIGH);
    sdCardMounted = sdCard.begin(
        SdSpiConfig(_sdChipSelect, SHARED_SPI, 250000, &SPI));
  }
  _mounted = sdCardMounted;
  if (!_mounted) {
    strncpy(_status, "SD mount failed; check card/FAT32",
            sizeof(_status) - 1);
    drawBrowser();
    return;
  }

  refreshDirectory();
}

void FileBrowserApp::update() {
  const TSPoint rawPoint = _touch.read();
  const bool pressed = TouchInput::isPressed(rawPoint);

  if (_textEditorActive) {
    if (pressed && !_touchHeld) {
      ScreenPoint point{};
      if (_touch.mapToScreen(rawPoint, _display.width(), _display.height(),
                             point)) {
        handleTouch(point);
      }
    }
    _touchHeld = pressed;
  } else {
    ScreenPoint point{};
    const bool mapped =
        pressed && _touch.mapToScreen(rawPoint, _display.width(),
                                      _display.height(), point);

    if (pressed && !_touchHeld) {
      _browserTouchPending = mapped;
      _browserTouchMoved = false;
      _browserScrollRemainder = 0;
      if (mapped) {
        _browserTouchStart = point;
        _browserTouchLastY = point.y;
      }
    } else if (pressed && _touchHeld && mapped &&
               _browserTouchPending) {
      const int16_t travel = point.y - _browserTouchStart.y;
      if (_browserTouchStart.y >= RowTop && _browserTouchStart.y < FooterTop &&
          (travel >= 8 || travel <= -8)) {
        _browserTouchMoved = true;
      }

      if (_browserTouchMoved) {
        // Keep sub-row drag distance so slow swipes accumulate instead of
        // being discarded until each 25-pixel row boundary is crossed.
        _browserScrollRemainder += _browserTouchLastY - point.y;
        _browserTouchLastY = point.y;
        const int16_t rows = _browserScrollRemainder / RowHeight;
        if (rows != 0) {
          _browserScrollRemainder -= rows * RowHeight;
          const uint16_t maxOffset =
              _entryCount > VisibleEntryCount
                  ? _entryCount - VisibleEntryCount
                  : 0;
          const int32_t requestedOffset =
              static_cast<int32_t>(_scrollOffset) + rows;
          const uint16_t newOffset = static_cast<uint16_t>(
              requestedOffset < 0
                  ? 0
                  : requestedOffset > maxOffset ? maxOffset
                                                : requestedOffset);
          if (newOffset != _scrollOffset) {
            _scrollOffset = newOffset;
            refreshDirectory(false);
          }
        }
      } else {
        _browserTouchLastY = point.y;
      }
    } else if (!pressed && _touchHeld) {
      if (_browserTouchPending && !_browserTouchMoved) {
        handleTouch(_browserTouchStart);
      }
      _browserTouchPending = false;
    }

    _touchHeld = pressed;
  }

  delay(1);
}

void FileBrowserApp::handleTouch(const ScreenPoint &point) {
  if (_textEditorActive) {
    handleTextEditorTouch(point);
    return;
  }

  if (!_mounted) {
    return;
  }

  if (point.y < HeaderHeight) {
    if (point.x < 42) {
      _exitRequested = true;
    } else if (point.x < 76 && !isRoot()) {
      navigateToParent();
    }
    return;
  }

  if (point.y >= FooterTop) {
    return;
  }

  if (point.y < RowTop) {
    return;
  }

  const uint8_t row = static_cast<uint8_t>((point.y - RowTop) / RowHeight);
  if (row >= _visibleEntryCount) {
    return;
  }

  const Entry &entry = _entries[row];
  if (entry.isDirectory) {
    enterDirectory(entry.name);
    return;
  }

  if (hasTxtExtension(entry.name)) {
    openTextFile(entry.name);
    return;
  }

  snprintf(_status, sizeof(_status), "File: %lu bytes",
           static_cast<unsigned long>(entry.size));
  drawBrowser();
}

bool FileBrowserApp::consumeExitRequested() {
  const bool requested = _exitRequested;
  _exitRequested = false;
  return requested;
}

void FileBrowserApp::drawBrowser() {
  _display.fillScreen(kExplorerBackground);
  _display.fillRect(0, 0, _display.width(), HeaderHeight, kExplorerToolbar);
  _display.setTextColor(kWhite);
  _display.setTextSize(1);

  _display.fillRoundRect(4, 5, 36, 22, 5, kExplorerBackground);
  _display.drawRoundRect(4, 5, 36, 22, 5, kExplorerLine);
  _display.setTextColor(kExplorerText);
  _display.setCursor(10, 12);
  _display.print("HOME");
  if (!isRoot()) {
    _display.fillRoundRect(44, 5, 28, 22, 5, kExplorerBackground);
    _display.drawRoundRect(44, 5, 28, 22, 5, kExplorerLine);
    _display.drawLine(61, 10, 55, 16, kExplorerText);
    _display.drawLine(55, 16, 61, 22, kExplorerText);
    _display.drawFastHLine(55, 16, 10, kExplorerText);
  }

  _display.setTextColor(kWhite);
  const char *pathLabel = "This PC";
  if (!isRoot()) {
    const char *lastSlash = strrchr(_currentPath, '/');
    pathLabel = lastSlash == nullptr ? _currentPath : lastSlash + 1;
  }
  printClippedName(_display, pathLabel, 78, 12, 39);

  if (!_mounted) {
    _display.setCursor(8, 48);
    _display.setTextColor(kRed);
    _display.print(_status);
    return;
  }

  _display.fillRect(0, HeaderHeight, _display.width(),
                    RowTop - HeaderHeight, kExplorerHeader);
  _display.setTextColor(kExplorerSubtleText);
  _display.setCursor(36, 39);
  _display.print("Name");
  _display.setCursor(_display.width() - 42, 39);
  _display.print("Size");
  _display.drawFastHLine(0, RowTop - 1, _display.width(), kExplorerLine);

  drawBrowserRows();
  drawBrowserFooter();
}

void FileBrowserApp::drawBrowserRows() {
  // The header and footer stay fixed during scrolling; repaint only the list.
  _display.fillRect(0, RowTop, _display.width(), FooterTop - RowTop,
                    kExplorerBackground);
  for (uint8_t row = 0; row < VisibleEntryCount; ++row) {
    const int16_t y = RowTop + row * RowHeight;
    _display.fillRect(0, y, _display.width(), RowHeight,
                      row % 2 == 0 ? kExplorerBackground
                                   : kExplorerAlternate);
    _display.drawFastHLine(0, y + RowHeight - 1, _display.width(),
                           kExplorerLine);

    if (row >= _visibleEntryCount) {
      continue;
    }

    const Entry &entry = _entries[row];
    if (entry.isDirectory) {
      drawFolderIcon(_display, 9, y + 4);
    } else {
      drawFileIcon(_display, 10, y + 3, hasTxtExtension(entry.name));
    }

    _display.setTextColor(kExplorerText);

    if (!entry.isDirectory) {
      char sizeLabel[16];
      snprintf(sizeLabel, sizeof(sizeLabel), "%lu",
               static_cast<unsigned long>(entry.size));
      const int16_t sizeX =
          _display.width() - static_cast<int16_t>(strlen(sizeLabel) * 6) - 6;
      const uint8_t maxNameCharacters =
          static_cast<uint8_t>((sizeX - 40) / 6);
      printClippedName(_display, entry.name, 36, y + 9,
                       maxNameCharacters);
      _display.setTextColor(kExplorerSubtleText);
      _display.setCursor(sizeX, y + 10);
      _display.print(sizeLabel);
    } else {
      printClippedName(_display, entry.name, 36, y + 9, 45);
    }
  }

  if (_entryCount > VisibleEntryCount) {
    const int16_t trackTop = RowTop + 2;
    const int16_t trackHeight = VisibleEntryCount * RowHeight - 4;
    const uint16_t maxOffset = _entryCount - VisibleEntryCount;
    const int16_t calculatedThumbHeight =
        static_cast<int16_t>(static_cast<uint32_t>(trackHeight) *
                             VisibleEntryCount / _entryCount);
    const int16_t thumbHeight =
        calculatedThumbHeight < 12 ? 12 : calculatedThumbHeight;
    const int16_t thumbRange = trackHeight - thumbHeight;
    const int16_t thumbTop =
        trackTop + static_cast<int16_t>(
                       static_cast<uint32_t>(_scrollOffset) * thumbRange /
                       maxOffset);
    _display.fillRoundRect(_display.width() - 5, trackTop, 3, trackHeight, 1,
                           kExplorerLine);
    _display.fillRoundRect(_display.width() - 6, thumbTop, 5, thumbHeight, 2,
                           kExplorerToolbar);
  }
}

void FileBrowserApp::drawBrowserFooter() {
  _display.fillRect(0, FooterTop, _display.width(),
                    _display.height() - FooterTop, kExplorerToolbar);

  if (_status[0] != '\0') {
    _display.setTextColor(kWhite);
    _display.setCursor(8, FooterTop + 10);
    _display.print(_status);
  } else {
    _display.setTextColor(kWhite);
    _display.setCursor(8, FooterTop + 10);
    _display.print(_entryCount);
    _display.print(" items");
    if (_entryCount > VisibleEntryCount) {
      _display.setCursor(_display.width() - 94, FooterTop + 10);
      _display.print("Swipe to scroll");
    }
  }
}

void FileBrowserApp::drawTextEditor() {
  _display.fillScreen(kExplorerBackground);
  _display.fillRect(0, 0, _display.width(), HeaderHeight, kExplorerToolbar);
  drawEditorKey(_display, 4, 4, 46, 24, "BACK");
  drawEditorKey(_display, 270, 4, 46, 24, "SAVE");

  const char *fileName = strrchr(_openedFilePath, '/');
  fileName = fileName == nullptr ? _openedFilePath : fileName + 1;
  char fileLabel[34]{};
  strncpy(fileLabel, fileName, sizeof(fileLabel) - 1);
  _display.setTextColor(kWhite);
  _display.setTextSize(1);
  _display.setCursor(56, 12);
  _display.print(fileLabel);

  drawEditorTextArea();

  static const char *const kFirstRow = "qwertyuiop";
  static const char *const kSecondRow = "asdfghjkl";
  static const char *const kThirdRow = "zxcvbnm,.";
  static const char *const kNumberRow = "1234567890";
  char keyLabel[2] = {'\0', '\0'};

  for (uint8_t i = 0; i < 10; ++i) {
    keyLabel[0] = kFirstRow[i];
    drawEditorKey(_display, 5 + i * 31, EditorKeyboardTop, 30, 22,
                  keyLabel);
    keyLabel[0] = kNumberRow[i];
    drawEditorKey(_display, 5 + i * 31, EditorKeyboardTop + 72, 30, 22,
                  keyLabel);
  }
  for (uint8_t i = 0; i < 9; ++i) {
    keyLabel[0] = kSecondRow[i];
    drawEditorKey(_display, 17 + i * 32, EditorKeyboardTop + 24, 30, 22,
                  keyLabel);
    keyLabel[0] = kThirdRow[i];
    drawEditorKey(_display, 50 + i * 30, EditorKeyboardTop + 48, 29, 22,
                  keyLabel);
  }

  drawEditorKey(_display, 4, EditorKeyboardTop + 48, 42, 22, "SHIFT",
                _shiftActive);
  drawEditorKey(_display, 5, EditorKeyboardTop + 96, 38, 24, "<");
  drawEditorKey(_display, 45, EditorKeyboardTop + 96, 38, 24, ">");
  drawEditorKey(_display, 85, EditorKeyboardTop + 96, 100, 24, "SPACE");
  drawEditorKey(_display, 187, EditorKeyboardTop + 96, 58, 24, "ENTER");
  drawEditorKey(_display, 247, EditorKeyboardTop + 96, 68, 24, "BS");

  drawEditorFooter();
}

void FileBrowserApp::drawEditorTextArea() {
  _display.fillRect(4, EditorTextTop + 1, _display.width() - 8,
                    EditorTextHeight - 2, kWhite);
  _display.drawRect(3, EditorTextTop, _display.width() - 6,
                    EditorTextHeight, kExplorerLine);

  // Keep the cursor visible by anchoring the four-line viewport to its
  // current wrapped line.
  uint16_t lineStart = 0;
  uint8_t column = 0;
  for (uint16_t i = 0; i < _textCursor; ++i) {
    const char character = _textBuffer[i];
    if (character == '\r') {
      continue;
    }
    if (character == '\n') {
      lineStart = i + 1;
      column = 0;
      continue;
    }
    if (column == kTextColumns) {
      lineStart = i;
      column = 0;
    }
    ++column;
    if (column == kTextColumns) {
      lineStart = i + 1;
      column = 0;
    }
  }
  _textViewStart = lineStart;

  _display.setTextColor(kExplorerText);
  _display.setTextSize(1);
  uint8_t row = 0;
  column = 0;
  for (uint16_t i = _textViewStart; i < _textLength && row < kTextRows;
       ++i) {
    const char character = _textBuffer[i];
    if (character == '\r') {
      continue;
    }
    if (character == '\n') {
      ++row;
      column = 0;
      continue;
    }
    if (column == kTextColumns) {
      ++row;
      column = 0;
      if (row >= kTextRows) {
        break;
      }
    }

    const char displayCharacter =
        character >= 32 && character <= 126 ? character : '?';
    _display.setCursor(kEditorTextLeft + column * 6,
                       EditorTextTop + 3 + row * kEditorLineHeight);
    _display.print(displayCharacter);
    ++column;
  }

  uint8_t cursorColumn = 0;
  for (uint16_t i = _textViewStart; i < _textCursor; ++i) {
    if (_textBuffer[i] == '\r') {
      continue;
    }
    if (_textBuffer[i] == '\n' || cursorColumn == kTextColumns) {
      cursorColumn = 0;
      continue;
    }
    ++cursorColumn;
  }
  _display.drawFastVLine(kEditorTextLeft + cursorColumn * 6,
                         EditorTextTop + 3, 9, kExplorerToolbar);
}

void FileBrowserApp::drawEditorFooter() {
  _display.fillRect(0, FooterTop, _display.width(),
                    _display.height() - FooterTop, kExplorerToolbar);
  drawEditorKey(_display, 5, FooterTop + 4, 66, 22, "CANCEL");
  const bool showStatus = _status[0] != '\0';
  _display.setTextColor(!showStatus && _textDirty ? kYellow : kWhite);
  _display.setTextSize(1);
  _display.setCursor(82, FooterTop + 12);
  _display.print(showStatus ? _status : (_textDirty ? "Unsaved" : ""));
}

void FileBrowserApp::openTextFile(const char *name) {
  const size_t pathLength = strlen(_currentPath);
  const size_t separatorLength = isRoot() ? 0 : 1;
  const size_t nameLength = strlen(name);
  if (pathLength + separatorLength + nameLength >=
      sizeof(_openedFilePath)) {
    strncpy(_status, "File path too long", sizeof(_status) - 1);
    drawBrowser();
    return;
  }

  memcpy(_openedFilePath, _currentPath, pathLength);
  size_t pathOffset = pathLength;
  if (separatorLength != 0) {
    _openedFilePath[pathOffset++] = '/';
  }
  memcpy(_openedFilePath + pathOffset, name, nameLength + 1);

  File32 file = sdCard.open(_openedFilePath, O_RDONLY);
  if (!file) {
    strncpy(_status, "Cannot open TXT file", sizeof(_status) - 1);
    drawBrowser();
    return;
  }

  const uint32_t fileSize = file.size();
  if (fileSize > TextBufferCapacity) {
    file.close();
    snprintf(_status, sizeof(_status), "TXT limit: %u bytes",
             static_cast<unsigned>(TextBufferCapacity));
    drawBrowser();
    return;
  }

  const int bytesRead = file.read(_textBuffer, fileSize);
  const bool closed = file.close();
  if (bytesRead != static_cast<int>(fileSize) || !closed) {
    strncpy(_status, "TXT read failed", sizeof(_status) - 1);
    drawBrowser();
    return;
  }

  uint16_t normalizedLength = 0;
  for (uint16_t i = 0; i < fileSize; ++i) {
    char character = _textBuffer[i];
    if (character == '\r') {
      if (i + 1 < fileSize && _textBuffer[i + 1] == '\n') {
        continue;
      }
      character = '\n';
    }
    _textBuffer[normalizedLength++] = character;
  }
  _textLength = normalizedLength;
  _textBuffer[_textLength] = '\0';
  _textCursor = _textLength;
  _textDirty = false;
  _textEditorActive = true;
  _shiftActive = false;
  snprintf(_status, sizeof(_status), "Loaded %u bytes",
           static_cast<unsigned>(_textLength));
  drawTextEditor();
}

void FileBrowserApp::saveTextFile() {
  if (!_textDirty) {
    strncpy(_status, "No changes", sizeof(_status) - 1);
    drawEditorFooter();
    return;
  }

  File32 file = sdCard.open(_openedFilePath, O_WRONLY | O_TRUNC);
  if (!file) {
    strncpy(_status, "Cannot save TXT file", sizeof(_status) - 1);
    drawEditorFooter();
    return;
  }

  const size_t bytesWritten = file.write(
      reinterpret_cast<const uint8_t *>(_textBuffer), _textLength);
  const bool synced = bytesWritten == _textLength && file.sync();
  const bool closed = file.close();
  if (!synced || !closed) {
    strncpy(_status, "Save failed", sizeof(_status) - 1);
  } else {
    _textDirty = false;
    snprintf(_status, sizeof(_status), "Saved %u bytes",
             static_cast<unsigned>(_textLength));
  }
  drawEditorFooter();
}

void FileBrowserApp::leaveTextEditor() {
  _textEditorActive = false;
  _textDirty = false;
  _openedFilePath[0] = '\0';
  _status[0] = '\0';
  refreshDirectory();
}

void FileBrowserApp::insertTextCharacter(char character) {
  if (_textLength >= TextBufferCapacity) {
    strncpy(_status, "1 KiB limit reached", sizeof(_status) - 1);
    drawEditorFooter();
    return;
  }

  const bool shiftWasActive = _shiftActive;
  if (_shiftActive && character >= 'a' && character <= 'z') {
    character = static_cast<char>(character - ('a' - 'A'));
  }
  _shiftActive = false;
  memmove(_textBuffer + _textCursor + 1, _textBuffer + _textCursor,
          _textLength - _textCursor + 1);
  _textBuffer[_textCursor++] = character;
  ++_textLength;
  _textDirty = true;
  _status[0] = '\0';
  drawEditorTextArea();
  drawEditorFooter();
  if (shiftWasActive) {
    drawEditorKey(_display, 4, EditorKeyboardTop + 48, 42, 22, "SHIFT",
                  false);
  }
}

void FileBrowserApp::eraseTextCharacter() {
  if (_textCursor == 0) {
    return;
  }

  memmove(_textBuffer + _textCursor - 1, _textBuffer + _textCursor,
          _textLength - _textCursor + 1);
  --_textCursor;
  --_textLength;
  _textDirty = true;
  _status[0] = '\0';
  drawEditorTextArea();
  drawEditorFooter();
}

void FileBrowserApp::moveTextCursor(int8_t direction) {
  if (direction < 0 && _textCursor > 0) {
    --_textCursor;
  } else if (direction > 0 && _textCursor < _textLength) {
    ++_textCursor;
  }
  drawEditorTextArea();
}

void FileBrowserApp::handleTextEditorTouch(const ScreenPoint &point) {
  static const char *const kFirstRow = "qwertyuiop";
  static const char *const kSecondRow = "asdfghjkl";
  static const char *const kThirdRow = "zxcvbnm,.";
  static const char *const kNumberRow = "1234567890";

  if (point.y < HeaderHeight) {
    if (point.x < 54) {
      if (_textDirty) {
        strncpy(_status, "Save or Cancel first", sizeof(_status) - 1);
        drawEditorFooter();
      } else {
        leaveTextEditor();
      }
    } else if (point.x >= 264) {
      saveTextFile();
    }
    return;
  }

  if (point.y >= FooterTop && point.x < 76) {
    leaveTextEditor();
    return;
  }

  if (point.y >= EditorTextTop &&
      point.y < EditorTextTop + EditorTextHeight) {
    const int16_t rowOffset = point.y - EditorTextTop - 3;
    uint8_t targetRow = rowOffset <= 0
                            ? 0
                            : static_cast<uint8_t>(rowOffset /
                                                   kEditorLineHeight);
    if (targetRow >= kTextRows) {
      targetRow = kTextRows - 1;
    }
    uint8_t targetColumn =
        point.x <= kEditorTextLeft
            ? 0
            : static_cast<uint8_t>((point.x - kEditorTextLeft) / 6);
    if (targetColumn > kTextColumns) {
      targetColumn = kTextColumns;
    }

    uint16_t position = _textViewStart;
    uint8_t row = 0;
    uint8_t column = 0;
    while (position < _textLength && row < targetRow) {
      const char character = _textBuffer[position];
      if (character == '\r') {
        ++position;
      } else if (character == '\n') {
        ++position;
        ++row;
        column = 0;
      } else {
        if (column == kTextColumns) {
          ++row;
          column = 0;
          continue;
        }
        ++position;
        ++column;
      }
    }
    while (position < _textLength && row == targetRow &&
           column < targetColumn) {
      const char character = _textBuffer[position];
      if (character == '\r') {
        ++position;
      } else if (character == '\n' || column == kTextColumns) {
        break;
      } else {
        ++position;
        ++column;
      }
    }
    _textCursor = position;
    drawEditorTextArea();
    return;
  }

  if (point.y >= EditorKeyboardTop &&
      point.y < EditorKeyboardTop + 22) {
    if (point.x >= 5 && point.x < 315) {
      const uint8_t index = static_cast<uint8_t>((point.x - 5) / 31);
      if (index < 10) {
        insertTextCharacter(kFirstRow[index]);
      }
    }
    return;
  }

  if (point.y >= EditorKeyboardTop + 24 &&
      point.y < EditorKeyboardTop + 46) {
    if (point.x >= 17 && point.x < 305) {
      const uint8_t index = static_cast<uint8_t>((point.x - 17) / 32);
      if (index < 9) {
        insertTextCharacter(kSecondRow[index]);
      }
    }
    return;
  }

  if (point.y >= EditorKeyboardTop + 48 &&
      point.y < EditorKeyboardTop + 70) {
    if (point.x < 46) {
      _shiftActive = !_shiftActive;
      drawEditorKey(_display, 4, EditorKeyboardTop + 48, 42, 22, "SHIFT",
                    _shiftActive);
    } else if (point.x >= 50) {
      const uint8_t index = static_cast<uint8_t>((point.x - 50) / 30);
      if (index < 9) {
        insertTextCharacter(kThirdRow[index]);
      }
    }
    return;
  }

  if (point.y >= EditorKeyboardTop + 72 &&
      point.y < EditorKeyboardTop + 94) {
    if (point.x >= 5 && point.x < 315) {
      const uint8_t index = static_cast<uint8_t>((point.x - 5) / 31);
      if (index < 10) {
        insertTextCharacter(kNumberRow[index]);
      }
    }
    return;
  }

  if (point.y >= EditorKeyboardTop + 96 &&
      point.y < EditorKeyboardTop + 120) {
    if (point.x < 44) {
      moveTextCursor(-1);
    } else if (point.x < 84) {
      moveTextCursor(1);
    } else if (point.x < 186) {
      insertTextCharacter(' ');
    } else if (point.x < 246) {
      insertTextCharacter('\n');
    } else {
      eraseTextCharacter();
    }
  }
}

void FileBrowserApp::refreshDirectory(bool redrawPage) {
  _visibleEntryCount = 0;
  _entryCount = 0;
  _status[0] = '\0';

  digitalWrite(_displayChipSelect, HIGH);
  digitalWrite(_sdChipSelect, HIGH);
  File32 directory = sdCard.open(_currentPath, O_RDONLY);
  if (!directory || !directory.isDirectory()) {
    if (directory) {
      directory.close();
    }
    strncpy(_status, "Cannot open folder", sizeof(_status) - 1);
    drawBrowser();
    return;
  }

  File32 entry;
  while (entry.openNext(&directory, O_RDONLY)) {
    char name[EntryNameLength]{};
    if (entry.getName(name, sizeof(name)) == 0 &&
        entry.getSFN(name, sizeof(name)) == 0) {
      strncpy(name, "(unnamed)", sizeof(name) - 1);
    }

    if (_entryCount >= _scrollOffset &&
        _visibleEntryCount < VisibleEntryCount) {
      Entry &visibleEntry = _entries[_visibleEntryCount++];
      strncpy(visibleEntry.name, name, sizeof(visibleEntry.name) - 1);
      visibleEntry.name[sizeof(visibleEntry.name) - 1] = '\0';
      visibleEntry.isDirectory = entry.isDirectory();
      visibleEntry.size = entry.size();
    }
    if (_entryCount < UINT16_MAX) {
      ++_entryCount;
    }
    entry.close();
  }
  const uint8_t directoryError = directory.getError();
  directory.close();

  if (directoryError != 0) {
    snprintf(_status, sizeof(_status), "Folder read error: %02X",
             directoryError);
  } else {
    _status[0] = '\0';
  }
  if (redrawPage) {
    drawBrowser();
  } else {
    drawBrowserRows();
  }
}

void FileBrowserApp::enterDirectory(const char *name) {
  const size_t pathLength = strlen(_currentPath);
  const size_t separatorLength = isRoot() ? 0 : 1;
  const size_t nameLength = strlen(name);
  if (pathLength + separatorLength + nameLength >= sizeof(_currentPath)) {
    strncpy(_status, "Folder path too long", sizeof(_status) - 1);
    drawBrowser();
    return;
  }

  size_t pathOffset = pathLength;
  if (separatorLength != 0) {
    _currentPath[pathOffset++] = '/';
  }
  memcpy(_currentPath + pathOffset, name, nameLength + 1);
  _scrollOffset = 0;
  refreshDirectory();
}

void FileBrowserApp::navigateToParent() {
  char *lastSlash = strrchr(_currentPath, '/');
  if (lastSlash == nullptr || lastSlash == _currentPath) {
    strncpy(_currentPath, "/", sizeof(_currentPath));
  } else {
    *lastSlash = '\0';
  }
  _scrollOffset = 0;
  refreshDirectory();
}

bool FileBrowserApp::isRoot() const {
  return _currentPath[0] == '/' && _currentPath[1] == '\0';
}

}  // namespace drawing
