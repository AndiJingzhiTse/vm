module Buffer;
import <fstream>;
import <stdexcept>;
import <cctype>;
import <set>;
import <string>;
import <utility>;

Buffer::Buffer() { buffer.push_back(""); }
Buffer *Buffer::append(const std::string &s) {
  std::stringstream ss(s);

  std::vector<std::string> lines;
  std::string line;

  while (std::getline(ss, line, '\n')) {
    lines.push_back(line);
  }

  for (size_t i = 0; i < lines.size(); ++i) {
    if (i == 0) {
      line = buffer.back() + lines[0];
      buffer.pop_back();
      buffer.push_back(line);
    } else
      buffer.push_back(lines[i]);
  }
  return this;
}
// if there is only one match in the buffer, both forward and backward must
// return the current match
Interval Buffer::search(const std::string &s, Coor coor, bool isForward) const {
  if (s.empty() || buffer.empty())
    return Interval::INVALID;
  size_t n = buffer.size();

  if (coor.row >= n)
    return Interval::INVALID;
  const std::string &line = buffer[coor.row];

  if (isForward) {
    size_t r = coor.row;
    size_t c = coor.col;

    bool atFirstChar = false;
    size_t matchStartPos = 0;
    if (coor.col + s.size() <= line.size() &&
        line.compare(coor.col, s.size(), s) == 0) {
      atFirstChar = true;
      matchStartPos = coor.col;
    }
    bool inMatchNotFirstChar = false;
    if (!atFirstChar && coor.col < line.size()) {
      size_t matchStart = line.rfind(s, coor.col);
      if (matchStart != std::string::npos && matchStart < coor.col &&
          matchStart + s.size() > coor.col) {
        inMatchNotFirstChar = true;
        matchStartPos = matchStart;
      }
    }

    // --- handle the special edge case: when the current position is at a match
    // and there are no other matches in the buffer, both forward and backward
    // must return the current match
    bool onlyOneMatchInBuffer = false;
    if (atFirstChar || inMatchNotFirstChar) {
      // Count how many matches of s in buffer
      size_t matchCount = 0;
      for (size_t i = 0; i < n; ++i) {
        const std::string &checkLine = buffer[i];
        size_t searchStart = 0;
        while (searchStart + s.size() <= checkLine.size()) {
          size_t pos = checkLine.find(s, searchStart);
          if (pos != std::string::npos) {
            ++matchCount;
            // optimization: more than 1 found, break
            if (matchCount > 1)
              break;
            searchStart = pos + 1;
          } else {
            break;
          }
        }
        if (matchCount > 1)
          break;
      }
      if (matchCount == 1) {
        onlyOneMatchInBuffer = true;
      }
    }

    if ((atFirstChar || inMatchNotFirstChar) && onlyOneMatchInBuffer) {
      // exactly one match in buffer and we're at it: return its interval
      return Interval{Coor{r, matchStartPos},
                      Coor{r, matchStartPos + s.size() - 1}};
    }
    if (atFirstChar) {
      // Search for "next" match (not this one)
      size_t curR = r;
      size_t curC = c + 1;
      while (curR < n) {
        const std::string &curLine = buffer[curR];
        size_t searchStart = (curR == r ? curC : 0);
        size_t pos = curLine.find(s, searchStart);
        if (pos != std::string::npos) {
          return Interval{Coor{curR, pos}, Coor{curR, pos + s.size() - 1}};
        }
        ++curR;
        curC = 0;
      }
      // Wrap: search from start to position before the original
      for (size_t wrapR = 0; wrapR <= r; ++wrapR) {
        const std::string &wrapLine = buffer[wrapR];
        size_t limit = wrapLine.size();
        if (wrapR == r) {
          limit = (c > 0) ? c : 0;
        }
        size_t searchStart = 0;
        while (searchStart + s.size() <= limit) {
          size_t pos = wrapLine.find(s, searchStart);
          if (pos != std::string::npos && pos < limit) {
            return Interval{Coor{wrapR, pos}, Coor{wrapR, pos + s.size() - 1}};
          }
          if (pos == std::string::npos)
            break;
          searchStart = pos + 1;
        }
      }
    } else if (inMatchNotFirstChar) {
      // We are inside a match but not at first char; true must skip to next
      size_t curR = r;
      size_t curC = c + 1;
      while (curR < n) {
        const std::string &curLine = buffer[curR];
        size_t searchStart = (curR == r ? curC : 0);
        size_t pos = curLine.find(s, searchStart);
        if (pos != std::string::npos) {
          return Interval{Coor{curR, pos}, Coor{curR, pos + s.size() - 1}};
        }
        ++curR;
        curC = 0;
      }
      // Wrap: search from start to the (inside) position (but don't match this)
      for (size_t wrapR = 0; wrapR <= r; ++wrapR) {
        const std::string &wrapLine = buffer[wrapR];
        size_t limit = wrapLine.size();
        if (wrapR == r) {
          limit = (c > 0) ? c : 0;
        }
        size_t searchStart = 0;
        while (searchStart + s.size() <= limit) {
          size_t pos = wrapLine.find(s, searchStart);
          if (pos != std::string::npos && pos < limit) {
            return Interval{Coor{wrapR, pos}, Coor{wrapR, pos + s.size() - 1}};
          }
          if (pos == std::string::npos)
            break;
          searchStart = pos + 1;
        }
      }
    } else {
      // Not inside a match, regular behavior: search at/after position
      size_t curR = r;
      size_t curC = c;
      while (curR < n) {
        const std::string &curLine = buffer[curR];
        size_t searchStart = (curR == r ? curC : 0);
        size_t pos = curLine.find(s, searchStart);
        if (pos != std::string::npos) {
          return Interval{Coor{curR, pos}, Coor{curR, pos + s.size() - 1}};
        }
        ++curR;
        curC = 0;
      }
      for (size_t wrapR = 0; wrapR <= r; ++wrapR) {
        const std::string &wrapLine = buffer[wrapR];
        size_t limit = wrapLine.size();
        if (wrapR == r) {
          limit = (c > 0) ? c : 0;
        }
        size_t searchStart = 0;
        while (searchStart + s.size() <= limit) {
          size_t pos = wrapLine.find(s, searchStart);
          if (pos != std::string::npos && pos < limit) {
            return Interval{Coor{wrapR, pos}, Coor{wrapR, pos + s.size() - 1}};
          }
          if (pos == std::string::npos)
            break;
          searchStart = pos + 1;
        }
      }
    }
  } else {
    // isForward == false (backward)
    size_t r = coor.row;
    size_t c = coor.col;

    bool atFirstChar = false;
    size_t matchStartPos = 0;
    if (coor.col + s.size() <= line.size() &&
        line.compare(coor.col, s.size(), s) == 0) {
      atFirstChar = true;
      matchStartPos = coor.col;
    }
    bool inMatchNotFirstChar = false;
    if (!atFirstChar && coor.col < line.size()) {
      size_t matchStart = line.rfind(s, coor.col);
      if (matchStart != std::string::npos && matchStart < coor.col &&
          matchStart + s.size() > coor.col) {
        inMatchNotFirstChar = true;
        matchStartPos = matchStart;
      }
    }

    // --- handle the special edge case: when the current position is at a match
    // and there are no other matches in the buffer, both forward and backward
    // must return the current match
    bool onlyOneMatchInBuffer = false;
    if (atFirstChar || inMatchNotFirstChar) {
      // Count how many matches of s in buffer
      size_t matchCount = 0;
      for (size_t i = 0; i < n; ++i) {
        const std::string &checkLine = buffer[i];
        size_t searchStart = 0;
        while (searchStart + s.size() <= checkLine.size()) {
          size_t pos = checkLine.find(s, searchStart);
          if (pos != std::string::npos) {
            ++matchCount;
            if (matchCount > 1)
              break;
            searchStart = pos + 1;
          } else {
            break;
          }
        }
        if (matchCount > 1)
          break;
      }
      if (matchCount == 1) {
        onlyOneMatchInBuffer = true;
      }
    }

    if ((atFirstChar || inMatchNotFirstChar) && onlyOneMatchInBuffer) {
      // exactly one match in buffer and we're at it: return its interval
      return Interval{Coor{r, matchStartPos},
                      Coor{r, matchStartPos + s.size() - 1}};
    } else if (atFirstChar) {
      // Move search to "last previous" match before this one (exclude current)
      size_t curR = r;
      ssize_t curLimit = coor.col - 1;
      while (true) {
        const std::string &curLine = buffer[curR];
        ssize_t searchEnd = (ssize_t)curLine.size() - s.size();
        if (searchEnd < 0)
          searchEnd = -1;

        ssize_t limit = (curR == r) ? (curLimit - s.size() + 1) : searchEnd;
        if (limit < 0)
          limit = -1;

        for (ssize_t i = limit; i >= 0; --i) {
          if (curLine.compare(i, s.size(), s) == 0) {
            return Interval{Coor{curR, static_cast<size_t>(i)},
                            Coor{curR, static_cast<size_t>(i + s.size() - 1)}};
          }
        }
        if (curR == 0)
          break;
        --curR;
      }
      // Wrapping: search from end to after original interval
      for (ssize_t wrapR = n - 1; wrapR >= (ssize_t)r; --wrapR) {
        const std::string &wrapLine = buffer[wrapR];
        ssize_t searchEnd = (ssize_t)wrapLine.size() - s.size();
        if (searchEnd < 0)
          searchEnd = -1;
        ssize_t lower = (wrapR == (ssize_t)r) ? coor.col + 1 : 0;
        for (ssize_t i = searchEnd; i >= lower; --i) {
          if (wrapLine.compare(i, s.size(), s) == 0) {
            return Interval{
                Coor{static_cast<size_t>(wrapR), static_cast<size_t>(i)},
                Coor{static_cast<size_t>(wrapR),
                     static_cast<size_t>(i + s.size() - 1)}};
          }
        }
      }
    } else if (inMatchNotFirstChar) {
      // We're inside a match but not at the first char - return this match!
      size_t matchStart = line.rfind(s, coor.col);
      return Interval{Coor{r, matchStart}, Coor{r, matchStart + s.size() - 1}};
    } else {
      // Regular backward search from coor.col - 1 down
      size_t curR = r;
      ssize_t curLimit = c - s.size();
      if ((ssize_t)curLimit < 0)
        curLimit = -1;
      while (true) {
        const std::string &curLine = buffer[curR];
        ssize_t searchEnd = (ssize_t)curLine.size() - s.size();
        if (searchEnd < 0)
          searchEnd = -1;
        ssize_t limit = (curR == r) ? curLimit : searchEnd;
        for (ssize_t i = limit; i >= 0; --i) {
          if (curLine.compare(i, s.size(), s) == 0) {
            return Interval{Coor{curR, static_cast<size_t>(i)},
                            Coor{curR, static_cast<size_t>(i + s.size() - 1)}};
          }
        }
        if (curR == 0)
          break;
        --curR;
      }
      // Wrapping - from end down to after original coor
      for (ssize_t wrapR = n - 1; wrapR >= (ssize_t)r; --wrapR) {
        const std::string &wrapLine = buffer[wrapR];
        ssize_t searchEnd = (ssize_t)wrapLine.size() - s.size();
        if (searchEnd < 0)
          searchEnd = -1;
        ssize_t lower = (wrapR == (ssize_t)r) ? coor.col + 1 : 0;
        for (ssize_t i = searchEnd; i >= lower; --i) {
          if (wrapLine.compare(i, s.size(), s) == 0) {
            return Interval{
                Coor{static_cast<size_t>(wrapR), static_cast<size_t>(i)},
                Coor{static_cast<size_t>(wrapR),
                     static_cast<size_t>(i + s.size() - 1)}};
          }
        }
      }
    }
  }
  return Interval::INVALID;
}
Coor Buffer::insert(Coor coor, const std::string &s) {
  size_t row = coor.row;
  size_t col = coor.col;
  std::stringstream ss(s);
  std::vector<std::string> lines;
  std::string line;

  // Split s into lines
  while (std::getline(ss, line, '\n')) {
    lines.push_back(line);
  }
  if (lines.empty()) {
    return coor;
  }

  // Handle inserting above the start of the buffer (prepend)
  if (row < 0) {
    buffer.insert(buffer.begin(), lines[0]);
    for (size_t i = 1; i < lines.size(); ++i) {
      buffer.insert(buffer.begin() + i, lines[i]);
    }
    // The last char in last line inserted, which is at (lines.size()-1, ...)
    if (lines.empty())
      return Coor{0, 0};
    size_t lastRow = lines.size() - 1;
    size_t lastCol = lines.back().size();
    // If last line is empty, put at col 0, otherwise col = length - 1
    return Coor{lastRow, lastCol == 0 ? 0 : lastCol - 1};
  } else if (row >= buffer.size()) {
    // Append to end
    for (const auto &part : lines) {
      buffer.push_back(part);
    }
    size_t lastRow = buffer.size() - 1;
    size_t lastCol = buffer.back().size();
    return Coor{lastRow, lastCol == 0 ? 0 : lastCol - 1};
  }

  // Inserting into existing text
  std::string &targetLine = buffer[row];
  // Clamp col to line size to prevent out-of-bounds access
  size_t clampedCol = std::min(col, targetLine.size());
  std::string left = targetLine.substr(0, clampedCol);
  std::string right = targetLine.substr(clampedCol);

  // Insert first line at [row], then insert rest, adjusting 'right' as needed
  buffer[row] = left + lines[0];

  if (lines.size() > 1) {
    for (size_t i = 1; i < lines.size(); ++i) {
      buffer.insert(buffer.begin() + row + i, lines[i]);
    }
    buffer[row + lines.size() - 1] +=
        right; // Only append 'right' to the last inserted line

    size_t lastRow = row + lines.size() - 1;
    size_t lastCol = buffer[lastRow].size();
    return Coor{lastRow, lastCol == 0 ? 0 : lastCol - 1};
  } else {
    buffer[row] += right;
    size_t lastCol = (left + lines[0]).size();
    // If inserted string is empty, col should not move forward
    // Use clampedCol as base position, then add length of inserted text
    return Coor{row, lastCol == 0 ? clampedCol : lastCol - 1};
  }
}
Coor Buffer::insertLines(Coor coor, const std::vector<std::string> &lines) {
  for (const std::string &line : lines) {
    coor = insert(coor, line);
    if (&line != &lines.back()) {
      breakLine(coor.row, buffer[coor.row].size());
      coor = Coor{coor.row + 1, 0};
    }
  }
  return coor;
}
Coor Buffer::del(Coor coor1, Coor coor2) {
  // del [(row, 0), (row, size[row])) means to remove all the characters but
  // keep the empty line
  // del [(row, 0), (row+1, 0)] means to remove all the
  // characters and the empty line
  // NEW: del [(row-1, size[row-1]), (row, size[row])] also removes line
  // 'row'

  // Ensure coor1 <= coor2, swap if needed
  if (coor1.row > coor2.row ||
      (coor1.row == coor2.row && coor1.col > coor2.col)) {
    std::swap(coor1, coor2);
  }
  size_t row1 = coor1.row;
  size_t col1 = coor1.col;
  size_t row2 = coor2.row;
  size_t col2 = coor2.col;
  // Clamp bounds
  if (buffer.empty())
    return coor1;
  // Clamp coor1
  size_t c_row1 = std::min(std::max(row1, size_t(0)), buffer.size() - 1);
  size_t c_col1 = std::min(std::max(col1, size_t(0)), buffer[c_row1].size());

  // Clamp coor2
  size_t c_row2 = std::min(std::max(row2, size_t(0)), buffer.size() - 1);
  size_t c_col2 = std::min(std::max(col2, size_t(0)), buffer[c_row2].size());

  // If the range is invalid (no-op)
  if (c_row1 > c_row2 || (c_row1 == c_row2 && c_col1 >= c_col2)) {
    return coor1;
  }

  if (c_row1 == c_row2) {
    // Single line deletion
    std::string &line = buffer[c_row1];
    if (c_col2 > line.size())
      c_col2 = line.size();

    // Check if deleting entire line: [(row, 0), (row, size[row]))
    if (c_col1 == 0 && c_col2 == line.size()) {
      // del all characters but keep empty line
      line.clear();
      coor1.col = 0;
      return coor1;
    }

    // Partial line deletion
    line.erase(c_col1, c_col2 - c_col1);
    if (buffer[c_row1].empty()) {
      coor1.col = 0;
    } else if (coor1.col >= buffer[c_row1].size()) {
      coor1.col = buffer[c_row1].size() - 1;
    }
    return coor1;
  }

  // Multi-line deletion
  // Standard: Check if deleting entire line(s): [(row, 0), (row+1, 0)]
  if (c_col1 == 0 && c_col2 == 0 && c_row2 == c_row1 + 1) {
    // Remove the entire line at row1
    buffer.erase(buffer.begin() + c_row1);
    // Cursor should be at start of what was row2 (now row1)
    if (c_row1 < buffer.size()) {
      coor1.row = c_row1;
      coor1.col = 0;
    } else {
      // If we deld the last line, go to end of previous line
      if (c_row1 > 0) {
        coor1.row = c_row1 - 1;
        coor1.col = buffer[c_row1 - 1].size();
      } else {
        coor1.row = 0;
        coor1.col = 0;
      }
    }
    return coor1;
  }

  // NEW: Check if deleting entire line(s) by [prevEnd, currEnd]:
  // that is, [(row-1, size[row-1]), (row, size[row])]
  if (
      // for buffer index safety
      c_row1 + 1 == c_row2 && c_col1 == buffer[c_row1].size() &&
      c_col2 == buffer[c_row2].size()) {
    // Remove the entire line at c_row2
    buffer.erase(buffer.begin() + c_row2);
    // Place cursor at the end of previous line (c_row1)
    coor1.row = c_row1;
    coor1.col = buffer[c_row1].size();
    return coor1;
  }

  // Multiple lines: del from first col1 to end, all middle, col0 to col2 in
  // last
  std::string &firstLine = buffer[c_row1];
  std::string &lastLine = buffer[c_row2];
  std::string merged = firstLine.substr(0, c_col1) + lastLine.substr(c_col2);

  // Remove lines in between
  buffer.erase(buffer.begin() + c_row1, buffer.begin() + c_row2 + 1);

  // Insert newly merged line in place of first deld line
  buffer.insert(buffer.begin() + c_row1, merged);

  if (buffer[c_row1].empty()) {
    coor1.col = 0;
  } else if (coor1.col >= buffer[c_row1].size()) {
    coor1.col = buffer[c_row1].size() - 1;
  }
  return coor1;
}
Coor Buffer::delLineKeepEmptyLine(Coor coor) {
  // dels the entire line but always leaves an empty line in its place,
  // and the cursor is placed at (row, 0).

  if (buffer.empty()) {
    return Coor{0, 0};
  }
  size_t row = coor.row;
  if (row >= buffer.size()) {
    row = buffer.size() - 1;
  }

  buffer[row].clear();
  return Coor{row, 0};
}
Coor Buffer::delLine(Coor coor) { return delLine(coor.row); }
Coor Buffer::delLine(size_t row) {
  if (buffer.size() <= 1) {
    // Can't del the last line - just clear it
    buffer[0].clear();
    return Coor{0, 0};
  }

  if (row == buffer.size() - 1) {
    // Deleting last line - move to previous line
    buffer.erase(buffer.begin() + row);
    return Coor{row - 1, 0};
  } else {
    // Deleting middle line - stay at same row index
    buffer.erase(buffer.begin() + row);
    return Coor{row, 0};
  }
}
const std::string &Buffer::getLine(size_t row) const { return buffer[row]; }
std::string &Buffer::getLine(size_t row) { return buffer[row]; }
std::vector<std::string> Buffer::getLines(Coor coor1, Coor coor2) const {
  std::vector<std::string> result;

  // Clamp: coor1 <= coor2
  Coor c1 = coor1;
  Coor c2 = coor2;
  if (c1.row > c2.row || (c1.row == c2.row && c1.col > c2.col)) {
    std::swap(c1, c2);
  }

  // If buffer is empty, or c1.row is out of bounds, return empty vector
  if (buffer.empty() || c1.row >= buffer.size()) {
    return result;
  }

  // Clamp c2.row if out-of-bounds
  size_t last_row = buffer.size() - 1;
  if (c2.row > last_row)
    c2.row = last_row;

  // If c1 and c2 on same row
  if (c1.row == c2.row) {
    const std::string &line = buffer[c1.row];
    if (c1.col >= line.size()) {
      // Starting column is out of bounds; no string is included
      return result;
    }
    // If c2.col >= line.size(), we want to return the line from c1.col to end
    if (c2.col >= line.size()) {
      result.push_back(line.substr(c1.col));
    } else {
      result.push_back(line.substr(c1.col, c2.col - c1.col));
    }
    return result;
  }

  // Multiple lines
  // First line: [c1.col, end)
  const std::string &firstLine = buffer[c1.row];
  if (c1.col < firstLine.size()) {
    result.push_back(firstLine.substr(c1.col));
  }
  // If c1.col >= firstLine.size(), skip the first line (do not push empty
  // string)

  // Middle lines: [c1.row+1, c2.row)
  for (size_t r = c1.row + 1; r < c2.row; ++r) {
    result.push_back(buffer[r]);
  }

  // Last line (c2.row)
  const std::string &lastLine = buffer[c2.row];
  if (c2.col >= lastLine.size()) {
    // If c2.col out-of-bounds, the whole line is included
    result.push_back(lastLine);
  } else if (c2.col > 0) {
    result.push_back(lastLine.substr(0, c2.col));
  } else {
    // c2.col == 0, empty string
    result.push_back("");
  }

  return result;
}
std::vector<std::string> Buffer::getLines(size_t row1, size_t row2) const {
  if (row1 >= buffer.size()) {
    return std::vector<std::string>();
  }
  size_t lastRow = buffer.size() - 1;
  size_t r2 = row2 >= buffer.size() ? lastRow : row2;
  return std::vector<std::string>(buffer.begin() + row1,
                                  buffer.begin() + r2 + 1);
}
size_t Buffer::lineCount() const { return buffer.size(); }
void Buffer::breakLine(size_t row, size_t col) {
  std::string line = buffer[row];
  buffer[row] = line.substr(0, col);
  buffer.insert(buffer.begin() + row + 1, line.substr(col));
}
DeletionResult Buffer::backspace(size_t row, size_t col) {
  if (row == 0 && col == 0) {
    // At the very beginning - nothing to del
    return DeletionResult::Nothing;
  }

  if (col > 0) {
    // del a character from current line
    std::string &line = buffer[row];
    line.erase(col - 1, 1);
    return DeletionResult::Character;
  }

  // col == 0, so we're at the start of a line (but not first line)
  // Merge with previous line
  std::string &prevLine = buffer[row - 1];
  std::string &currLine = buffer[row];
  prevLine += currLine;
  buffer.erase(buffer.begin() + row);
  return DeletionResult::Newline;
}
// returns the coor of the first char in the next word or new line character
Coor Buffer::nextWord(Coor coor) const {
  size_t row = coor.row;
  size_t col = coor.col;
  while (row < buffer.size()) {
    const std::string &line = buffer[row];
    // If on the starting line, start from col; otherwise, from 0
    size_t searchCol = (row == coor.row) ? col : 0;

    // Find the first nonspace after col
    size_t wordStart = line.find_first_not_of(" \t\r\n", searchCol);
    if (wordStart == std::string::npos) {
      // Line is empty or only whitespace
      if (row > coor.row) {
        // Finished previous line; this line is empty or all spaces, but it's a
        // new line Continue to next line (per instructions)
        row++;
        col = 0;
        continue;
      } else {
        // Still on the original line: advance to next line
        row++;
        col = 0;
        continue;
      }
    } else {
      // If on the starting line and our cursor is in the middle of a word, need
      // to look ahead. If we're inside a word already, move to the end, then
      // search new word.
      if (row == coor.row && wordStart <= col) {
        // Are we inside a word at col?
        size_t wordEnd = line.find_first_of(" \t\r\n", col);
        if (wordEnd == std::string::npos) {
          // At end of line, need to go to next line
          row++;
          col = 0;
          continue;
        } else {
          // There may be a next word after this one, keep searching from
          // wordEnd
          size_t nextStart = line.find_first_not_of(" \t\r\n", wordEnd);
          if (nextStart == std::string::npos) {
            // No next word, go to next line
            row++;
            col = 0;
            continue;
          } else {
            return Coor{row, nextStart};
          }
        }
      } else {
        // On a new line after coor.row, or just found first word after
        // whitespace
        return Coor{row, wordStart};
      }
    }
  }
  // If we get here, there is no next word, return last position (end of buffer)
  if (!buffer.empty()) {
    return Coor{buffer.size() - 1, buffer.back().size()};
  } else {
    return Coor{0, 0};
  }
}
Coor Buffer::prevWord(Coor coor) const {
  // Traverse backwards to find the previous word start (Vim 'b' motion style)
  const auto &buffer = this->buffer;

  size_t row = coor.row;
  size_t col = coor.col;

  // Adjust to previous char if exactly at start of word (exclude case col==0 at
  // very first buffer char)
  if (row >= buffer.size())
    row = buffer.size() - 1;
  if (row < 0)
    return Coor{0, 0};

  if (col == 0) {
    // If at beginning of a line, move to end of previous line
    if (row == 0)
      return Coor{0, 0};
    row--;
    col = buffer[row].size();
  }

  // Start searching
  while (row >= 0) {
    const std::string &line = buffer[row];
    if (line.empty() && row == 0)
      break;
    if (col > line.size())
      col = line.size();
    // Search left for first non-space
    size_t i = col;
    // 1. Skip spaces (left)
    while (i > 0 && isspace(line[i - 1]))
      --i;
    // 2. Skip word (left)
    while (i > 0 && !isspace(line[i - 1]))
      --i;
    // If we found non-space and i < current col, and it's a word char, return
    if (i < col && i < line.size() && !isspace(line[i])) {
      return Coor{row, (size_t)i};
    }
    // If no word found in this line, go to previous line's end
    row--;
    if (row >= 0)
      col = buffer[row].size();
  }
  // If nothing found, return very start
  return Coor{0, 0};
}
Coor Buffer::lineStart(Coor coor) const { return Coor{coor.row, 0}; }
Coor Buffer::lineEnd(Coor coor) const {
  return Coor{coor.row, buffer[coor.row].size() - 1};
}
Coor Buffer::nextSameLineMatchingChar(Coor coor, char c) const {
  // Return input coor if out of range
  if (coor.row >= buffer.size())
    return coor;
  const std::string &line = buffer[coor.row];
  if (coor.col + 1 >= line.size())
    return coor;

  size_t pos = line.find(c, coor.col + 1);
  if (pos != std::string::npos) {
    return Coor{coor.row, pos};
  } else {
    return coor;
  }
}
Coor Buffer::lastSameLineMatchingChar(Coor coor, char c) const {
  // Return input coor if out of range
  if (coor.row >= buffer.size())
    return coor;
  const std::string &line = buffer[coor.row];
  if (coor.col == 0 || coor.col > line.size())
    return coor;

  // Search backwards in the line for previous occurrence of c
  size_t pos = line.rfind(c, coor.col - 1);
  if (pos != std::string::npos) {
    return Coor{coor.row, pos};
  } else {
    return coor;
  }
}
Coor Buffer::replace(Coor coor, char c) {
  if (coor.row >= buffer.size() || buffer[coor.row].empty() ||
      coor.col >= buffer[coor.row].size()) {
    return coor;
  }
  buffer[coor.row][coor.col] = c;
  size_t size = buffer[coor.row].size();
  return Coor{coor.row, std::min(size - 1, coor.col + 1)};
}
Coor Buffer::replaceMode(Coor coor, char c) {
  if (coor.row >= buffer.size())
    return coor;

  std::string &line = buffer[coor.row];

  if (coor.col < line.size()) {
    line[coor.col] = c;
  } else if (coor.col == line.size()) {
    line.push_back(c);
  }
  return Coor{coor.row, coor.col + 1};
}
Coor Buffer::firstNoneWhitespaceChar(Coor coor) const {
  if (coor.row >= buffer.size())
    return coor;
  const std::string &line = buffer[coor.row];
  if (line.empty())
    return Coor{coor.row, 0};

  // Find first non-whitespace character
  for (size_t i = 0; i < line.size(); ++i) {
    if (!std::isspace(static_cast<unsigned char>(line[i]))) {
      return Coor{coor.row, i};
    }
  }
  // If all whitespace, return last character's coor
  return Coor{coor.row, line.size() > 0 ? (line.size() - 1) : 0};
}
Coor Buffer::lastNoneWhitespaceChar(Coor coor) const {
  if (coor.row >= buffer.size())
    return coor;
  const std::string &line = buffer[coor.row];
  if (line.empty())
    return Coor{coor.row, 0};

  // Search for last non-whitespace char starting from line.size()-1 backward
  for (size_t i = line.size(); i > 0; --i) {
    if (!std::isspace(static_cast<unsigned char>(line[i - 1]))) {
      return Coor{coor.row, i - 1};
    }
  }
  // If all whitespace, return beginning of line
  return Coor{coor.row, 0};
}
bool Buffer::hasTrailingWhitespace(Coor coor) const {
  if (coor.row >= buffer.size())
    return false;
  const std::string &line = buffer[coor.row];
  if (line.empty())
    return false; // no trailing whitespace in empty line
  // Skip if coor.col is beyond line
  if (coor.col >= line.size())
    return false;
  // Check if all characters after coor.col (inclusive or exclusive? assume
  // exclusive) are whitespace
  for (size_t i = coor.col + 1; i < line.size(); ++i) {
    if (!std::isspace(static_cast<unsigned char>(line[i]))) {
      return false;
    }
  }
  // If there are "characters" after coor.col and all are whitespace, return
  // true
  return (line.size() > coor.col + 1);
}
Coor Buffer::appendNextLineOntoCurrentLine(Coor coor) {
  // If row is last line, do nothing
  if (coor.row >= buffer.size() - 1) {
    return coor;
  }
  std::string &curLine = buffer[coor.row];
  std::string &nextLine = buffer[coor.row + 1];

  // Find if current line has trailing whitespace (from last non-ws to end)
  bool hasTrailingWS = false;
  if (!curLine.empty()) {
    size_t lastNonWS = curLine.find_last_not_of(" \t\r\n");
    if (lastNonWS != std::string::npos && lastNonWS < curLine.size() - 1) {
      hasTrailingWS = true;
    }
  }

  if (!hasTrailingWS && !curLine.empty()) {
    curLine += ' ';
  }
  curLine += nextLine;
  // Remove the next line
  buffer.erase(buffer.begin() + coor.row + 1);

  // Cursor position: put at original line, col unchanged (if past new line end,
  // clamp)
  size_t newCol =
      std::min(coor.col, curLine.size() > 0 ? curLine.size() - 1 : 0);
  return Coor{coor.row, newCol};
}

