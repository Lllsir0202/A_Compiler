#ifndef LOCATION_H
#define LOCATION_H
#include <cstdint>
#include <iomanip>
#include <ostream>
#include <string>

namespace frontend {
/**
 * @brief struct to record location of a token
 * @param file_id File id[[maybe_unused]], currently not used, reserved for future use when we support multiple files
 * @param offset the offset of the token in the source code[[maybe_unused]], reserved for multiple files compiling, starting from 0
 * @param line the line number of the token, starting from 0
 * @param column the column number of the token, starting from 0
 */
struct SourceLocation {
  [[maybe_unused]] int32_t file_id_;
  uint32_t offset_;
  uint32_t line_;
  uint32_t column_;

 public:
  SourceLocation() = default;

 private:
  SourceLocation(uint32_t offset, uint32_t line, uint32_t column) : offset_(offset), line_(line), column_(column) {}
  friend struct SourceRange;

  // Provide a method to get information from the SourceLocation
 public:
  [[maybe_unused]] auto GetOffset() const -> uint32_t { return offset_; }
  auto GetLine() const -> uint32_t { return line_; }
  auto GetColumn() const -> uint32_t { return column_; }
  [[maybe_unused]] auto GetFileId() const -> int32_t { return file_id_; }
};

// struct to record location range
/**
 * @param begin_ start location
 * @param end_ end location
 */
struct SourceRange {
  SourceLocation begin_;
  SourceLocation end_;

 public:
  SourceRange() = default;

 private:
  SourceRange(SourceLocation begin, SourceLocation end) : begin_(begin), end_(end) {}

 public:
  static auto GetRange(uint32_t begin_offset, uint32_t begin_line, uint32_t begin_column, uint32_t end_offset,
                       uint32_t end_line, uint32_t end_column) -> SourceRange {
    return {SourceLocation(begin_offset, begin_line, begin_column), SourceLocation(end_offset, end_line, end_column)};
  }
  auto GetBegin() const -> SourceLocation { return begin_; }
  auto GetEnd() const -> SourceLocation { return end_; }
  auto Print(std::ostream &out) -> void {
    // since the location will be at the end of a line for a variable, so we need to add '\n'
    out << std::left << std::setw(16) << GetBegin().line_ << std::setw(16) << GetBegin().column_ << "\n";
  }

  // Add overloaded Print with width parameters
  auto Print(std::ostream &out, int line_width, int column_width) -> void {
    out << std::left << std::setw(line_width) << GetBegin().line_ << std::setw(column_width) << GetBegin().column_
        << "\n";
  }

  // Add overloaded for error reporting
  auto ToString() const -> std::string {
    return "Line " + std::to_string(GetBegin().line_) + ", Column " + std::to_string(GetBegin().column_);
  }
};

};  // namespace frontend

#endif