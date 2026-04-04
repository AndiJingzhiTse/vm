module;
#include <ncurses.h>
export module Editor;
import Buffer;
import Screen;
// import IMode;
// import Modes;
import <cstddef>;
import <memory>;
import <string>;
import <optional>;
import <set>;
import <iostream>;
import <fstream>;
import <utility>;
import <stack>;
import <vector>;
import <cctype>;
import <unordered_map>;
import <sys/stat.h>;
import <unistd.h>;

// module;
// #include "IMode.cc"
// #include "Modes.cc"
// export module Editor;

// import Buffer;
// import Screen;
// import <cstddef>;
// import <memory>;
// import <string>;

// class IMode;
// class NormalMode;
// class EditMode;
export class Editor {
  class IMode {
  public:
    // handleKey is how the mode handles the key stroke
    virtual void handleKey(Editor &e, int key) = 0;
    // onKeyStroke calls handleKey and updates things accordingly
    virtual void onKeyStroke(Editor &e, int key) = 0;
    // Returns true if the cursor should be drawn in the buffer
    virtual bool shouldDrawBufferCursor() const { return true; }
    // Returns true if this mode supports search result highlighting
    virtual bool shouldHighLightSearchResult() const { return false; }
  };

  class InsertMode : public IMode {
  public:
    void onKeyStroke(Editor &e, int key) override {
      handleKey(e, key);
      e.screen->statusBarRight = e.getCoor1Indexed();
    }
    void handleKey(Editor &e, int key) override {
      // ESC goes back to normal mode
      if (key == 27) { // int for ESC
        // idk why the real vim would shift left by 1
        // when entering normal mode from insert mode
        if (e.cursorCol > 0) {
          e.cursorCol = e.cursorCol - 1;
        }
        e.screen->statusBarLeft = "";
        e.enterNormalMode();
        return;
      }

      if (key == 127 || key == KEY_BACKSPACE) {
        e.backspace();
        return;
      }

      if (key == 10) { // ENTER
        e.breakLine();
        return;
      }
      if (key == KEY_UP) {
        // Move cursor up
        if (e.cursorRow > 0) {
          e.cursorRow--;
          e.cursorCol = std::min(e.cursorCol,
                                 e.buffer->getLine(e.cursorRow).length() - 1);
        }
        return;
      }

      if (key == KEY_DOWN) {
        // Move cursor down
        if (e.cursorRow < e.buffer->lineCount() - 1) {
          e.cursorRow++;
          e.cursorCol = std::min(e.cursorCol,
                                 e.buffer->getLine(e.cursorRow).length() - 1);
        }
        return;
      }

      if (key == KEY_LEFT) {
        // Move cursor left
        if (e.cursorCol > 0) {
          e.cursorCol--;
        }
        // else if (e.cursorRow > 0) {
        //   // Move to end of previous line
        //   e.cursorRow--;
        //   e.cursorCol = e.buffer->getLine(e.cursorRow).length();
        // }
        return;
      }

      if (key == KEY_RIGHT) {
        // Move cursor right
        std::string &line = e.buffer->getLine(e.cursorRow);
        if (e.cursorCol < line.length()) {
          e.cursorCol++;
        }
        // else if (e.cursorRow < e.buffer->lineCount() - 1) {
        //   // Move to start of next line
        //   e.cursorRow++;
        //   e.cursorCol = 0;
        // }
        return;
      }

      // any printable char is inserted
      if (isprint(key)) {
        e.insertChar((char)key);
        return;
      }
    }
    // ~InsertMode() override
    // {
    // }
  };
  class NormalMode : public IMode {

  public:
    struct Command {
      int multiplier1;
      std::optional<char> op;
      int multiplier2;
      std::optional<char> motion;
      Command()
          : multiplier1{0}, op{std::nullopt}, multiplier2{0},
            motion{std::nullopt} {}
      void print() const {
        std::cout << "Multiplier1: " << multiplier1 << "\n"
                  << "Operator: " << op.value_or(' ') << "\n"
                  << "Multiplier2: " << multiplier2 << "\n"
                  << "Motion: " << motion.value_or(' ') << "\n";
        std::cout.flush();
      }
    };
    Command command;
    char searchChar;
    bool waitForSearchChar;
    char replaceChar;
    bool waitForReplaceChar;
    bool lineSearchForward;
    bool hasLineSearched;

    std::optional<Command> lastCommand;
    char lastReplaceChar = '\0';       // Store replace char for 'r' commands
    char lastSearchChar = '\0';        // Store search char for 'f'/'F' commands
    bool lastLineSearchForward = true; // Store direction for 'f'/'F'

    static const std::set<char> validOperators;
    static const std::set<char> validMotions;
    static const std::set<char> standaloneCommands;
    static const std::set<char> invalidStandaloneMotions;

    bool waitingForMacroRegister = false;
    bool waitingForMacroReplayRegister = false;

    void onKeyStroke(Editor &e, int key) override {
      IMode *oldMode = e.mode.get();
      handleKey(e, key);
      // Only update if we're still in NormalMode
      if (e.mode.get() == oldMode) {
        updateStatusBarCommand(e);
      }
    }