std::vector<std::string> Buffer::readFile(const std::string &filename) {
  std::vector<std::string> lines;
  std::ifstream file(filename);

  if (!file.is_open()) {
    throw std::runtime_error("Cannot open file: " + filename);
  }

  std::string line;
  while (std::getline(file, line)) {
    lines.push_back(line);
  }

  // If file was empty, return vector with one empty string
  if (lines.empty()) {
    lines.push_back("");
  }

  file.close();
  return lines;
}

void Buffer::saveToFile(const std::string &filename) const {
  std::ofstream file(filename);
  if (!file.is_open()) {
    throw std::runtime_error("Cannot open file for writing: " + filename);
  }

  for (size_t i = 0; i < buffer.size(); ++i) {
    file << buffer[i];
    // Add newline after each line except the last if it's empty
    if (i < buffer.size() - 1 || !buffer[i].empty()) {
      file << '\n';
    }
  }

  file.close();
}

void Buffer::loadFromLines(const std::vector<std::string> &lines) {
  buffer = lines;
  if (buffer.empty()) {
    buffer.push_back("");
  }
}

Coor Buffer::indent(Coor coor) {
  size_t row = coor.row;
  size_t col = coor.col;

  if (row >= buffer.size())
    return coor;
  std::string &line = buffer[row];
  if (line.empty())
    return coor;
  line = std::string(8, ' ') + line;
  return Coor{row, col + 8};
}
size_t Buffer::getBraceNestingLevel(size_t row) const {
  size_t nestingLevel = 0;
  bool inMultiLineComment = false;
  bool inString = false;
  char stringDelim = '\0';

  // Count opening and closing curly braces from the start of the file to the
  // start of the given row
  for (size_t i = 0; i < row && i < buffer.size(); ++i) {
    const std::string &line = buffer[i];
    bool inSingleLineComment = false;

    for (size_t j = 0; j < line.length(); ++j) {
      char c = line[j];
      char next = (j + 1 < line.length()) ? line[j + 1] : '\0';

      // Handle multi-line comments
      if (inMultiLineComment) {
        if (c == '*' && next == '/') {
          inMultiLineComment = false;
          j++; // Skip the '/'
        }
        continue;
      }

      // Handle single-line comments
      if (inSingleLineComment) {
        break; // Rest of line is comment
      }

      // Handle strings
      if (inString) {
        // Check for escaped quote
        if (c == '\\' && next == stringDelim) {
          j++; // Skip escaped quote
          continue;
        }
        // Check for escaped backslash
        if (c == '\\' && next == '\\') {
          j++; // Skip escaped backslash
          continue;
        }
        // Check for closing quote
        if (c == stringDelim) {
          inString = false;
          stringDelim = '\0';
        }
        continue;
      }

      // Check for string start
      if (c == '"' || c == '\'') {
        inString = true;
        stringDelim = c;
        continue;
      }

      // Check for single-line comment
      if (c == '/' && next == '/') {
        inSingleLineComment = true;
        break;
      }

      // Check for multi-line comment start
      if (c == '/' && next == '*') {
        inMultiLineComment = true;
        j++; // Skip the '*'
        continue;
      }

      // Count braces (only if not in string or comment)
      if (c == '{') {
        nestingLevel++;
      } else if (c == '}') {
        if (nestingLevel > 0) {
          nestingLevel--;
        }
      }
    }
  }

  return nestingLevel;
}

