#include "frontend/ast/value/value.h"
#include <cassert>

namespace frontend::ast {
auto Value::GetIntValue() const -> int {
  assert(type_ == ValueType::INT);
  return int_value_;
}

auto Value::GetFloatValue() const -> float {
  assert(type_ == ValueType::FLOAT);
  return float_value_;
}

auto Value::GetBoolValue() const -> bool {
  assert(type_ == ValueType::BOOL);
  return int_value_ != 0;
}

auto Value::SetIntValue(int int_value) -> void {
  int_value_ = int_value;
  type_ = ValueType::INT;
}

auto Value::SetFloatValue(float float_value) -> void {
  float_value_ = float_value;
  type_ = ValueType::FLOAT;
}

auto Value::SetBoolValue(bool bool_value) -> void {
  int_value_ = static_cast<int>(bool_value);
  type_ = ValueType::BOOL;
}

}  // namespace frontend::ast