    void handleKey(Editor &e, int key) override {
      // Handle macro recording - check early but allow command building
      if (e.isRecordingMacro) {
        if (key == (int)'q') {
          e.isRecordingMacro = false;
          e.screen->statusBarLeft = "";
          return; // Stop recording, don't process 'q' further
        }
        // Record the key first
        e.macroMap[e.currentMacroRegister].push_back(key);
        // Continue processing to build command state, but we'll prevent
        // execution
      }

      if (waitForSearchChar && isprint(key)) {
        waitForSearchChar = false;
        hasLineSearched = true;
        searchChar = (char)key;
        // Store for '.' command replay
        lastSearchChar = searchChar;
        lastLineSearchForward = lineSearchForward;
        executeCommand(e);
        return;
      }
      if (waitForReplaceChar && isprint(key)) {
        waitForReplaceChar = false;
        replaceChar = (char)key;
        // Store for '.' command replay
        lastReplaceChar = replaceChar;
        executeCommand(e);
        return;
      }

      // Handle waiting for macro register to start recording
      if (waitingForMacroRegister) {
        if (key == 27) { // ESC - cancel
          waitingForMacroRegister = false;
          e.screen->statusBarLeft = "";
          clearCommand();
          return;
        }
        if (isprint(key)) {
          e.isRecordingMacro = true;
          e.currentMacroRegister = key;
          e.macroMap[key].clear(); // Clear existing macro for this register
          e.screen->statusBarLeft = "recording @" + std::string(1, (char)key);
          waitingForMacroRegister = false;
          clearCommand();
          return;
        }
        // For non-printable keys, just ignore them (don't process, but don't
        // freeze)
        return; // Wait for valid register key
      }

      // Handle waiting for macro register to replay
      if (waitingForMacroReplayRegister) {
        if (key == 27) { // ESC - cancel
          waitingForMacroReplayRegister = false;
          e.screen->statusBarLeft = "";
          clearCommand();
          return;
        }
        if (isprint(key)) {
          // Check if macro exists for this register
          if (e.macroMap.find(key) != e.macroMap.end() &&
              !e.macroMap[key].empty()) {
            // Replay the macro
            int mult1 = command.multiplier1 == 0 ? 1 : command.multiplier1;
            const std::vector<int> &macro = e.macroMap[key];

            // Replay macro multiple times
            for (int i = 0; i < mult1; i++) {
              // Create a new NormalMode instance for each macro replay
              // iteration
              NormalMode tempMode;

              // Replay each key in the macro
              for (int macroKey : macro) {
                tempMode.onKeyStroke(e, macroKey);
              }
            }
            e.screen->statusBarLeft = "";
          } else {
            e.screen->statusBarLeft = "No macro found for register '" +
                                      std::string(1, (char)key) + "'";
          }

          waitingForMacroReplayRegister = false;
          clearCommand();
          executionEpilogue(e);
          return;
        }
        // For non-printable keys, just ignore them
        return; // Wait for valid register key
      }

      // Start recording macro
      if (key == (int)'q' && !waitingForMacroRegister) {
        waitingForMacroRegister = true;
        e.screen->statusBarLeft = "recording @";
        return;
      }

      // Start replaying macro
      if (key == (int)'@' && !waitingForMacroReplayRegister) {
        waitingForMacroReplayRegister = true;
        e.screen->statusBarLeft = "replay @";
        return;
      }

      if (key == 27) { // ESC
        clearCommand();
        return;
      }
      if (key == KEY_UP) {
        if (e.cursorRow > 0) {
          e.cursorRow--;
          // Clamp cursorCol to new line length
          size_t lineSize = e.buffer->getLine(e.cursorRow).size();
          if (lineSize == 0) {
            e.cursorCol = 0;
          } else {
            e.cursorCol = std::min(e.cursorCol, lineSize - 1);
          }
        }
        return;
      }
      if (key == KEY_DOWN) {
        if (e.cursorRow < e.buffer->lineCount() - 1) {
          e.cursorRow++;
          // Clamp cursorCol to new line length
          size_t lineSize = e.buffer->getLine(e.cursorRow).size();
          if (lineSize == 0) {
            e.cursorCol = 0;
          } else {
            e.cursorCol = std::min(e.cursorCol, lineSize - 1);
          }
        }
        return;
      }
      if (key == KEY_LEFT) {
        if (e.cursorCol > 0) {
          e.cursorCol--;
        }
        return;
      }
      if (key == KEY_RIGHT) {
        if (e.cursorCol < e.buffer->getLine(e.cursorRow).length() - 1) {
          e.cursorCol++;
        }
        return;
      }
      // Ctrl+B - scroll up full page
      if (key == 2) { // Ctrl+B
        size_t newBufferRow =
            e.screen->scrollUpFullPage(e.cursorRow, *e.buffer);
        e.cursorRow = newBufferRow;
        // Move to first non-whitespace character
        Coor newCoor = e.buffer->firstNoneWhitespaceChar(Coor{e.cursorRow, 0});
        e.updateCoor(newCoor);
        return;
      }

      // Ctrl+F - scroll down full page
      if (key == 6) { // Ctrl+F
        size_t newBufferRow =
            e.screen->scrollDownFullPage(e.cursorRow, *e.buffer);
        e.cursorRow = newBufferRow;
        // Move to first non-whitespace character
        Coor newCoor = e.buffer->firstNoneWhitespaceChar(Coor{e.cursorRow, 0});
        e.updateCoor(newCoor);
        return;
      }

      // Ctrl+U - scroll up half page
      if (key == 21) { // Ctrl+U
        size_t newBufferRow =
            e.screen->scrollUpHalfPage(e.cursorRow, *e.buffer);
        e.cursorRow = newBufferRow;
        // Move to first non-whitespace character
        Coor newCoor = e.buffer->firstNoneWhitespaceChar(Coor{e.cursorRow, 0});
        e.updateCoor(newCoor);
        return;
      }

      // Ctrl+D - scroll down half page
      if (key == 4) { // Ctrl+D
        size_t newBufferRow =
            e.screen->scrollDownHalfPage(e.cursorRow, *e.buffer);
        e.cursorRow = newBufferRow;
        // Move to first non-whitespace character
        Coor newCoor = e.buffer->firstNoneWhitespaceChar(Coor{e.cursorRow, 0});
        e.updateCoor(newCoor);
        return;
      }

      // Ctrl+I - auto-indent entire file (only for C++ files)
      if (key == 9) { // Ctrl+I
        // Check if file is C++ (.cc or .h)
        bool isCppFile = false;
        if (!e.filename.empty()) {
          size_t len = e.filename.length();
          if ((len >= 2 && e.filename.substr(len - 2) == ".h") ||
              (len >= 3 && e.filename.substr(len - 3) == ".cc")) {
            isCppFile = true;
          }
        }

        if (isCppFile) {
          e.pushUndoState();
          lastCommand = command;
          // Format entire file - apply auto-indent to all lines
          size_t lineCount = e.buffer->lineCount();
          size_t originalRow = e.cursorRow;

          for (size_t i = 0; i < lineCount; ++i) {
            Coor coor = e.buffer->autoIndent(Coor{i, 0});
            // Update cursor position if we're on this line
            if (i == originalRow) {
              e.cursorRow = coor.row;
              e.cursorCol = coor.col;
            }
          }
        }
        return;
      }

      // Ctrl+G - show file information
      if (key == 7) { // Ctrl+G
        std::string fileInfo;

        // Filename (or "[No Name]" if no filename)
        if (e.filename.empty()) {
          fileInfo = "\"[No Name]\"";
        } else {
          fileInfo = "\"" + e.filename + "\"";
        }

        // Modified status
        if (e.hasUnsavedChanges()) {
          fileInfo += " [modified]";
        }

        // Number of lines
        size_t lineCount = e.buffer->lineCount();
        fileInfo += " " + std::to_string(lineCount) + " line";
        if (lineCount != 1) {
          fileInfo += "s";
        }

        // Percentage viewed (based on cursor position)
        if (lineCount > 0) {
          // Calculate percentage: (current line + 1) / total lines * 100
          // Use 1-indexed line number for percentage
          size_t currentLine1Indexed = e.cursorRow + 1;
          int percentage = (currentLine1Indexed * 100) / lineCount;
          // Clamp to 0-100
          if (percentage < 0)
            percentage = 0;
          if (percentage > 100)
            percentage = 100;
          fileInfo += " --" + std::to_string(percentage) + "%--";
        } else {
          fileInfo += " --0%--";
        }

        e.screen->statusBarLeft = fileInfo;
        // Clear status bar after a short delay or on next keypress
        // For now, just show it - it will be cleared on next command
        return;
      }

      if (key == 105) { // i
        e.pushUndoState();
        lastCommand = command;
        e.enterInsertMode();
        return;
      }
      if (key == (int)'I') {
        e.pushUndoState();
        lastCommand = command;
        Coor newCoor = e.buffer->firstNoneWhitespaceChar(e.getCoor());
        e.updateCoor(newCoor);
        e.enterInsertMode();
        return;
      }
      if (key == 97) { // a
        e.pushUndoState();
        lastCommand = command;
        e.cursorCol++;
        e.enterInsertMode();
        return;
      }
      if (key == (int)'A') {
        e.pushUndoState();
        lastCommand = command;
        e.cursorCol = e.buffer->getLine(e.cursorRow).length();
        e.enterInsertMode();
        return;
      }
      if (key == (int)'s') { // s
        e.pushUndoState();
        lastCommand = command;
        Coor newCoor = e.buffer->del(
            e.getCoor(),
            Coor{e.cursorRow,
                 e.cursorCol +
                     (command.multiplier1 == 0 ? 1 : command.multiplier1)});
        e.updateCoor(newCoor);
        e.enterInsertMode();
        return;
      }
      if (key == (int)'S') {
        e.pushUndoState();
        lastCommand = command;
        size_t oldRow = e.cursorRow;
        size_t maxRowToDelete = std::min(
            e.cursorRow + (command.multiplier1 == 0 ? 1 : command.multiplier1) -
                1,
            e.buffer->lineCount() - 1);
        for (size_t i = oldRow; i <= maxRowToDelete; ++i) {
          delLine(e);
        }
        e.breakLine();
        e.cursorRow--;
        e.cursorCol = 0;
        e.enterInsertMode();
        return;
      }
      if (key == (int)'o') { // o
        e.pushUndoState();
        lastCommand = command;
        e.cursorCol = e.buffer->getLine(e.cursorRow).length();
        int n = command.multiplier1 == 0 ? 1 : command.multiplier1;
        for (int i = 0; i < n; i++) {
          e.breakLine();
        }

        e.enterInsertMode();
        return;
      }
      if (key == (int)'O') { // O
        e.pushUndoState();
        lastCommand = command;
        e.cursorCol = 0;
        int n = command.multiplier1 == 0 ? 1 : command.multiplier1;
        for (int i = 0; i < n; i++) {
          e.breakLine();
        }
        e.cursorRow -= n;
        e.enterInsertMode();
        return;
      }
      if (key == 58) { //:
        e.enterCommandMode();
        return;
      }
      if (key == 47) { // /
        e.isForward = true;
        e.enterSearchMode(true);
        return;
      }
      if (key == 63) { // ?
        e.isForward = false;
        e.enterSearchMode(false);
        return;
      }

      if (key == (int)'f') {
        waitForSearchChar = true;
        lineSearchForward = true;
        if (command.op.has_value()) {
          command.motion = 'f';
        } else {
          command.op = 'f';
        }
        return;
      }
      if (key == (int)'F') {
        waitForSearchChar = true;
        lineSearchForward = false;
        if (command.op.has_value()) {
          command.motion = 'F';
        } else {
          command.op = 'F';
        }
        return;
      }

      if (key == (int)'r') {
        waitForReplaceChar = true;
        if (command.op.has_value()) {
          clearCommand();
        } else {
          command.op = 'r';
        }
        return;
      }
      if (key == (int)'R') {
        e.pushUndoState();
        lastCommand = command;
        e.enterReplaceMode();
        return;
      }

      // ############## parsing operations ##############
      if (std::isdigit(key) && (char)key == '0' && command.multiplier1 == 0 &&
          !command.op.has_value()) {
        // edge case: if 0 is used as a standalone command
        command.op = '0';
        executeCommand(e);
        return;
      }
      if (std::isdigit(key) && (char)key == '0' && command.op.has_value() &&
          command.multiplier2 == 0) {
        // edge case: if 0 is used as a motion
        command.motion = '0';
        executeCommand(e);
        return;
      }

      if (std::isdigit(key)) {
        int value = key - '0';

        if (command.op.has_value()) {
          if (command.multiplier2 == 0) {
            command.multiplier2 = value;
          } else {
            command.multiplier2 = command.multiplier2 * 10 + value;
          }
        } else {
          if (command.multiplier1 == 0) {
            command.multiplier1 = value;
          } else {
            command.multiplier1 = command.multiplier1 * 10 + value;
          }
        }
        return;
      }
      if (standaloneCommands.contains((char)key)) {
        if (command.op.has_value()) {
          command.motion = (char)key;
          executeCommand(e);
        } else {
          command.op = (char)key;
          executeCommand(e);
        }
        return;
      }

      if (validOperators.contains((char)key)) {
        if (command.op.has_value()) {
          // dd, cc, yy, >>, <<
          command.motion = (char)key;
          executeCommand(e);
        } else {
          command.op = (char)key;
        }
        return;
      }

      if (validMotions.contains((char)key)) {
        if (!command.op.has_value() &&
            invalidStandaloneMotions.contains((char)key)) {
          command.print();
          throw std::runtime_error("Invalid standalone motion: " +
                                   std::string(1, (char)key));
        }
        if (command.op.has_value()) {
          command.motion = (char)key;
          executeCommand(e);
        } else {
          // a motion cannot exist without an operator
          command.print();
          throw std::runtime_error("Unexpected point reached");
        }
        return;
      }
    }

