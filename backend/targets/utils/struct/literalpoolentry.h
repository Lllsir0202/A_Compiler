#ifndef BACKEND_TARGETS_UTILS_STRUCT_LITERALPOOLENTRY_H
#define BACKEND_TARGETS_UTILS_STRUCT_LITERALPOOLENTRY_H

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
namespace backend::targetir {
/**
 * @brief Used as an entry in the literal pool,
 * which contains the literal value and its offset in the literal pool.
 * @note The offset might be able to be used to do sdata optimization
 */
class TargetFloatLiteralPoolEntry {
 public:
  TargetFloatLiteralPoolEntry(float value, size_t offset, size_t index)
      : value_(value), offset_(offset), index_(index) {}
  TargetFloatLiteralPoolEntry() = default;
  auto operator<(const TargetFloatLiteralPoolEntry &other) -> bool { return index_ < other.index_; }

  auto GetOffset() const -> size_t { return offset_; }
  auto GetIndex() const -> size_t { return index_; }
  // Just output the literal value as a hex string.
  auto Emit(std::ofstream *out) const -> void {
    // memcpy the float value to a uint32_t, then output it as a hex string
    uint32_t bits = 0;
    std::memcpy(&bits, &value_, sizeof(float));
    *out << "    .word    0x" << std::hex << bits << std::dec;
    *out << "    # float literal: " << value_ << "\n";
  }

  auto GetName() const -> std::string { return ".LCPI_" + std::to_string(index_); }

 private:
  float value_;
  size_t offset_;
  size_t index_;  // The index of this entry in the literal pool, used for optimization
};

}  // namespace backend::targetir

#endif