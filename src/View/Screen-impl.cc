module;
#include <curses.h>
module Screen;

Screen::Screen() {
  initscr();              // Start ncurses mode
  set_escdelay(25);       // Set ESC delay to 25ms (instead of default 1000ms)
  cbreak();               // Disable line buffering
  noecho();               // Don't echo typed characters
  keypad(stdscr, TRUE);   // Enable function keys (arrows, F1, etc.)
  nodelay(stdscr, FALSE); // BLOCKING mode
  curs_set(0);            // Hide the terminal cursor (vim style)

  // Initialize colors if terminal supports it
  if (has_colors()) {
    start_color();

    // Check if terminal supports custom colors
    // If supported, define custom colors from hex values
    // #E394D0 = RGB(227, 148, 208) for strings
    // #66D2CE = RGB(102, 210, 206) for keywords
    // #5F6357 = RGB(95, 99, 87) for comments
    // ncurses uses 0-1000 scale for RGB

    if (can_change_color()) {
      // Redefine standard colors or use extended colors if available
      // String color: #E394D0 = RGB(227, 148, 208) -> (890, 580, 816)
      // Keyword color: #66D2CE = RGB(102, 210, 206) -> (400, 824, 808)
      // Comment color: #5F6357 = RGB(95, 99, 87) -> (373, 388, 341)
      init_color(COLOR_BLACK, 78, 78, 78); // #141414 (RGB 20,20,20)

      if (COLORS >= 16) {
        // Use extended color numbers if available
        init_color(16, 890, 580, 816); // String color (#E394D0)
        init_color(17, 400, 824, 808); // Keyword color (#66D2CE)
        init_color(18, 373, 388, 341); // Comment color (#5F6357)

        init_pair(1, 16, COLOR_BLACK); // String literals (#E394D0)
        init_pair(3, 17, COLOR_BLACK); // Keywords (#66D2CE)
        init_pair(5, 18, COLOR_BLACK); // Comments (#5F6357)
      } else {
        // Redefine standard colors (0-7) - use colors that aren't used
        // elsewhere Use COLOR_MAGENTA for strings, COLOR_CYAN for keywords,
        // COLOR_YELLOW for comments
        init_color(COLOR_MAGENTA, 890, 580, 816); // String color (#E394D0)
        init_color(COLOR_CYAN, 400, 824, 808);    // Keyword color (#66D2CE)
        init_color(COLOR_YELLOW, 373, 388, 341);  // Comment color (#5F6357)

        init_pair(1, COLOR_MAGENTA, COLOR_BLACK); // String literals (#E394D0)
        init_pair(3, COLOR_CYAN, COLOR_BLACK);    // Keywords (#66D2CE)
        init_pair(5, COLOR_YELLOW, COLOR_BLACK);  // Comments (#5F6357)
      }

      init_pair(2, COLOR_MAGENTA, COLOR_BLACK); // Numeric literals
      init_pair(4, COLOR_WHITE, COLOR_BLACK);   // Identifiers (white)
      init_pair(6, COLOR_RED, COLOR_BLACK);     // Preprocessor directives
      init_pair(7, COLOR_WHITE, COLOR_BLACK);   // Default/normal text
      init_pair(8, COLOR_RED, COLOR_BLACK);     // Mismatched braces
    } else {
      // Fallback to standard colors if custom colors not supported
      init_pair(1, COLOR_MAGENTA,
                COLOR_BLACK); // String literals (closest to #E394D0)
      init_pair(2, COLOR_MAGENTA, COLOR_BLACK); // Numeric literals
      init_pair(3, COLOR_CYAN, COLOR_BLACK);    // Keywords (closest to #66D2CE)
      init_pair(4, COLOR_WHITE, COLOR_BLACK);   // Identifiers (white)
      init_pair(5, COLOR_WHITE, COLOR_BLACK);   // Comments (closest to #5F6357)
      init_pair(6, COLOR_RED, COLOR_BLACK);     // Preprocessor directives
      init_pair(7, COLOR_WHITE, COLOR_BLACK);   // Default/normal text
      init_pair(8, COLOR_RED, COLOR_BLACK);     // Mismatched braces
    }

    // Color pairs for matching braces (cycling through colors)
    init_pair(9, COLOR_CYAN, COLOR_BLACK);     // Brace color 1
    init_pair(10, COLOR_YELLOW, COLOR_BLACK);  // Brace color 2
    init_pair(11, COLOR_MAGENTA, COLOR_BLACK); // Brace color 3
    init_pair(12, COLOR_GREEN, COLOR_BLACK);   // Brace color 4
    init_pair(13, COLOR_BLUE, COLOR_BLACK);    // Brace color 5
    init_pair(14, COLOR_WHITE, COLOR_BLACK);   // Brace color 6
    init_pair(15, COLOR_CYAN, COLOR_BLACK);    // Brace color 7 (cycle back)
    init_pair(16, COLOR_YELLOW, COLOR_BLACK);  // Brace color 8
  }

  refreshScreen();
}