    void updateStatusBarCommand(Editor &e) {
      std::string commandStr;
      if (command.multiplier1 > 0) {
        commandStr += std::to_string(command.multiplier1);
      }
      if (command.op.has_value()) {
        commandStr += command.op.value();
      }
      if (command.multiplier2 > 0) {
        commandStr += std::to_string(command.multiplier2);
      }
      if (command.motion.has_value()) {
        commandStr += command.motion.value();
      }

      e.screen->statusBarRight = commandStr + "      " + e.getCoor1Indexed();
    }
    void executeCommand(Editor &e) {
      // TODO: implement this in full
      // remember that when executing the command, if multiplier is 0,
      // still needs to execute the command once
      int mult1 = command.multiplier1 == 0 ? 1 : command.multiplier1;
      int mult2 = command.multiplier2 == 0 ? 1 : command.multiplier2;
      if (command.motion.has_value()) {

        // ############## SPECIAL CASES ##############
        if (command.motion.value() == 'd') {
          if (command.op.value() == 'd') {
            e.pushUndoState();
            lastCommand = command;
            size_t oldRow = e.cursorRow;
            size_t maxRowToDelete = std::min(e.cursorRow + mult1 * mult2 - 1,
                                             e.buffer->lineCount() - 1);
            for (size_t i = oldRow; i <= maxRowToDelete; ++i) {
              delLine(e);
            }
          }
          executionEpilogue(e);
          return;
        } else if (command.motion.value() == 'c') {
          if (command.op.value() == 'c') {
            // cc, when it's a change of lines, add an empty line
            e.pushUndoState();
            lastCommand = command;
            size_t oldRow = e.cursorRow;
            size_t maxRowToDelete = std::min(e.cursorRow + mult1 * mult2 - 1,
                                             e.buffer->lineCount() - 1);
            bool addEmptyLineAfter =
                maxRowToDelete == e.buffer->lineCount() - 1;
            for (size_t i = oldRow; i <= maxRowToDelete; ++i) {
              delLine(e);
            }
            if (addEmptyLineAfter) {
              e.buffer->breakLine(e.cursorRow,
                                  e.buffer->getLine(e.cursorRow).length());
              e.cursorRow++;
              e.cursorCol = 0;
            } else {
              e.buffer->breakLine(e.cursorRow, 0);
              e.cursorCol = 0;
            }
            executionEpilogue(e);
            e.enterInsertMode();
            return;
          }
        } else if (command.motion.value() == 'y') {
          if (command.op.value() == 'y') {
            e.clipboard.clear();
            e.clipboard = e.buffer->getLines(e.cursorRow,
                                             e.cursorRow + mult1 * mult2 - 1);
          }
          executionEpilogue(e);
          return;
        } else if (command.op.has_value() && command.op.value() == 'y' &&
                   command.motion.value() == 'j') {
          e.clipboard.clear();
          e.clipboard =
              e.buffer->getLines(e.cursorRow, e.cursorRow + mult1 * mult2);
          executionEpilogue(e);
          return;
        } else if (command.op.has_value() && command.op.value() == 'y' &&
                   command.motion.value() == 'k') {
          e.clipboard.clear();
          e.clipboard =
              e.buffer->getLines(e.cursorRow - mult1 * mult2, e.cursorRow);
          executionEpilogue(e);
          return;
        }

        else if (command.op.has_value() &&
                 (command.op.value() == 'd' || command.op.value() == 'c') &&
                 command.motion.value() == 'j') {
          e.pushUndoState();
          lastCommand = command;
          size_t oldRow = e.cursorRow;
          size_t maxRowToDelete =
              std::min(e.cursorRow + mult1 * mult2, e.buffer->lineCount() - 1);
          bool addEmptyLineAfter = maxRowToDelete == e.buffer->lineCount() - 1;
          for (size_t i = oldRow; i <= maxRowToDelete; ++i) {
            delLine(e);
          }
          char op = command.op.value();
          // this clears the command, needs to save op first
          executionEpilogue(e);
          if (op == 'c') {
            if (addEmptyLineAfter) {
              e.buffer->breakLine(e.cursorRow,
                                  e.buffer->getLine(e.cursorRow).length());
              e.cursorRow++;
              e.cursorCol = 0;
            } else {
              e.buffer->breakLine(e.cursorRow, 0);
              e.cursorCol = 0;
            }
            e.enterInsertMode();
          }
          return;
        } else if (command.op.has_value() &&
                   (command.op.value() == 'd' || command.op.value() == 'c') &&
                   command.motion.value() == 'k') {

          e.pushUndoState();
          lastCommand = command;
          size_t minRowToDelete =
              (e.cursorRow < static_cast<size_t>(mult1 * mult2))
                  ? 0
                  : e.cursorRow - static_cast<size_t>(mult1 * mult2);
          bool addEmptyLineBefore = minRowToDelete == 0;
          for (; e.cursorRow >= minRowToDelete &&
                 e.cursorRow < e.buffer->lineCount();
               e.cursorRow--) {
            delLine(e);
            if (e.cursorRow == minRowToDelete)
              break; // prevent underflow
          }
          char op = command.op.value();
          // this clears the command, needs to save op first
          executionEpilogue(e);
          if (op == 'c') {
            if (addEmptyLineBefore) {
              e.buffer->breakLine(0, 0);
              e.cursorRow = 0;
              e.cursorCol = 0;
            } else {
              e.buffer->breakLine(e.cursorRow, 0);
              e.cursorCol = 0;
            }
            e.enterInsertMode();
          }
          return;
        } else if (command.op.has_value() && command.op.value() == '>' &&
                   command.motion.value() == '>') {
          e.pushUndoState();
          lastCommand = command;
          for (int i = 0; i < mult1 * mult2; i++) {
            Coor newCoor = e.buffer->indent(e.getCoor());
            if (i < mult1 * mult2 - 1) {
              newCoor = Coor{newCoor.row + 1, newCoor.col};
            }
            e.updateCoor(newCoor);
          }
          executionEpilogue(e);
          return;
        } else if (command.op.has_value() && command.op.value() == '<' &&
                   command.motion.value() == '<') {
          e.pushUndoState();
          lastCommand = command;
          for (int i = 0; i < mult1 * mult2; i++) {
            Coor newCoor = e.buffer->dedent(e.getCoor());
            if (i < mult1 * mult2 - 1) {
              newCoor = Coor{newCoor.row + 1, newCoor.col};
            }
            e.updateCoor(newCoor);
          }
          executionEpilogue(e);
          return;
        }
        for (int i = 0; i < mult1; i++) {
          Coor startCoor = Coor{e.cursorRow, e.cursorCol};
          Coor endCoor = Coor{e.cursorRow, e.cursorCol};
          int yankCount = 0;
        yankMore:
          for (int j = 0; j < mult2; j++) {
            // ############## motions ##############
            if (command.motion.value() == 'w') {
              endCoor = e.buffer->nextWord(endCoor);
            } else if (command.motion.value() == 'b') {
              endCoor = e.buffer->prevWord(endCoor);
            } else if (command.motion.value() == 'n' ||
                       command.motion.value() == 'N') {
              if (e.hasSearched) {
                Interval result = e.buffer->search(
                    e.searchString, endCoor,
                    command.motion.value() == 'n' ? e.isForward : !e.isForward);
                if (result.valid()) {
                  endCoor = result.start;
                } else {
                  // if no such string
                }
              } else {
                // if never searched
              }
            } else if (command.motion.value() == 'f') {
              // Store for '.' command replay
              lastSearchChar = searchChar;
              lastLineSearchForward = true;
              Coor oldCoor = endCoor;
              endCoor = e.buffer->nextSameLineMatchingChar(endCoor, searchChar);
              if (oldCoor != endCoor)
                endCoor = Coor{endCoor.row, endCoor.col + 1};
            } else if (command.motion.value() == 'F') {
              // Store for '.' command replay
              lastSearchChar = searchChar;
              lastLineSearchForward = false;
              endCoor = e.buffer->lastSameLineMatchingChar(endCoor, searchChar);
            } else if (command.motion.value() == ';') {
              if (hasLineSearched) {
                if (lineSearchForward) {
                  endCoor =
                      e.buffer->nextSameLineMatchingChar(endCoor, searchChar);
                } else {
                  endCoor =
                      e.buffer->lastSameLineMatchingChar(endCoor, searchChar);
                }
              } else {
                executionEpilogue(e);
                return;
              }
            } else if (command.motion.value() == 'h') {
              endCoor =
                  Coor{endCoor.row, (endCoor.col > 0) ? endCoor.col - 1 : 0};
            } else if (command.motion.value() == 'l') {
              endCoor = Coor{endCoor.row, endCoor.col + 1};
            } else if (command.motion.value() == 'j') {
              if (endCoor.row < e.buffer->lineCount() - 1) {
                size_t nextRow = endCoor.row + 1;
                size_t nextLineSize = e.buffer->getLine(nextRow).size();
                size_t newCol = (nextLineSize == 0)
                                    ? 0
                                    : std::min(nextLineSize - 1, endCoor.col);
                endCoor = Coor{nextRow, newCol};
              }
            } else if (command.motion.value() == 'k') {
              if (endCoor.row > 0) {
                size_t prevRow = endCoor.row - 1;
                size_t prevLineSize = e.buffer->getLine(prevRow).size();
                size_t newCol = (prevLineSize == 0)
                                    ? 0
                                    : std::min(prevLineSize - 1, endCoor.col);
                endCoor = Coor{prevRow, newCol};
              }
            } else if (command.motion.value() == '^') {
              endCoor = e.buffer->firstNoneWhitespaceChar(endCoor);
            } else if (command.motion.value() == '$') {
              endCoor =
                  Coor{endCoor.row, e.buffer->getLine(endCoor.row).size() - 1};
            } else if (command.motion.value() == '0') {
              endCoor = Coor{endCoor.row, 0};
            } else if (command.motion.value() == '%') {
              endCoor = e.buffer->findMatchingBracket(endCoor);
            }
          }
          // ############## operators ##############
          if (command.op.value() == 'd') {
            e.pushUndoState();
            lastCommand = command;
            Coor newCoor = e.buffer->del(startCoor, endCoor);
            e.updateCoor(newCoor);
          } else if (command.op.value() == 'c') {
            e.pushUndoState();
            lastCommand = command;
            Coor newCoor = e.buffer->del(startCoor, endCoor);
            e.updateCoor(newCoor);
            executionEpilogue(e);
            e.enterInsertMode();
            return;
          } else if (command.op.value() == 'y') {
            yankCount++;
            if (yankCount < mult1) {
              goto yankMore;
            }
            e.clipboard.clear();
            e.clipboard = e.buffer->getLines(startCoor, endCoor);
          }
        }
      } else {
        // ############## standalone commands ##############
        if (!command.op.has_value()) {
          command.print();
          throw std::runtime_error(
              "Invalid command, no operator and no motion");
        } else {
          for (int i = 0; i < mult1; i++) {
            switch (command.op.value()) {
            case 'h': {
              if (e.cursorCol > 0) {
                e.cursorCol--;
              }
              break;
            }
            case 'l': {
              if (e.cursorCol < e.buffer->getLine(e.cursorRow).size() - 1) {
                e.cursorCol++;
              }
              break;
            }
            case 'r': {
              if (i == 0)
                e.pushUndoState();
              lastCommand = command;
              lastReplaceChar = replaceChar; // Store for '.' command replay
              Coor newCoor = e.buffer->replace(e.getCoor(), replaceChar);
              e.updateCoor(newCoor);
              break;
            }
            case 'p': {
              if (i == 0)
                e.pushUndoState();
              lastCommand = command;
              Coor newCoor = e.buffer->insertLines(
                  Coor{e.cursorRow, e.cursorCol + 1}, e.clipboard);
              e.updateCoor(newCoor);
              break;
            }
            case 'P': {
              if (i == 0)
                e.pushUndoState();
              lastCommand = command;
              Coor newCoor = e.buffer->insertLines(e.getCoor(), e.clipboard);
              e.updateCoor(newCoor);
              break;
            }
            case 'j': {
              if (e.cursorRow < e.buffer->lineCount() - 1) {
                e.cursorRow++;
                // Clamp cursorCol to new line length
                size_t lineSize = e.buffer->getLine(e.cursorRow).size();
                if (lineSize == 0) {
                  e.cursorCol = 0;
                } else {
                  e.cursorCol = std::min(e.cursorCol, lineSize - 1);
                }
              }
              break;
            }
            case 'k': {
              if (e.cursorRow > 0) {
                e.cursorRow--;
                // Clamp cursorCol to new line length
                size_t lineSize = e.buffer->getLine(e.cursorRow).size();
                if (lineSize == 0) {
                  e.cursorCol = 0;
                } else {
                  e.cursorCol = std::min(e.cursorCol, lineSize - 1);
                }
              }
              break;
            }
            case 'w': {
              Coor newCoor = e.buffer->nextWord(e.getCoor());
              e.updateCoor(newCoor);
              break;
            }
            case 'b': {
              Coor newCoor = e.buffer->prevWord(e.getCoor());
              e.updateCoor(newCoor);
              break;
            }
            case 'e': {
              break;
            }
            case '$': {
              Coor newCoor =
                  Coor{e.cursorRow, e.buffer->getLine(e.cursorRow).size() - 1};
              e.updateCoor(newCoor);
              break;
            }
            case '0': {
              e.cursorCol = 0;
              break;
            }
            case 'u': {
              e.popUndoState();
              break;
            }
            case (char)18: { // Ctrl+R
              e.popRedoState();
              break;
            }
            case '.': {
              if (lastCommand.has_value()) {
                Command command_copy = lastCommand.value();

                // Execute the last command multiple times, each with a fresh
                // NormalMode instance
                for (int i = 0; i < mult1; i++) {
                  // Create a new NormalMode instance with clean state
                  NormalMode tempMode;

                  // Copy the last command to the new instance
                  tempMode.command = command_copy;

                  // Copy the necessary state for commands that need it
                  tempMode.replaceChar = lastReplaceChar;
                  tempMode.searchChar = lastSearchChar;
                  tempMode.lineSearchForward = lastLineSearchForward;
                  tempMode.waitForReplaceChar =
                      false; // Command is ready to execute
                  tempMode.waitForSearchChar =
                      false; // Command is ready to execute

                  // Execute the command using the temporary instance
                  tempMode.executeCommand(e);
                }
              }
              executionEpilogue(e);
              return;
              break;
            }
            case 'x': {
              if (i == 0)
                e.pushUndoState();
              lastCommand = command;
              Coor newCoor = e.buffer->del(
                  e.getCoor(),
                  Coor{e.cursorRow, e.cursorCol + (command.multiplier1 == 0
                                                       ? 1
                                                       : command.multiplier1)});
              e.updateCoor(newCoor);
              break;
            }
            case 'X': {
              if (i == 0)
                e.pushUndoState();
              lastCommand = command;
              if (e.cursorCol > 0) {
                Coor newCoor = e.buffer->del(Coor{e.cursorRow, e.cursorCol - 1},
                                             e.getCoor());
                e.updateCoor(newCoor);
              }
              break;
            }
            case '^': {
              Coor newCoor = e.buffer->firstNoneWhitespaceChar(e.getCoor());
              e.updateCoor(newCoor);
              break;
            }
            case 'J': {
              if (e.cursorRow < e.buffer->lineCount() - 1) {
                if (i == 0)
                  e.pushUndoState();
                lastCommand = command;
                // Coor newCoor =
                e.buffer->appendNextLineOntoCurrentLine(e.getCoor());
                // e.updateCoor(newCoor);
              }
              break;
            }
            case 'f': {
              // Store for '.' command replay
              lastSearchChar = searchChar;
              lastLineSearchForward = true;
              Coor newCoor =
                  e.buffer->nextSameLineMatchingChar(e.getCoor(), searchChar);
              e.updateCoor(newCoor);
              break;
            }
            case 'F': {
              // Store for '.' command replay
              lastSearchChar = searchChar;
              lastLineSearchForward = false;
              Coor newCoor =
                  e.buffer->lastSameLineMatchingChar(e.getCoor(), searchChar);
              e.updateCoor(newCoor);
              break;
            }
            case '%': {
              Coor newCoor = e.buffer->findMatchingBracket(e.getCoor());
              e.updateCoor(newCoor);
              break;
            }
            case ';': {
              if (hasLineSearched) {
                Coor endCoor = e.getCoor();
                if (lineSearchForward) {
                  endCoor =
                      e.buffer->nextSameLineMatchingChar(endCoor, searchChar);
                } else {
                  endCoor =
                      e.buffer->lastSameLineMatchingChar(endCoor, searchChar);
                }
                e.updateCoor(endCoor);
              }
              break;
            }
            case 'n': {
              if (e.hasSearched) {
                Interval result =
                    e.buffer->search(e.searchString, e.getCoor(), e.isForward);
                if (result.valid()) {
                  e.updateCoor(result.start);
                }
              }

              else {
                // if never searched
                clearCommand();
                return;
              }
              break;
            }
            case 'N': {
              if (e.hasSearched) {
                Interval result =
                    e.buffer->search(e.searchString, e.getCoor(), !e.isForward);
                // isForward is the only difference
                if (result.valid()) {
                  e.updateCoor(result.start);
                }
              } else {
                // if never searched
                clearCommand();
                return;
              }
              break;
            }
            }
          }
        }
      }
      executionEpilogue(e);
    }
    void executionEpilogue(Editor &e) {
      normalizeCoor(e);
      clearCommand();
    }
    void clearCommand() {
      command.multiplier1 = 0;
      command.op = std::nullopt;
      command.multiplier2 = 0;
      command.motion = std::nullopt;
    }
    void normalizeCoor(Editor &e) {
      // Clamp cursorRow: 0 <= row < buffer->lineCount()
      if (e.buffer->lineCount() == 0) {
        e.cursorRow = 0;
        e.cursorCol = 0;
      } else {
        if (e.cursorRow >= e.buffer->lineCount())
          e.cursorRow = e.buffer->lineCount() - 1;
        if (e.cursorRow < 0)
          e.cursorRow = 0;
        size_t lineLength = e.buffer->getLine(e.cursorRow).size();
        if (lineLength == 0) {
          e.cursorCol = 0;
        } else {
          if (e.cursorCol >= lineLength)
            e.cursorCol = lineLength - 1;
          if (e.cursorCol < 0)
            e.cursorCol = 0;
        }
      }
    }
    void delLine(Editor &e) {
      Coor startCoor = Coor{e.cursorRow, e.cursorCol};
      Coor endCoor = Coor{e.cursorRow, e.cursorCol};

      startCoor = e.buffer->lineStart(startCoor);
      endCoor = e.buffer->lineEnd(endCoor);
      e.buffer->del(startCoor, endCoor);
      Coor newCoor = e.buffer->delLine(startCoor);
      e.updateCoor(newCoor);
    }
  };