Coor Buffer::autoIndent(Coor coor) {
  size_t row = coor.row;
  size_t col = coor.col;

  if (row >= buffer.size()) {
    return coor;
  }

  // Get the brace nesting level for this line
  size_t nestingLevel = getBraceNestingLevel(row);

  // Calculate desired indentation: 8 spaces per nesting level
  size_t desiredIndent = nestingLevel * 8;

  std::string &line = buffer[row];

  // Find first non-whitespace character
  size_t firstNonWS = 0;
  while (firstNonWS < line.length() && std::isspace(line[firstNonWS])) {
    firstNonWS++;
  }

  // Get the content after whitespace
  std::string content = line.substr(firstNonWS);

  // Create new line with proper indentation
  line = std::string(desiredIndent, ' ') + content;

  // Update cursor column
  size_t newCol = desiredIndent;
  if (col < firstNonWS) {
    // Cursor was in whitespace, move to start of content
    newCol = desiredIndent;
  } else {
    // Cursor was in content, preserve relative position
    newCol = desiredIndent + (col - firstNonWS);
  }

  return Coor{row, newCol};
}

Coor Buffer::findMatchingBracket(Coor coor) const {
  size_t row = coor.row;
  size_t col = coor.col;

  if (row >= buffer.size()) {
    return coor;
  }

  const std::string &line = buffer[row];
  if (col >= line.length()) {
    return coor;
  }

  char bracket = line[col];
  char openBracket, closeBracket;
  bool isOpening = false;

  // Determine bracket type and direction
  if (bracket == '(') {
    openBracket = '(';
    closeBracket = ')';
    isOpening = true;
  } else if (bracket == ')') {
    openBracket = '(';
    closeBracket = ')';
  } else if (bracket == '[') {
    openBracket = '[';
    closeBracket = ']';
    isOpening = true;
  } else if (bracket == ']') {
    openBracket = '[';
    closeBracket = ']';
  } else if (bracket == '{') {
    openBracket = '{';
    closeBracket = '}';
    isOpening = true;
  } else if (bracket == '}') {
    openBracket = '{';
    closeBracket = '}';
  } else {
    // Not a bracket, return original position
    return coor;
  }

  // Track state for strings and comments
  bool inMultiLineComment = false;
  bool inString = false;
  char stringDelim = '\0';

  if (isOpening) {
    // Search forward for matching closing bracket
    int depth = 1;
    size_t startRow = row;
    size_t startCol = col;

    for (size_t i = startRow; i < buffer.size(); ++i) {
      const std::string &currentLine = buffer[i];
      size_t startJ = (i == startRow) ? startCol + 1 : 0;
      bool inSingleLineComment = false;

      for (size_t j = startJ; j < currentLine.length(); ++j) {
        char c = currentLine[j];
        char next = (j + 1 < currentLine.length()) ? currentLine[j + 1] : '\0';

        // Handle multi-line comments
        if (inMultiLineComment) {
          if (c == '*' && next == '/') {
            inMultiLineComment = false;
            j++;
          }
          continue;
        }

        // Handle single-line comments
        if (inSingleLineComment) {
          break;
        }

        // Handle strings
        if (inString) {
          if (c == '\\' && (next == stringDelim || next == '\\')) {
            j++; // Skip escaped character
            continue;
          }
          if (c == stringDelim) {
            inString = false;
            stringDelim = '\0';
          }
          continue;
        }

        // Check for string start
        if (c == '"' || c == '\'') {
          inString = true;
          stringDelim = c;
          continue;
        }

        // Check for single-line comment
        if (c == '/' && next == '/') {
          inSingleLineComment = true;
          break;
        }

        // Check for multi-line comment start
        if (c == '/' && next == '*') {
          inMultiLineComment = true;
          j++;
          continue;
        }

        // Count matching brackets
        if (c == openBracket) {
          depth++;
        } else if (c == closeBracket) {
          depth--;
          if (depth == 0) {
            return Coor{i, j};
          }
        }
      }
    }
  } else {
    // Search backward for matching opening bracket
    // Scan from the beginning, tracking depth until we reach the target closing
    // bracket
    int depth = 0;

    for (size_t i = 0; i <= row && i < buffer.size(); ++i) {
      const std::string &currentLine = buffer[i];
      size_t endJ = (i == row) ? col + 1 : currentLine.length();
      bool inSingleLineComment = false;

      for (size_t j = 0; j < endJ; ++j) {
        char c = currentLine[j];
        char next = (j + 1 < currentLine.length()) ? currentLine[j + 1] : '\0';

        // Handle multi-line comments
        if (inMultiLineComment) {
          if (c == '*' && next == '/') {
            inMultiLineComment = false;
            j++;
          }
          continue;
        }

        // Handle single-line comments
        if (inSingleLineComment) {
          break;
        }

        // Handle strings
        if (inString) {
          if (c == '\\' && (next == stringDelim || next == '\\')) {
            j++; // Skip escaped character
            continue;
          }
          if (c == stringDelim) {
            inString = false;
            stringDelim = '\0';
          }
          continue;
        }

        // Check for string start
        if (c == '"' || c == '\'') {
          inString = true;
          stringDelim = c;
          continue;
        }

        // Check for single-line comment
        if (c == '/' && next == '/') {
          inSingleLineComment = true;
          break;
        }

        // Check for multi-line comment start
        if (c == '/' && next == '*') {
          inMultiLineComment = true;
          j++;
          continue;
        }

        // Count brackets
        if (c == openBracket) {
          depth++;
        } else if (c == closeBracket) {
          if (i == row && j == col) {
            // Found the target closing bracket
            // Now search backward from here to find matching opening bracket
            int targetDepth = depth;

            // Reset state and search backward
            inMultiLineComment = false;
            inString = false;
            stringDelim = '\0';
            int currentDepth = 0;

            for (size_t bi = i; bi < buffer.size(); --bi) {
              const std::string &backLine = buffer[bi];
              size_t startJ = (bi == i) ? j - 1 : backLine.length() - 1;

              for (size_t bj = startJ; bj < backLine.length(); --bj) {
                char bc = backLine[bj];
                char bnext =
                    (bj + 1 < backLine.length()) ? backLine[bj + 1] : '\0';
                char bprev = (bj > 0) ? backLine[bj - 1] : '\0';

                // Handle multi-line comments (check for end marker)
                if (bc == '*' && bnext == '/') {
                  inMultiLineComment = true;
                  continue;
                }
                if (inMultiLineComment && bc == '/' && bprev == '*') {
                  inMultiLineComment = false;
                  continue;
                }

                if (inMultiLineComment) {
                  continue;
                }

                // Handle strings (check for unescaped quotes)
                if ((bc == '"' || bc == '\'') &&
                    (bj == 0 || backLine[bj - 1] != '\\')) {
                  inString = !inString;
                  if (inString) {
                    stringDelim = bc;
                  } else {
                    stringDelim = '\0';
                  }
                  continue;
                }

                if (inString) {
                  continue;
                }

                // Count brackets
                if (bc == closeBracket) {
                  currentDepth++;
                } else if (bc == openBracket) {
                  if (currentDepth == targetDepth - 1) {
                    return Coor{bi, bj};
                  }
                  if (currentDepth > 0) {
                    currentDepth--;
                  }
                }
              }

              if (bi == 0)
                break; // Prevent underflow
            }

            // No match found
            return coor;
          }
          if (depth > 0) {
            depth--;
          }
        }
      }
    }
  }

  // No match found, return original position
  return coor;
}