void Screen::rebuildDisplayLines(const Buffer &buffer) {
  displayLines.clear();
  int rows, cols;
  getmaxyx(stdscr, rows, cols);
  (void)rows;

  for (size_t i = 0; i < buffer.lineCount(); i++) {
    const std::string &line = buffer.getLine(i);

    if (line.length() > static_cast<size_t>(cols)) {
      // Line wraps - split into multiple display lines
      size_t start = 0;
      while (start < line.length()) {
        size_t end = std::min(start + cols, line.length());
        displayLines.push_back(line.substr(start, end - start));
        start += cols;
      }
    } else {
      // Line doesn't wrap - add as single display line
      displayLines.push_back(line);
    }
  }
}

std::pair<size_t, size_t> Screen::bufferToDisplay(size_t bufferRow,
                                                  size_t bufferCol,
                                                  const Buffer &buffer) const {
  int rows, cols;
  getmaxyx(stdscr, rows, cols);
  (void)rows;

  size_t displayRow = 0;

  // Count display lines for all buffer rows before cursorRow
  for (size_t i = 0; i < bufferRow && i < buffer.lineCount(); i++) {
    const std::string &line = buffer.getLine(i);
    if (line.length() > static_cast<size_t>(cols)) {
      size_t numSegments = (line.length() + cols - 1) / cols;
      displayRow += numSegments;
    } else {
      displayRow++;
    }
  }

  // Add the segment index for current line if it wraps
  size_t displayCol = bufferCol;
  if (bufferRow < buffer.lineCount()) {
    const std::string &line = buffer.getLine(bufferRow);
    // Clamp bufferCol to line length to prevent out-of-bounds access
    size_t clampedCol = std::min(bufferCol, line.length());
    if (line.length() > static_cast<size_t>(cols)) {
      size_t segment = clampedCol / cols;
      displayRow += segment;
      displayCol = clampedCol % cols;
    } else {
      displayCol = clampedCol;
    }
  }

  return {displayRow, displayCol};
}

size_t Screen::displayToBufferRow(size_t displayRow,
                                  const Buffer &buffer) const {
  int rows, cols;
  getmaxyx(stdscr, rows, cols);
  (void)rows;

  size_t currentDisplayRow = 0;

  // Find which buffer row corresponds to this display row
  for (size_t bufferRow = 0; bufferRow < buffer.lineCount(); bufferRow++) {
    const std::string &line = buffer.getLine(bufferRow);
    size_t segments = (line.length() > static_cast<size_t>(cols))
                          ? ((line.length() + cols - 1) / cols)
                          : 1;

    if (currentDisplayRow + segments > displayRow) {
      return bufferRow;
    }
    currentDisplayRow += segments;
  }

  // If displayRow is beyond all buffer rows, return last buffer row
  return buffer.lineCount() > 0 ? buffer.lineCount() - 1 : 0;
}

