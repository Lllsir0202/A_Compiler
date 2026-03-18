#include "utils/flatten/constinit.h"

namespace initializer {
auto ConstInitializer::SetIntValue(int value) -> void {
  int_value_ = value;
  float_value_ = std::nullopt;
}

auto ConstInitializer::SetFloatValue(float value) -> void {
  float_value_ = value;
  int_value_ = std::nullopt;
}

/**
* @brief Even though we do not check
* whether the initializer is the specific type, it will still be nullopt,
* since we will only set one type of value in the initializer, so we can just return the value directly without checking
the type.
*/
auto ConstInitializer::GetIntValue() const -> std::optional<int> { return int_value_; }

auto ConstInitializer::GetFloatValue() const -> std::optional<float> { return float_value_; }

}  // namespace initializer