export module Buffer;
import <vector>;
import <string>;
import <sstream>;
import <limits>;

export enum class DeletionResult {
  Nothing,   // Nothing to del (at start of file)
  Character, // deld a character
  Newline    // deld a newline (merged with previous line)
};

export struct Coor {
  size_t row;
  size_t col;
  static const Coor MAX;
  static const Coor MIN;
  static const Coor INVALID;
  bool operator==(const Coor &other) const {
    return row == other.row && col == other.col;
  }
  bool operator<(const Coor &other) const {
    return row < other.row || (row == other.row && col < other.col);
  }
  bool operator>(const Coor &other) const {
    return row > other.row || (row == other.row && col > other.col);
  }
  bool operator<=(const Coor &other) const {
    return *this < other || *this == other;
  }
  bool operator>=(const Coor &other) const {
    return *this > other || *this == other;
  }
  bool operator!=(const Coor &other) const { return !(*this == other); }
};
constexpr Coor Coor::MIN = {0, 0};
constexpr Coor Coor::MAX = {std::numeric_limits<size_t>::max(),
                            std::numeric_limits<size_t>::max()};
constexpr Coor Coor::INVALID = {std::numeric_limits<size_t>::max() - 1,
                                std::numeric_limits<size_t>::max() - 1};
export struct Interval {
  // [start, end]
  Coor start;
  Coor end;
  static const Interval INVALID;
  Interval()
      : start(Coor::MAX), end(Coor::MIN) {} // by default is an invalid interval
  Interval(Coor start, Coor end) : start(start), end(end) {}
  bool valid() const { return start <= end; }
};
const Interval Interval::INVALID = Interval(Coor::MAX, Coor::MIN);

// Syntax highlighting types
export enum class TokenType {
  KEYWORD,
  NUMBER,
  STRING,
  IDENTIFIER,
  COMMENT,
  PREPROCESSOR,
  OPEN_CURLY_BRACE,
  CLOSE_CURLY_BRACE,
  OPEN_SQUARE_BRACE,
  CLOSE_SQUARE_BRACE,
  OPEN_PARENTHESIS,
  CLOSE_PARENTHESIS,
  OPEN_ANGLE_BRACE,
  CLOSE_ANGLE_BRACE,
  WHITESPACE,
  NORMAL,
  MISMATCHED_BRACE,
};

export struct Token {
  TokenType type;
  size_t start;
  size_t end; // exclusive
  int braceColorIndex =
      -1; // Color index for matching braces (-1 means no color assigned)
};

export class Buffer {
  std::vector<std::string> buffer;

public:
  Buffer();
  Buffer *append(const std::string &s);
  Coor insert(Coor coor, const std::string &s);
  Coor insertLines(Coor coor, const std::vector<std::string> &lines);
  // dels [coor1, coor2)
  Coor del(Coor coor1, Coor coor2);
  Coor delLineKeepEmptyLine(Coor coor);
  Coor delLine(Coor coor);
  Coor delLine(size_t row);
  const std::string &getLine(size_t row) const;
  std::string &getLine(size_t row);
  std::vector<std::string> getLines(Coor coor1, Coor coor2) const;
  std::vector<std::string> getLines(size_t row1, size_t row2) const;
  size_t getLineSize(size_t row) const { return buffer[row].size(); }
  size_t getLineSize(Coor coor) const { return getLineSize(coor.row); }
  size_t lineCount() const;
  void breakLine(size_t row, size_t col);
  DeletionResult backspace(size_t row, size_t col);
  Interval search(const std::string &s, Coor coor, bool isForward) const;
  Coor firstNoneWhitespaceChar(Coor coor) const;
  Coor lastNoneWhitespaceChar(Coor coor) const;
  // support for operations
  Coor replace(Coor coor, char c);
  Coor replaceMode(Coor coor, char c);
  Coor appendNextLineOntoCurrentLine(Coor coor);
  Coor indent(Coor coor);
  Coor dedent(Coor coor);

  // Auto-indentation based on brace nesting
  size_t getBraceNestingLevel(size_t row) const;
  Coor autoIndent(Coor coor);

  // Find matching bracket for % command
  Coor findMatchingBracket(Coor coor) const;

  // support for motion
  Coor nextWord(Coor coor) const;
  Coor prevWord(Coor coor) const;
  Coor lineStart(Coor coor) const;
  Coor lineEnd(Coor coor) const;
  Coor nextSameLineMatchingChar(Coor coor, char c) const;
  Coor lastSameLineMatchingChar(Coor coor, char c) const;
  bool hasTrailingWhitespace(Coor coor) const;

  // file I/O
  static std::vector<std::string> readFile(const std::string &filename);
  void saveToFile(const std::string &filename) const;
  void loadFromLines(const std::vector<std::string> &lines);

  // syntax highlighting
  std::vector<std::vector<Token>> getTokens() const;
};

export struct State {
  Buffer buffer;
  Coor cursor;
  State(Buffer buffer, Coor cursor) : buffer(buffer), cursor(cursor) {}
};