void Screen::ensureCursorVisible(size_t displayRow) {
  int rows, cols;
  getmaxyx(stdscr, rows, cols);
  int maxDisplayRow = rows - 2; // Exclude status bar
  int lastRowDisplayed = firstRowDisplayed + maxDisplayRow;

  if (displayRow < firstRowDisplayed) {
    // Cursor is above visible area - scroll up
    firstRowDisplayed = displayRow;
  } else if (displayRow > static_cast<size_t>(lastRowDisplayed)) {
    // Cursor is below visible area - scroll down
    firstRowDisplayed = displayRow - maxDisplayRow;
  }
}

void Screen::centerCursorOnScreen(size_t displayRow) {
  int rows, cols;
  getmaxyx(stdscr, rows, cols);
  int screenHeight = rows - 2; // Exclude status bar
  int centerRow = screenHeight / 2;

  // Set firstRowDisplayed so that displayRow is at centerRow
  if (displayRow >= static_cast<size_t>(centerRow)) {
    firstRowDisplayed = displayRow - centerRow;
  } else {
    firstRowDisplayed = 0;
  }
}

int Screen::getScreenHeight() const {
  int rows, cols;
  getmaxyx(stdscr, rows, cols);
  return rows - 2; // Exclude status bar
}

// Helper function to get color pair for token
static int getColorPair(const Token &token) {
  // Check if this is a brace with a color index assigned
  if (token.braceColorIndex >= 0) {
    // Return color pair 9-16 based on color index (0-7 maps to 9-16)
    return 9 + (token.braceColorIndex % 8);
  }

  switch (token.type) {
  case TokenType::KEYWORD:
    return 3; // Green (changed from blue)
  case TokenType::NUMBER:
    return 2; // Magenta
  case TokenType::STRING:
    return 1; // Blue (swapped with keywords)
  case TokenType::IDENTIFIER:
    return 4; // White
  case TokenType::COMMENT:
    return 5; // Yellow
  case TokenType::PREPROCESSOR:
    return 6; // Red
  case TokenType::OPEN_CURLY_BRACE:
  case TokenType::CLOSE_CURLY_BRACE:
  case TokenType::OPEN_SQUARE_BRACE:
  case TokenType::CLOSE_SQUARE_BRACE:
  case TokenType::OPEN_PARENTHESIS:
  case TokenType::CLOSE_PARENTHESIS:
  case TokenType::OPEN_ANGLE_BRACE:
  case TokenType::CLOSE_ANGLE_BRACE:
    return 7; // White/normal (angle brackets not highlighted)
  case TokenType::MISMATCHED_BRACE:
    return 8; // Red for mismatched braces
  case TokenType::WHITESPACE:
  case TokenType::NORMAL:
  default:
    return 7; // White/normal
  }
}

