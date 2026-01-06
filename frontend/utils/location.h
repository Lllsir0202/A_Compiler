#ifndef LOCATION_H
#define LOCATION_H
#include <cstdint>
#include <iomanip>
#include <ostream>

namespace frontend {
// 记录源代码位置的结构体
/**
 * @param file_id 文件ID[[maybe_unused]],因为目前只处理单一文件,所以该字段暂未使用
 * @param line 行号
 * @param column 列号
 */
struct SourceLocation {
  [[maybe_unused]] int32_t file_id_;
  uint32_t offset_;
  uint32_t line_;
  uint32_t column_;

 private:
  SourceLocation(uint32_t offset, uint32_t line, uint32_t column) : offset_(offset), line_(line), column_(column) {}
  friend struct SourceRange;

  // 提供接口用于读取行号和列号(以及文件ID)
 public:
  [[maybe_unused]] auto GetOffset() const -> uint32_t { return offset_; }
  auto GetLine() const -> uint32_t { return line_; }
  auto GetColumn() const -> uint32_t { return column_; }
  [[maybe_unused]] auto GetFileId() const -> int32_t { return file_id_; }
};

// 记录源代码范围的结构体
/**
 * @param begin_ 起始位置
 * @param end_ 结束位置
 */
struct SourceRange {
  SourceLocation begin_;
  SourceLocation end_;

 private:
  SourceRange(SourceLocation begin, SourceLocation end) : begin_(begin), end_(end) {}

 public:
  // 提供静态接口用于创建SourceRange对象
  static auto GetRange(uint32_t begin_offset, uint32_t begin_line, uint32_t begin_column, uint32_t end_offset,
                       uint32_t end_line, uint32_t end_column) -> SourceRange {
    return {SourceLocation(begin_offset, begin_line, begin_column), SourceLocation(end_offset, end_line, end_column)};
  }
  auto GetBegin() const -> SourceLocation { return begin_; }
  auto GetEnd() const -> SourceLocation { return end_; }
  // TODO(): 添加输出函数,用于输出SourceRange信息,传入ostream对象
  auto Print(std::ostream &out) -> void {
    // since the location will be at the end of a line for a variable, so we need to add '\n'
    out << std::left << std::setw(8) << GetBegin().line_ << std::setw(8) << GetBegin().column_ << "\n";
  }
  // TODO(): 重载赋值运算符
};

};  // namespace frontend

#endif