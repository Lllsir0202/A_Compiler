#ifndef BACKEND_TARGETS_UTILS_BITSET_BITSET_H
#define BACKEND_TARGETS_UTILS_BITSET_BITSET_H

#include <cstdint>
#include <vector>

/**
 * @brief This file defines the BitSet class,
 * which is used to represent the live-in and live-out
 * @member:
 * 1. set
 * 2. reset
 * 3. test
 */
namespace backend::targetir {
class BitSet {
 public:
  explicit BitSet(size_t size) : bits_((size + 63) / 64, 0) {}
  BitSet() = default;

  // Set the bit at the given index
  auto Set(size_t index) -> void {
    auto word_index = index / 64;
    auto bit_index = index % 64;
    bits_[word_index] |= (1ULL << bit_index);
  }

  auto Reset(size_t index) -> void {
    auto word_index = index / 64;
    auto bit_index = index % 64;
    bits_[word_index] &= ~(1ULL << bit_index);
  }

  auto Test(size_t index) const -> bool {
    auto word_index = index / 64;
    auto bit_index = index % 64;
    return (bits_[word_index] & (1ULL << bit_index)) != 0;
  }

  // Define bitwise operations
  void operator|=(const BitSet &other) {
    for (size_t i = 0; i < bits_.size(); i++) {
      bits_[i] |= other.bits_[i];
    }
  }
  void operator&=(const BitSet &other) {
    for (size_t i = 0; i < bits_.size(); i++) {
      bits_[i] &= other.bits_[i];
    }
  }
  void operator-=(const BitSet &other) {
    for (size_t i = 0; i < bits_.size(); i++) {
      bits_[i] &= ~other.bits_[i];
    }
  }

  auto operator==(const BitSet &other) const -> bool { return bits_ == other.bits_; }

  auto operator!=(const BitSet &other) const -> bool { return bits_ != other.bits_; }

  // Get all set bits' indcies so that we can get the corresponding virtual registers
  auto GetSetBits() const -> std::vector<size_t> {
    std::vector<size_t> set_bits;
    for (size_t block = 0; block < bits_.size(); block++) {
      auto w = bits_[block];
      while (w != 0) {
        auto bit_index = __builtin_ctzll(w);
        set_bits.emplace_back(block * 64 + bit_index);
        w &= w - 1;  // Clear the least significant bit set
      }
    }
    return set_bits;
  }

 private:
  std::vector<uint64_t> bits_;
};

}  // namespace backend::targetir

#endif