void Screen::drawBuffer(const Buffer &buffer, const std::string &filename,
                        bool syntaxHighlightingEnabled) {
  int rows, cols;
  getmaxyx(stdscr, rows, cols);
  int maxDisplayRow = rows - 2;
  int numRowsToDisplay = maxDisplayRow + 1;

  werase(stdscr);

  // Store filename in member variable so it persists across calls
  if (!filename.empty()) {
    currentFilename = filename;
  }

  // Check if we should highlight (C++ files and syntax highlighting is enabled)
  bool shouldHighlight = false;
  if (syntaxHighlightingEnabled && !currentFilename.empty()) {
    size_t len = currentFilename.length();
    if ((len >= 2 && currentFilename.substr(len - 2) == ".h") ||
        (len >= 3 && currentFilename.substr(len - 3) == ".cc")) {
      shouldHighlight = true;
    }
  }

  // Get tokens if highlighting is enabled
  std::vector<std::vector<Token>> allTokens;
  if (shouldHighlight) {
    allTokens = buffer.getTokens();
  }

  // Draw visible display lines
  for (int i = 0; i < numRowsToDisplay; i++) {
    size_t displayLineIndex = firstRowDisplayed + i;

    if (displayLineIndex < displayLines.size()) {
      const std::string &displayLine = displayLines[displayLineIndex];

      if (shouldHighlight && !allTokens.empty()) {
        // Find which buffer line this display line comes from
        size_t bufferRow = displayToBufferRow(displayLineIndex, buffer);

        if (bufferRow < allTokens.size()) {
          const std::string &bufferLine = buffer.getLine(bufferRow);
          const std::vector<Token> &lineTokens = allTokens[bufferRow];

          // Calculate which segment of the buffer line this display line
          // represents (for wrapped lines)
          size_t bufferLineStartCol = 0;
          if (bufferLine.length() > static_cast<size_t>(cols)) {
            // Find which segment this display line is
            size_t currentDisplayRow = 0;
            for (size_t br = 0; br < bufferRow; ++br) {
              const std::string &bl = buffer.getLine(br);
              if (bl.length() > static_cast<size_t>(cols)) {
                currentDisplayRow += (bl.length() + cols - 1) / cols;
              } else {
                currentDisplayRow++;
              }
            }
            // Now find which segment of bufferRow this displayLineIndex is
            size_t segment = displayLineIndex - currentDisplayRow;
            bufferLineStartCol = segment * cols;
          }

          size_t bufferLineEndCol =
              std::min(bufferLineStartCol + cols, bufferLine.length());

          // Draw with syntax highlighting
          // Build a map of column -> token type for efficient lookup
          std::vector<TokenType> colTokenTypes(displayLine.length(),
                                               TokenType::NORMAL);

          for (const Token &token : lineTokens) {
            // Check if token overlaps with this display segment
            if (token.end <= bufferLineStartCol ||
                token.start >= bufferLineEndCol) {
              continue; // Token doesn't overlap with this display segment
            }

            // Calculate token bounds for this display segment
            size_t tokenStart = std::max(token.start, bufferLineStartCol);
            size_t tokenEnd = std::min(token.end, bufferLineEndCol);

            // Map to display column positions
            size_t displayTokenStart = tokenStart - bufferLineStartCol;
            size_t displayTokenEnd = tokenEnd - bufferLineStartCol;

            // Mark columns with this token type
            for (size_t c = displayTokenStart;
                 c < displayTokenEnd && c < colTokenTypes.size(); ++c) {
              colTokenTypes[c] = token.type;
            }
          }

          // Draw the line character by character with appropriate colors
          // Build a map of column -> token for brace color lookup
          std::vector<const Token *> colTokens(displayLine.length(), nullptr);
          for (const Token &token : lineTokens) {
            // Check if token overlaps with this display segment
            if (token.end <= bufferLineStartCol ||
                token.start >= bufferLineEndCol) {
              continue;
            }
            size_t tokenStart = std::max(token.start, bufferLineStartCol);
            size_t tokenEnd = std::min(token.end, bufferLineEndCol);
            size_t displayTokenStart = tokenStart - bufferLineStartCol;
            size_t displayTokenEnd = tokenEnd - bufferLineStartCol;
            for (size_t c = displayTokenStart;
                 c < displayTokenEnd && c < colTokens.size(); ++c) {
              colTokens[c] = &token;
            }
          }

          int currentColorPair = 7; // Default
          attron(COLOR_PAIR(currentColorPair));

          for (size_t c = 0; c < displayLine.length(); ++c) {
            int newColorPair;
            // If this column has a token with brace color, use it; otherwise
            // use token type color
            if (colTokens[c] != nullptr && colTokens[c]->braceColorIndex >= 0) {
              newColorPair = getColorPair(*colTokens[c]);
            } else {
              // Create a temporary token for type-based coloring
              Token tempToken = {colTokenTypes[c], 0, 0, -1};
              newColorPair = getColorPair(tempToken);
            }
            if (newColorPair != currentColorPair) {
              attroff(COLOR_PAIR(currentColorPair));
              currentColorPair = newColorPair;
              attron(COLOR_PAIR(currentColorPair));
            }
            mvaddch(i, c, displayLine[c]);
          }

          attroff(COLOR_PAIR(currentColorPair));
        } else {
          // Fallback: draw normally if tokenization failed
          mvaddstr(i, 0, displayLine.c_str());
        }
      } else {
        // No highlighting - draw normally
        mvaddstr(i, 0, displayLine.c_str());
      }

      clrtoeol();
    } else {
      // Past end - show ~
      mvaddch(i, 0, '~');
      clrtoeol();
    }
  }
}

