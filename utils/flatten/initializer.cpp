#include "utils/flatten/initializer.h"
#include <stdexcept>

namespace initializer {
auto Initializer::SetIntValue(int value) -> void {
  int_value_ = value;
  float_value_.reset();
  init_list_.clear();
  dims_.clear();
}

auto Initializer::SetFloatValue(float value) -> void {
  float_value_ = value;
  int_value_.reset();
  init_list_.clear();
  dims_.clear();
}

auto Initializer::GetIntValue() const -> int {
  if (!int_value_.has_value()) {
    throw std::logic_error("Initializer does not hold an integer");
  }
  return int_value_.value();
}

auto Initializer::GetFloatValue() const -> float {
  if (!float_value_.has_value()) {
    throw std::logic_error("Initializer does not hold a float");
  }
  return float_value_.value();
}

}  // namespace initializer