Coor Buffer::dedent(Coor coor) {
  size_t row = coor.row;
  size_t col = coor.col;
  if (row >= buffer.size())
    return coor;
  std::string &line = buffer[row];
  size_t wsCount = 0;
  while (wsCount < line.size() && line[wsCount] == ' ' && wsCount < 8) {
    ++wsCount;
  }
  if (wsCount == 8) {
    line.erase(0, 8);
    size_t newCol = (col > 8) ? (col - 8) : 0;
    return Coor{row, newCol};
  } else if (wsCount > 0) {
    line.erase(0, wsCount);
    size_t newCol = (col > wsCount) ? (col - wsCount) : 0;
    return Coor{row, newCol};
  }
  return coor;
}

// C++ keywords
static const std::set<std::string> cppKeywords = {
    "alignas",      "alignof",      "and",           "and_eq",
    "asm",          "auto",         "bitand",        "bitor",
    "bool",         "break",        "case",          "catch",
    "char",         "char8_t",      "char16_t",      "char32_t",
    "class",        "compl",        "concept",       "const",
    "consteval",    "constexpr",    "constinit",     "const_cast",
    "continue",     "co_await",     "co_return",     "co_yield",
    "decltype",     "default",      "delete",        "do",
    "double",       "dynamic_cast", "else",          "enum",
    "explicit",     "export",       "extern",        "false",
    "float",        "for",          "friend",        "goto",
    "if",           "inline",       "int",           "long",
    "mutable",      "namespace",    "new",           "noexcept",
    "not",          "not_eq",       "nullptr",       "module",
    "operator",     "or",           "or_eq",         "private",
    "protected",    "public",       "register",      "reinterpret_cast",
    "requires",     "return",       "short",         "signed",
    "sizeof",       "static",       "static_assert", "static_cast",
    "struct",       "switch",       "template",      "this",
    "thread_local", "throw",        "true",          "try",
    "typedef",      "typeid",       "typename",      "union",
    "unsigned",     "using",        "virtual",       "void",
    "volatile",     "wchar_t",      "while",         "xor",
    "xor_eq"};