void Screen::drawCursor(size_t displayRow, size_t displayCol) {
  int rows, cols;
  getmaxyx(stdscr, rows, cols);
  int maxDisplayRow = rows - 2;

  // Calculate screen position
  int screenRow =
      static_cast<int>(displayRow) - static_cast<int>(firstRowDisplayed);

  if (screenRow >= 0 && screenRow <= maxDisplayRow) {
    if (displayRow < displayLines.size()) {
      const std::string &line = displayLines[displayRow];
      if (displayCol < line.length()) {
        mvaddch(screenRow, displayCol, line[displayCol] | A_REVERSE);
      } else {
        mvaddch(screenRow, displayCol, ' ' | A_REVERSE);
      }
    } else {
      // Cursor past end of display lines
      mvaddch(screenRow, displayCol, ' ' | A_REVERSE);
    }
  }
}

void Screen::draw(const Buffer &buffer, size_t bufferRow, size_t bufferCol,
                  bool cursorVisible, const std::string &filename,
                  bool syntaxHighlightingEnabled) {
  // Rebuild display lines from buffer
  rebuildDisplayLines(buffer);

  // Map buffer coordinates to display coordinates
  auto [displayRow, displayCol] = bufferToDisplay(bufferRow, bufferCol, buffer);

  // Ensure cursor is visible
  ensureCursorVisible(displayRow);

  // Draw everything
  drawBuffer(buffer, filename, syntaxHighlightingEnabled);
  drawStatusBar();

  if (cursorVisible) {
    drawCursor(displayRow, displayCol);
  } else {
    drawStatusBarCursor();
  }

  wrefresh(stdscr);
}
void Screen::drawStatusBarCursor() {
  // Draw the cursor after the current statusBarLeft for command/search modes.
  int rows, cols;
  getmaxyx(stdscr, rows, cols);
  int statusBarRow = rows - 1;
  int cursorCol = static_cast<int>(statusBarLeft.length());
  mvchgat(statusBarRow, cursorCol, 1, A_REVERSE, 0, nullptr);
}
void Screen::highlightInterval(const Buffer &buffer, const Interval &interval) {
  // Highlights the interval [start, end], inclusive
  // Note: This assumes the interval is on a single buffer line
  // Map buffer coordinates to display coordinates
  auto [startDisplayRow, startDisplayCol] =
      bufferToDisplay(interval.start.row, interval.start.col, buffer);
  auto [endDisplayRow, endDisplayCol] =
      bufferToDisplay(interval.end.row, interval.end.col, buffer);

  int rows, cols;
  getmaxyx(stdscr, rows, cols);
  int maxDisplayRow = rows - 2;

  // Highlight each display row that the interval spans
  for (size_t displayRow = startDisplayRow; displayRow <= endDisplayRow;
       displayRow++) {
    int screenRow =
        static_cast<int>(displayRow) - static_cast<int>(firstRowDisplayed);

    if (screenRow < 0 || screenRow > maxDisplayRow ||
        displayRow >= displayLines.size())
      continue;

    const std::string &line = displayLines[displayRow];
    size_t startCol = (displayRow == startDisplayRow) ? startDisplayCol : 0;
    size_t endCol = (displayRow == endDisplayRow)
                        ? endDisplayCol
                        : (line.length() > 0 ? line.length() - 1 : 0);

    // Ensure valid column bounds
    if (startCol > endCol)
      std::swap(startCol, endCol);

    for (size_t c = startCol; c <= endCol && c < line.length(); ++c) {
      mvaddch(screenRow, c, line[c] | A_REVERSE);
    }
  }
}

Screen::~Screen() { endwin(); }
void Screen::drawText(int row, int col, const std::string &s) {
  mvaddstr(row, col, s.c_str()); // CORRECT
}
int Screen::getKey() {
  int ch = getch();
  return ch;
}
void Screen::refreshScreen() { refresh(); }
// void Screen::clear();