  class CommandMode : public IMode {
  public:
    bool shouldDrawBufferCursor() const override { return false; }
    void handleKey(Editor &e, int key) override {
      if (key == 10) { // ENTER
        e.handleCommand();
        return;
      } else if (key == 27) { // ESC
        e.screen->statusBarLeft = "";
        e.enterNormalMode();
        return;
      }

      // real vim handles moving the cursor to insert char in the middle of
      // the command not supported in vm, if the user changes their mind, they
      // need to del the command with backspace
      if (isprint(key)) {
        e.insertCommandChar((char)key);
        return;
      }
    }
    void onKeyStroke(Editor &e, int key) override { handleKey(e, key); }
  };
  class SearchMode : public IMode {
  public:
    bool isForward_local;
    std::string searchString_local;
    // isForward is decided from whether the last search was / or ?
    SearchMode(bool isForward_local)
        : isForward_local(isForward_local), searchString_local("") {}
    bool shouldHighLightSearchResult() const override { return true; }
    bool shouldDrawBufferCursor() const override { return false; }
    void handleKey(Editor &e, int key) override {
      if (key == 27) { // ESC
        e.screen->statusBarLeft = "";
        e.enterNormalMode();

      } else if (key == 127 || key == KEY_BACKSPACE) {
        if (!searchString_local.empty()) {
          searchString_local.pop_back();
        } else {
          e.screen->statusBarLeft = "";
          e.enterNormalMode();
        }
      } else if (isprint(key)) {
        searchString_local.push_back((char)key);
        e.hasSearched = true;
      } else if (key == 10) { // ENTER
        e.isForward = isForward_local;
        e.hasSearched = true;
        e.searchString = searchString_local;
        if (e.searchResult.valid()) {
          e.updateCoor(e.searchResult.start);
        }
        e.enterNormalMode();
      }
    }

