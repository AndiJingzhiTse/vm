module;
#include <ncurses.h>
export module Screen;

import <string>;
import <vector>;
import <utility>;
import <iostream>;
import <fstream>;
import <algorithm>;
import Buffer;

export class Screen {
  // Displayable lines (pre-processed with wrapping)
  std::vector<std::string> displayLines;
  size_t firstRowDisplayed = 0; // Index into displayLines
  std::string currentFilename;  // Store filename for syntax highlighting

  // Rebuild displayLines from buffer
  void rebuildDisplayLines(const Buffer &buffer);

  // Map buffer coordinates to display line coordinates
  std::pair<size_t, size_t> bufferToDisplay(size_t bufferRow, size_t bufferCol,
                                            const Buffer &buffer) const;

  // Map display row to buffer row (returns buffer row and column 0)
  size_t displayToBufferRow(size_t displayRow, const Buffer &buffer) const;

  // Adjust firstRowDisplayed to keep cursor visible
  void ensureCursorVisible(size_t displayRow);

  // Center cursor on screen (set firstRowDisplayed so cursor is in center)
  void centerCursorOnScreen(size_t displayRow);

  // Internal draw methods
  void drawBuffer(const Buffer &buffer, const std::string &filename,
                  bool syntaxHighlightingEnabled);
  void drawCursor(size_t displayRow, size_t displayCol);

public:
  Screen();
  ~Screen();
  std::string statusBarLeft = "";
  std::string statusBarRight = "";
  void drawText(int row, int col, const std::string &s);
  int getKey();
  void refreshScreen();
  void clear();
  void draw(const Buffer &b, size_t bufferRow, size_t bufferCol,
            bool cursorVisible = true, const std::string &filename = "",
            bool syntaxHighlightingEnabled = true);
  void drawStatusBar();
  void highlightInterval(const Buffer &buffer, const Interval &interval);
  void drawStatusBarCursor();

  // Scrolling methods - return new buffer row for cursor
  // These scroll the screen and move cursor, centering it
  size_t scrollUpFullPage(size_t currentBufferRow, const Buffer &buffer);
  size_t scrollDownFullPage(size_t currentBufferRow, const Buffer &buffer);
  size_t scrollUpHalfPage(size_t currentBufferRow, const Buffer &buffer);
  size_t scrollDownHalfPage(size_t currentBufferRow, const Buffer &buffer);

  // Helper to get screen height
  int getScreenHeight() const;
};