std::vector<std::vector<Token>> Buffer::getTokens() const {
  std::vector<std::vector<Token>> result;

  // State tracking for multi-line constructs
  bool inMultiLineComment = false;
  bool inString = false;
  char stringDelimiter = '\0';

  // First pass: tokenize all lines
  for (size_t lineNum = 0; lineNum < buffer.size(); ++lineNum) {
    const std::string &line = buffer[lineNum];
    std::vector<Token> lineTokens;

    enum State {
      NORMAL,
      IN_STRING,
      IN_SINGLE_COMMENT,
      IN_MULTI_COMMENT,
      IN_IDENTIFIER,
      IN_NUMBER,
      IN_PREPROCESSOR
    };

    State state =
        inMultiLineComment ? IN_MULTI_COMMENT : (inString ? IN_STRING : NORMAL);
    size_t tokenStart = 0;
    TokenType currentType = TokenType::NORMAL;
    bool firstNonWS =
        true; // Track first non-whitespace character for preprocessor

    // If continuing from previous line's string, set up the token
    if (inString && state == IN_STRING) {
      tokenStart = 0;
      currentType = TokenType::STRING;
    }

    // If continuing from previous line's multiline comment, set up the token
    if (inMultiLineComment && state == IN_MULTI_COMMENT) {
      tokenStart = 0;
      currentType = TokenType::COMMENT;
      // If line is empty, create token immediately
      if (line.empty()) {
        lineTokens.push_back({currentType, 0, 0});
      }
    }

    for (size_t i = 0; i < line.length(); ++i) {
      char c = line[i];
      char next = (i + 1 < line.length()) ? line[i + 1] : '\0';

      switch (state) {
      case NORMAL:
        // Check for whitespace
        if (std::isspace(c)) {
          // Add whitespace token (merge consecutive whitespace)
          if (!lineTokens.empty() &&
              lineTokens.back().type == TokenType::WHITESPACE &&
              lineTokens.back().end == i) {
            lineTokens.back().end = i + 1;
          } else {
            lineTokens.push_back({TokenType::WHITESPACE, i, i + 1});
          }
          firstNonWS = false;
          continue;
        }

        // Check if this is first non-whitespace for preprocessor
        if (firstNonWS && c == '#') {
          state = IN_PREPROCESSOR;
          tokenStart = i;
          currentType = TokenType::PREPROCESSOR;
          firstNonWS = false;
          continue;
        }
        firstNonWS = false;

        // Check for braces
        if (c == '{') {
          lineTokens.push_back({TokenType::OPEN_CURLY_BRACE, i, i + 1});
          continue;
        } else if (c == '}') {
          lineTokens.push_back({TokenType::CLOSE_CURLY_BRACE, i, i + 1});
          continue;
        } else if (c == '[') {
          lineTokens.push_back({TokenType::OPEN_SQUARE_BRACE, i, i + 1});
          continue;
        } else if (c == ']') {
          lineTokens.push_back({TokenType::CLOSE_SQUARE_BRACE, i, i + 1});
          continue;
        } else if (c == '(') {
          lineTokens.push_back({TokenType::OPEN_PARENTHESIS, i, i + 1});
          continue;
        } else if (c == ')') {
          lineTokens.push_back({TokenType::CLOSE_PARENTHESIS, i, i + 1});
          continue;
        }
        // Note: Angle brackets are not highlighted (removed per user request)

        // Check for string
        if (c == '"' || c == '\'') {
          state = IN_STRING;
          stringDelimiter = c;
          tokenStart = i;
          currentType = TokenType::STRING;
          inString = true;
          continue;
        }

        // Check for comments
        if (c == '/' && next == '/') {
          state = IN_SINGLE_COMMENT;
          tokenStart = i;
          currentType = TokenType::COMMENT;
          i++; // Skip next '/'
          continue;
        } else if (c == '/' && next == '*') {
          state = IN_MULTI_COMMENT;
          tokenStart = i;
          currentType = TokenType::COMMENT;
          inMultiLineComment = true;
          i++; // Skip next '*'
          continue;
        }

        // Check for identifier (starts with letter or underscore)
        if (std::isalpha(c) || c == '_') {
          state = IN_IDENTIFIER;
          tokenStart = i;
          currentType = TokenType::IDENTIFIER;
          continue;
        }

        // Check for number (starts with digit)
        if (std::isdigit(c)) {
          state = IN_NUMBER;
          tokenStart = i;
          currentType = TokenType::NUMBER;
          continue;
        }

        // Everything else is NORMAL
        lineTokens.push_back({TokenType::NORMAL, i, i + 1});
        break;

      case IN_STRING:
        // Check for escaped quote - handle \"
        if (c == '\\' && next == stringDelimiter) {
          i++; // Skip the escaped quote, stay in string
          continue;
        }
        // Check for escaped backslash - handle \\ (fixed comment)
        if (c == '\\' && next == '\\') {
          i++; // Skip the escaped backslash, stay in string
          continue;
        }
        // Check for closing quote (must match the opening delimiter)
        if (c == stringDelimiter) {
          lineTokens.push_back({currentType, tokenStart, i + 1});
          state = NORMAL;
          inString = false;
          stringDelimiter = '\0';
        }
        // Everything else stays in string
        break;

      case IN_SINGLE_COMMENT:
        // Everything until end of line is comment
        // Will be closed at end of line
        break;

      case IN_MULTI_COMMENT:
        if (c == '*' && next == '/') {
          // Close the multiline comment
          lineTokens.push_back({currentType, tokenStart, i + 2});
          state = NORMAL;
          inMultiLineComment = false;
          i++; // Skip next '/'
        }
        // Everything else stays in comment - token will be closed at end of
        // line
        break;

      case IN_IDENTIFIER:
        // Continue identifier: letters, digits, underscores
        // Exit when whitespace or other non-identifier character is met
        if (std::isalnum(c) || c == '_') {
          // Stay in identifier state
        } else {
          // Identifier ended - check if it's a keyword
          std::string word = line.substr(tokenStart, i - tokenStart);
          if (cppKeywords.count(word)) {
            currentType = TokenType::KEYWORD;
          }
          lineTokens.push_back({currentType, tokenStart, i});
          state = NORMAL;
          i--; // Re-process this character
        }
        break;

      case IN_NUMBER:
        // Continue number: digits, decimal point, scientific notation, type
        // suffixes
        if (std::isdigit(c) || c == '.' || c == 'e' || c == 'E' || c == 'f' ||
            c == 'F' || c == 'l' || c == 'L' || c == 'x' || c == 'X' ||
            c == 'b' || c == 'B' ||
            (i > tokenStart && (c == '+' || c == '-'))) {
          // Valid number character - stay in NUMBER state
        } else {
          // Number ended
          lineTokens.push_back({currentType, tokenStart, i});
          state = NORMAL;
          i--; // Re-process this character
        }
        break;

      case IN_PREPROCESSOR:
        // Preprocessor directive continues to end of line
        // Will be closed at end of line
        break;
      }
    }

    // Close any open tokens at end of line
    if (state == IN_STRING) {
      // String continues to next line
      lineTokens.push_back({currentType, tokenStart, line.length()});
      // Keep inString = true for next line
    } else if (state == IN_SINGLE_COMMENT) {
      lineTokens.push_back({currentType, tokenStart, line.length()});
      inMultiLineComment = false;
    } else if (state == IN_MULTI_COMMENT) {
      lineTokens.push_back({currentType, tokenStart, line.length()});
      // Keep inMultiLineComment = true for next line
    } else if (state == IN_IDENTIFIER) {
      std::string word = line.substr(tokenStart);
      if (cppKeywords.count(word)) {
        currentType = TokenType::KEYWORD;
      }
      lineTokens.push_back({currentType, tokenStart, line.length()});
      inMultiLineComment = false;
    } else if (state == IN_NUMBER) {
      lineTokens.push_back({currentType, tokenStart, line.length()});
      inMultiLineComment = false;
    } else if (state == IN_PREPROCESSOR) {
      lineTokens.push_back({currentType, tokenStart, line.length()});
      inMultiLineComment = false;
    } else {
      inMultiLineComment = false;
    }

    result.push_back(lineTokens);
  }

  // Second pass: check for mismatched braces and assign colors to matching
  // pairs
  struct BraceInfo {
    TokenType type;
    size_t lineNum;
    size_t tokenIndex;
    int colorIndex;
  };
  std::vector<BraceInfo> stack;
  std::set<std::pair<size_t, size_t>> mismatchedTokens; // (lineNum, tokenIndex)

  for (size_t lineNum = 0; lineNum < result.size(); ++lineNum) {
    for (size_t tokenIndex = 0; tokenIndex < result[lineNum].size();
         ++tokenIndex) {
      Token &token = result[lineNum][tokenIndex];

      // Process brace tokens (braces inside strings/comments are already
      // tokenized as STRING/COMMENT, so we only process actual brace tokens)
      if (token.type == TokenType::OPEN_CURLY_BRACE) {
        // Use stack size as color index (nesting level) - cycles through 0-7
        int colorIdx = stack.size() % 8;
        stack.push_back(
            {TokenType::OPEN_CURLY_BRACE, lineNum, tokenIndex, colorIdx});
        token.braceColorIndex = colorIdx;
      } else if (token.type == TokenType::CLOSE_CURLY_BRACE) {
        if (!stack.empty() &&
            stack.back().type == TokenType::OPEN_CURLY_BRACE) {
          BraceInfo match = stack.back();
          stack.pop_back();
          // Assign the same color to the closing brace
          token.braceColorIndex = match.colorIndex;
        } else {
          // Mismatched closing brace
          mismatchedTokens.insert({lineNum, tokenIndex});
        }
      } else if (token.type == TokenType::OPEN_SQUARE_BRACE) {
        int colorIdx = stack.size() % 8;
        stack.push_back(
            {TokenType::OPEN_SQUARE_BRACE, lineNum, tokenIndex, colorIdx});
        token.braceColorIndex = colorIdx;
      } else if (token.type == TokenType::CLOSE_SQUARE_BRACE) {
        if (!stack.empty() &&
            stack.back().type == TokenType::OPEN_SQUARE_BRACE) {
          BraceInfo match = stack.back();
          stack.pop_back();
          token.braceColorIndex = match.colorIndex;
        } else {
          // Mismatched closing brace
          mismatchedTokens.insert({lineNum, tokenIndex});
        }
      } else if (token.type == TokenType::OPEN_PARENTHESIS) {
        int colorIdx = stack.size() % 8;
        stack.push_back(
            {TokenType::OPEN_PARENTHESIS, lineNum, tokenIndex, colorIdx});
        token.braceColorIndex = colorIdx;
      } else if (token.type == TokenType::CLOSE_PARENTHESIS) {
        if (!stack.empty() &&
            stack.back().type == TokenType::OPEN_PARENTHESIS) {
          BraceInfo match = stack.back();
          stack.pop_back();
          token.braceColorIndex = match.colorIndex;
        } else {
          // Mismatched closing brace
          mismatchedTokens.insert({lineNum, tokenIndex});
        }
      }
      // Note: Angle brackets are not highlighted (removed per user request)
    }
  }

  // Mark all unclosed opening braces as mismatched
  for (const auto &brace : stack) {
    mismatchedTokens.insert({brace.lineNum, brace.tokenIndex});
    // Clear color index for mismatched braces
    result[brace.lineNum][brace.tokenIndex].braceColorIndex = -1;
  }

  // Mark mismatched tokens
  for (const auto &mismatch : mismatchedTokens) {
    result[mismatch.first][mismatch.second].type = TokenType::MISMATCHED_BRACE;
    result[mismatch.first][mismatch.second].braceColorIndex = -1;
  }

  return result;
}