    void onKeyStroke(Editor &e, int key) override {
      IMode *oldMode = e.mode.get();
      handleKey(e, key);

      if (e.mode.get() == oldMode) {
        e.screen->statusBarLeft =
            (isForward_local ? "/" : "?") + searchString_local;
        e.screen->statusBarRight = "";

        e.searchResult =
            e.buffer->search(searchString_local, e.getCoor(), isForward_local);
        // the searched result will be [first, second]
        if (e.searchResult.valid()) {
          // highlight the searched buffer range [first, second], all on one
          // row
        } else {
          // if not found
        }
      }
    }
  };
  class ReplaceMode : public IMode {
  public:
    void handleKey(Editor &e, int key) override {

      if (key == KEY_UP) {
        if (e.cursorRow > 0) {
          e.cursorRow--;
          // Clamp cursorCol to new line length
          size_t lineSize = e.buffer->getLine(e.cursorRow).size();
          if (lineSize == 0) {
            e.cursorCol = 0;
          } else {
            e.cursorCol = std::min(e.cursorCol, lineSize - 1);
          }
        }
        return;
      }
      if (key == KEY_DOWN) {
        if (e.cursorRow < e.buffer->lineCount() - 1) {
          e.cursorRow++;
          // Clamp cursorCol to new line length
          size_t lineSize = e.buffer->getLine(e.cursorRow).size();
          if (lineSize == 0) {
            e.cursorCol = 0;
          } else {
            e.cursorCol = std::min(e.cursorCol, lineSize - 1);
          }
        }
        return;
      }
      if (key == KEY_LEFT) {
        if (e.cursorCol > 0) {
          e.cursorCol--;
        }
        return;
      }
      if (key == KEY_RIGHT) {
        if (e.cursorCol < e.buffer->getLine(e.cursorRow).length() - 1) {
          e.cursorCol++;
        }
        return;
      }

      if (key == 10) { // ENTER
        e.breakLine();
        return;
      } else if (key == 27) { // ESC
        e.screen->statusBarLeft = "";
        e.enterNormalMode();
        return;
      }
      if (isprint(key)) {
        Coor newCoor = e.buffer->replaceMode(e.getCoor(), (char)key);
        e.updateCoor(newCoor);
        return;
      }
    }
    void onKeyStroke(Editor &e, int key) override { handleKey(e, key); }
  };