void debugLog(const std::string &message) {
  std::ofstream log("/tmp/vm_debug.log", std::ios::app);
  log << message << std::endl;
  log.flush();
  log.close();
}

void Screen::drawStatusBar() {
  int rows, cols;
  getmaxyx(stdscr, rows, cols);
  int y = rows - 1;

  debugLog("statusBarLeft: " + statusBarLeft);
  debugLog("statusBarRight: " + statusBarRight);
  mvaddstr(y, 0, statusBarLeft.c_str());
  mvaddstr(y, cols - statusBarRight.length(), statusBarRight.c_str());
}

size_t Screen::scrollUpFullPage(size_t currentBufferRow, const Buffer &buffer) {
  // Get current display row
  auto [currentDisplayRow, currentDisplayCol] =
      bufferToDisplay(currentBufferRow, 0, buffer);

  int screenHeight = getScreenHeight();

  // Move cursor up by one screen height in display coordinates
  size_t newDisplayRow =
      (currentDisplayRow >= static_cast<size_t>(screenHeight))
          ? currentDisplayRow - screenHeight
          : 0;

  // Convert back to buffer row
  size_t newBufferRow = displayToBufferRow(newDisplayRow, buffer);

  // Center cursor on screen
  rebuildDisplayLines(buffer);
  auto [newDisplayRow2, _] = bufferToDisplay(newBufferRow, 0, buffer);
  centerCursorOnScreen(newDisplayRow2);

  return newBufferRow;
}

size_t Screen::scrollDownFullPage(size_t currentBufferRow,
                                  const Buffer &buffer) {
  // Get current display row
  rebuildDisplayLines(buffer);
  auto [currentDisplayRow, currentDisplayCol] =
      bufferToDisplay(currentBufferRow, 0, buffer);

  int screenHeight = getScreenHeight();

  // Move cursor down by one screen height in display coordinates
  size_t maxDisplayRow = displayLines.size() > 0 ? displayLines.size() - 1 : 0;
  size_t newDisplayRow =
      std::min(currentDisplayRow + screenHeight, maxDisplayRow);

  // Convert back to buffer row
  size_t newBufferRow = displayToBufferRow(newDisplayRow, buffer);

  // Center cursor on screen
  centerCursorOnScreen(newDisplayRow);

  return newBufferRow;
}

size_t Screen::scrollUpHalfPage(size_t currentBufferRow, const Buffer &buffer) {
  // Get current display row
  auto [currentDisplayRow, currentDisplayCol] =
      bufferToDisplay(currentBufferRow, 0, buffer);

  int screenHeight = getScreenHeight();
  int halfPage = screenHeight / 2;

  // Move cursor up by half screen height in display coordinates
  size_t newDisplayRow = (currentDisplayRow >= static_cast<size_t>(halfPage))
                             ? currentDisplayRow - halfPage
                             : 0;

  // Convert back to buffer row
  size_t newBufferRow = displayToBufferRow(newDisplayRow, buffer);

  // Center cursor on screen
  rebuildDisplayLines(buffer);
  auto [newDisplayRow2, _] = bufferToDisplay(newBufferRow, 0, buffer);
  centerCursorOnScreen(newDisplayRow2);

  return newBufferRow;
}

size_t Screen::scrollDownHalfPage(size_t currentBufferRow,
                                  const Buffer &buffer) {
  // Get current display row
  rebuildDisplayLines(buffer);
  auto [currentDisplayRow, currentDisplayCol] =
      bufferToDisplay(currentBufferRow, 0, buffer);

  int screenHeight = getScreenHeight();
  int halfPage = screenHeight / 2;

  // Move cursor down by half screen height in display coordinates
  size_t maxDisplayRow = displayLines.size() > 0 ? displayLines.size() - 1 : 0;
  size_t newDisplayRow = std::min(currentDisplayRow + halfPage, maxDisplayRow);

  // Convert back to buffer row
  size_t newBufferRow = displayToBufferRow(newDisplayRow, buffer);

  // Center cursor on screen
  centerCursorOnScreen(newDisplayRow);

  return newBufferRow;
}