  std::unique_ptr<Buffer> buffer;
  std::unique_ptr<Screen> screen;
  std::unique_ptr<IMode> mode;
  size_t cursorRow, cursorCol;
  size_t maxCursorCol; // TODO: implement this
  // to enable memory when moving the cursor up and down
  // i.e. when moving from the end of a long line to a short line,
  // the cursor should not be at the end of the line
  // when moved back to the previous line, the cursor should be at where it
  // was
  Interval searchResult; // if not found, start = MAX, end = MIN
  bool hasSearched;
  std::string searchString;
  bool isForward;
  // keep the search data of the last entered search

  std::vector<std::string> clipboard;

  std::stack<State> undoStack;
  std::stack<State> redoStack;

  bool running;
  std::string filename;
  std::unordered_map<int, std::vector<int>> macroMap;
  bool isRecordingMacro;
  int currentMacroRegister;
  bool isReadOnly = false; // Track if current file is read-only
  std::vector<std::string>
      originalBufferState; // Copy of buffer when file was opened/saved
  bool syntaxHighlightingEnabled = true; // Syntax highlighting on/off flag

  bool hasUnsavedChanges() const {
    // Compare current buffer with original state
    if (buffer->lineCount() != originalBufferState.size()) {
      return true;
    }
    for (size_t i = 0; i < buffer->lineCount(); ++i) {
      if (buffer->getLine(i) != originalBufferState[i]) {
        return true;
      }
    }
    return false;
  }

  void saveBufferState() {
    // Save current buffer state
    originalBufferState.clear();
    for (size_t i = 0; i < buffer->lineCount(); ++i) {
      originalBufferState.push_back(buffer->getLine(i));
    }
  }

public:
  Editor(const std::string &filename = "")
      : buffer{std::make_unique<Buffer>()}, screen{std::make_unique<Screen>()},
        mode{std::make_unique<NormalMode>()}, cursorRow{0},
        cursorCol{0}, // needs to be changed to normal mode
        searchResult{Interval::INVALID}, hasSearched{false}, searchString{""},
        isForward{true}, running{false}, filename{filename},
        isRecordingMacro{false}, currentMacroRegister{0} {
    if (!filename.empty()) {
      try {
        std::vector<std::string> lines = Buffer::readFile(filename);
        buffer->loadFromLines(lines);
        saveBufferState(); // Save the initial state

        // Check if file is read-only
        struct stat fileStat;
        if (stat(filename.c_str(), &fileStat) == 0) {
          // Check if file is writable (not read-only)
          // On Unix, check if we have write permission
          if (access(filename.c_str(), W_OK) != 0) {
            isReadOnly = true;
          }
        }
      } catch (const std::exception &e) {
        // File doesn't exist or can't be read - start with empty buffer
        // This is fine for new files
        saveBufferState(); // Save empty buffer state
      }
    } else {
      // No filename - save empty buffer state
      saveBufferState();
    }
  }
  void run() {
    running = true;
    screen->statusBarRight = getCoor1Indexed();
    screen->draw(*buffer, cursorRow, cursorCol, mode->shouldDrawBufferCursor(),
                 filename, syntaxHighlightingEnabled);

    while (running) {
      int key = screen->getKey(); // blocks until key is pressed
      mode->onKeyStroke(*this, key);
      screen->draw(*buffer, cursorRow, cursorCol,
                   mode->shouldDrawBufferCursor(), filename,
                   syntaxHighlightingEnabled);
      if (mode->shouldHighLightSearchResult() && searchResult.valid()) {
        screen->highlightInterval(*buffer, searchResult);
      }
      screen->refreshScreen();
    }
  }
  void onKeyStroke(int key) { mode->onKeyStroke(*this, key); }
  std::string getCoor1Indexed() {
    return std::to_string(cursorRow + 1) + "," + std::to_string(cursorCol + 1);
  }
  void enterInsertMode() {
    mode = std::make_unique<InsertMode>();
    screen->statusBarLeft = "-- INSERT --";
    screen->statusBarRight = getCoor1Indexed();
  }
  void enterNormalMode() {
    mode = std::make_unique<NormalMode>();
    screen->statusBarRight = getCoor1Indexed();
  }
  void enterCommandMode() {
    mode = std::make_unique<CommandMode>();
    screen->statusBarLeft = ":";
    screen->statusBarRight = "";
  }
  void enterSearchMode(bool isForward_local) {
    mode = std::make_unique<SearchMode>(isForward_local);
    searchResult = Interval::INVALID;
    screen->statusBarLeft = isForward_local ? "/" : "?";
    screen->statusBarRight = "";
  }
  void enterReplaceMode() {
    mode = std::make_unique<ReplaceMode>();
    screen->statusBarLeft = "-- REPLACE --";
    screen->statusBarRight = getCoor1Indexed();
  }
  void handleCommand() {
    std::string command = screen->statusBarLeft;

    // :q! - quit without saving
    if (command == ":q!") {
      running = false;
      return;
    }
    if (command == ":0") {
      // Like :line-number, but go to line 0 (first line) and non-whitespace
      // char
      if (buffer->lineCount() > 0) {
        cursorRow = 0;
        Coor newCoor = buffer->firstNoneWhitespaceChar(Coor{0, 0});
        updateCoor(newCoor);
      } else {
        cursorRow = 0;
        cursorCol = 0;
      }
      enterNormalMode();
      return;
    }
    if (command == ":$") {
      cursorRow = buffer->lineCount() > 0 ? buffer->lineCount() - 1 : 0;
      cursorCol = 0;
      enterNormalMode();
      return;
    }

    // :line-number - jump to specific line number (1-indexed)
    if (command.length() > 1 && command[0] == ':') {
      std::string lineNumStr = command.substr(1);
      // Check if the rest is all digits
      bool isAllDigits = true;
      for (char c : lineNumStr) {
        if (!std::isdigit(c)) {
          isAllDigits = false;
          break;
        }
      }
      if (isAllDigits && !lineNumStr.empty()) {
        size_t lineNum = std::stoul(lineNumStr);
        // vim uses 1-indexed line numbers, convert to 0-indexed
        if (lineNum > 0) {
          size_t targetRow = lineNum - 1;
          if (targetRow < buffer->lineCount()) {
            cursorRow = targetRow;
            // Move to first non-whitespace character (like vim)
            Coor newCoor = buffer->firstNoneWhitespaceChar(getCoor());
            updateCoor(newCoor);
            enterNormalMode();
            return;
          } else {
            cursorRow = targetRow;
            // Move to first non-whitespace character (like vim)
            Coor newCoor = buffer->firstNoneWhitespaceChar(
                Coor{buffer->lineCount() - 1, 0});
            updateCoor(newCoor);
            enterNormalMode();
            return;
          }
        }
      }
    }

    // :syntax on - enable syntax highlighting
    if (command == ":syntax on") {
      syntaxHighlightingEnabled = true;
      screen->statusBarLeft = "Syntax highlighting enabled";
      enterNormalMode();
      return;
    }

    // :syntax off - disable syntax highlighting
    if (command == ":syntax off") {
      syntaxHighlightingEnabled = false;
      screen->statusBarLeft = "Syntax highlighting disabled";
      enterNormalMode();
      return;
    }

    // :syntax toggle - toggle syntax highlighting
    if (command == ":syntax toggle") {
      syntaxHighlightingEnabled = !syntaxHighlightingEnabled;
      if (syntaxHighlightingEnabled) {
        screen->statusBarLeft = "Syntax highlighting enabled";
      } else {
        screen->statusBarLeft = "Syntax highlighting disabled";
      }
      enterNormalMode();
      return;
    }

    // :q - quit (only if no unsaved changes)
    if (command == ":q") {
      if (hasUnsavedChanges()) {
        screen->statusBarLeft =
            "No write since last change (add ! to override)";
        enterNormalMode();
        return;
      }
      running = false;
      return;
    }

    // :w - save file
    if (command == ":w") {
      if (filename.empty()) {
        screen->statusBarLeft = "No filename set. Use :w filename";
        enterNormalMode();
        return;
      }
      // Check if trying to save to read-only file
      if (isReadOnly) {
        screen->statusBarLeft =
            "\"" + filename +
            "\" is read-only. Use :w filename to save to a different file";
        enterNormalMode();
        return;
      }
      try {
        buffer->saveToFile(filename);
        saveBufferState(); // Update saved state after successful save
        screen->statusBarLeft = "\"" + filename + "\" written";
      } catch (const std::exception &e) {
        screen->statusBarLeft = "Error: " + std::string(e.what());
      }
      enterNormalMode();
      return;
    }

    // :w filename - save as new filename
    if (command.substr(0, 3) == ":w " && command.length() > 3) {
      std::string newFilename = command.substr(3);
      try {
        buffer->saveToFile(newFilename);
        filename = newFilename; // Update current filename
        // Check if new file is read-only
        struct stat fileStat;
        if (stat(newFilename.c_str(), &fileStat) == 0) {
          isReadOnly = (access(newFilename.c_str(), W_OK) != 0);
        } else {
          isReadOnly = false; // New file, not read-only
        }
        saveBufferState(); // Update saved state after successful save
        screen->statusBarLeft = "\"" + filename + "\" written";
      } catch (const std::exception &e) {
        screen->statusBarLeft = "Error: " + std::string(e.what());
      }
      enterNormalMode();
      return;
    }

    // :wq filename - save as new filename and quit
    if (command.substr(0, 4) == ":wq " && command.length() > 4) {
      std::string newFilename = command.substr(4);
      try {
        buffer->saveToFile(newFilename);
        filename = newFilename; // Update current filename
        // Check if new file is read-only
        struct stat fileStat;
        if (stat(newFilename.c_str(), &fileStat) == 0) {
          isReadOnly = (access(newFilename.c_str(), W_OK) != 0);
        } else {
          isReadOnly = false; // New file, not read-only
        }
        saveBufferState(); // Update saved state after successful save
        running = false;
        return;
      } catch (const std::exception &e) {
        screen->statusBarLeft = "Error: " + std::string(e.what());
        enterNormalMode();
        return;
      }
    }

    // :wq - save and quit
    if (command == ":wq") {
      if (filename.empty()) {
        screen->statusBarLeft = "No filename set. Use :w filename";
        enterNormalMode();
        return;
      }
      // Check if trying to save to read-only file
      if (isReadOnly) {
        screen->statusBarLeft =
            "\"" + filename +
            "\" is read-only. Use :wq filename to save to a different file";
        enterNormalMode();
        return;
      }
      try {
        buffer->saveToFile(filename);
        saveBufferState(); // Update saved state after successful save
        running = false;
        return;
      } catch (const std::exception &e) {
        screen->statusBarLeft = "Error: " + std::string(e.what());
        enterNormalMode();
        return;
      }
    }

    // :r filename - read file and insert below current line
    if (command.substr(0, 3) == ":r " && command.length() > 3) {
      std::string readFilename = command.substr(3);
      try {
        std::vector<std::string> lines = Buffer::readFile(readFilename);
        bool wasAtBottom = (cursorRow + 1 >= buffer->lineCount());

        // If there's a line below (not at bottom), break current line to create
        // space
        if (!wasAtBottom) {
          // Break at end of current line to create a new empty line
          buffer->breakLine(cursorRow, buffer->getLine(cursorRow).length());
        }

        // Insert at start of next line (or at end if at bottom)
        Coor insertPos = Coor{cursorRow + 1, 0};
        Coor newCoor = buffer->insertLines(insertPos, lines);

        updateCoor(newCoor);
        screen->statusBarLeft = "\"" + readFilename + "\" read";
      } catch (const std::exception &e) {
        screen->statusBarLeft = "Error: " + std::string(e.what());
      }
      enterNormalMode();
      return;
    }

    // Unknown command - show error and return to normal mode
    screen->statusBarLeft = "Unknown command: " + command;
    enterNormalMode();
  }
  void insertChar(char c) {
    std::string &line = buffer->getLine(cursorRow);
    line.insert(cursorCol, 1, c);
    cursorCol++;
  }
  void insertCommandChar(char c) {
    std::string &line = screen->statusBarLeft;
    line.insert(line.size(), 1, c);
  }
  void breakLine() {
    buffer->breakLine(cursorRow, cursorCol);
    cursorRow++;
    cursorCol = 0;
  }
  void backspace() {
    DeletionResult result = buffer->backspace(cursorRow, cursorCol);

    switch (result) {
    case DeletionResult::Nothing:
      // Do nothing, cursor stays where it is
      break;

    case DeletionResult::Character:
      // Cursor moves back one column
      cursorCol--;
      break;

    case DeletionResult::Newline:
      // Cursor moves to end of previous line
      cursorRow--;
      cursorCol = buffer->getLine(cursorRow).length();
      break;
    }
  }
  Coor getCoor() { return Coor{cursorRow, cursorCol}; }
  void updateCoor(Coor coor) {
    cursorRow = coor.row;
    cursorCol = coor.col;
  }
  void pushUndoState() {
    State state = State{*buffer, getCoor()};
    undoStack.push(state);
    redoStack = std::stack<State>();
  }
  void popUndoState() {
    // pop when undo
    if (undoStack.empty()) {
      return; // Nothing to undo
    }
    // Save current state to redoStack before restoring previous state
    State currentState = State{*buffer, getCoor()};
    redoStack.push(currentState);
    // Restore previous state
    State state = undoStack.top();
    undoStack.pop();
    *buffer = state.buffer;
    updateCoor(state.cursor);
  }
  void popRedoState() {
    // pop when redo
    if (redoStack.empty()) {
      return; // Nothing to redo
    }
    // Save current state to undoStack before restoring next state
    State currentState = State{*buffer, getCoor()};
    undoStack.push(currentState);
    // Restore next state
    State state = redoStack.top();
    redoStack.pop();
    *buffer = state.buffer;
    updateCoor(state.cursor);
  }
};
const std::set<char> Editor::NormalMode::validOperators = {'d', 'c', 'y', '>',
                                                           '<'};
const std::set<char> Editor::NormalMode::validMotions = {
    'w', 'b', 'e', '$', '0', 'h', 'l', 'j',
    'k', '^', 'f', 't', 'n', 'N', ';', '%'};
const std::set<char> Editor::NormalMode::standaloneCommands = {
    'h', 'l', 'j', 'k', '^', 'w', 'b', 'e', '$',      '0',
    'u', '.', ';', 'n', 'N', 'a', 'f', 'p', 'P',      'o',
    'O', 'r', 's', 'x', 'F', 'J', 'X', '%', (char)18, '@'};
const std::set<char> Editor::NormalMode::invalidStandaloneMotions = {};