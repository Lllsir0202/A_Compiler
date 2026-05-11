#include "frontend/ast/value/value.h"
#include <cassert>
#include <stdexcept>

namespace frontend::ast {
auto Value::GetIntValue() const -> int {
  assert(type_ == ValueType::INT);
  return int_value_;
}

auto Value::GetFloatValue() const -> double {
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

auto Value::SetFloatValue(double float_value) -> void {
  float_value_ = float_value;
  type_ = ValueType::FLOAT;
}

auto Value::SetBoolValue(bool bool_value) -> void {
  int_value_ = static_cast<int>(bool_value);
  type_ = ValueType::BOOL;
}

auto Value::ToString() const -> std::string {
  switch (type_) {
    case ValueType::INT:
      return "Int " + std::to_string(int_value_);
    case ValueType::FLOAT:
      return "Float " + std::to_string(float_value_);
    case ValueType::BOOL:
      return int_value_ != 0 ? "Bool true" : "Bool false";
    default:
      return "UNKNOWN_VALUE";
  }
}

auto Value::AsInt() const -> int {
  if (type_ == ValueType::INT) {
    return int_value_;
  }
  if (type_ == ValueType::FLOAT) {
    return static_cast<int>(float_value_);
  }
  if (type_ == ValueType::BOOL) {
    return int_value_ != 0 ? 1 : 0;
  }
  throw std::runtime_error("Cannot convert UNKNOWN value to int");
}

auto Value::AsFloat() const -> double {
  if (type_ == ValueType::FLOAT) {
    return float_value_;
  }
  if (type_ == ValueType::INT) {
    return static_cast<double>(int_value_);
  }
  if (type_ == ValueType::BOOL) {
    return int_value_ != 0 ? 1.0 : 0.0;
  }
  throw std::runtime_error("Cannot convert UNKNOWN value to float");
}

auto Value::AsBool() const -> bool {
  if (type_ == ValueType::BOOL) {
    return int_value_ != 0;
  }
  if (type_ == ValueType::INT) {
    return int_value_ != 0;
  }
  if (type_ == ValueType::FLOAT) {
    return float_value_ != 0.0;
  }
  throw std::runtime_error("Cannot convert UNKNOWN value to bool");
}

// Return true if either this or other is a float,
//  indicating that the result of an operation between them should be a float
auto Value::NeedFloat(const Value &other) const -> bool {
  return type_ == ValueType::FLOAT || other.type_ == ValueType::FLOAT;
}

// override operators for basic arithmetic and logical operations

/**
 * @brief Arithemtic Operators
 */
auto Value::operator+(const Value &other) const -> Value {
  if (NeedFloat(other)) {
    return Value(this->AsFloat() + other.AsFloat());
  }
  // There is no float involved
  // Since bool and int will always be a int result
  // We do not need to consider bool separately
  return Value(this->AsInt() + other.AsInt());
}

auto Value::operator-(const Value &other) const -> Value {
  if (NeedFloat(other)) {
    return Value(this->AsFloat() - other.AsFloat());
  }
  return Value(this->AsInt() - other.AsInt());
}

auto Value::operator*(const Value &other) const -> Value {
  if (NeedFloat(other)) {
    return Value(this->AsFloat() * other.AsFloat());
  }
  return Value(this->AsInt() * other.AsInt());
}

auto Value::operator/(const Value &other) const -> Value {
  // Need to check if other is zero
  if (NeedFloat(other)) {
    double other_float = other.AsFloat();
    if (other_float == 0.0) {
      throw std::runtime_error("Division by zero");
    }
    return Value(this->AsFloat() / other_float);
  }
  // Even though the divided result maybe a float,
  // We still return an int here
  int other_int = other.AsInt();
  if (other_int == 0) {
    throw std::runtime_error("Division by zero");
  }
  return Value(this->AsInt() / other_int);
}

auto Value::operator%(const Value &other) const -> Value {
  // It is not supported for floats
  if (NeedFloat(other)) {
    throw std::runtime_error("Modulo operator is not defined for float values");
  }
  int other_int = other.AsInt();
  if (other_int == 0) {
    throw std::runtime_error("Modulo by zero");
  }
  return Value(this->AsInt() % other_int);
}

// Unary operators
auto Value::operator-() const -> Value {
  if (type_ == ValueType::FLOAT) {
    return Value(-float_value_);
  }
  return Value(-int_value_);
}

auto Value::operator+() const -> Value {
  // Unary plus does not change the value, but we still need to consider float and int
  if (type_ == ValueType::FLOAT) {
    return Value(float_value_);
  }
  return Value(int_value_);
}

auto Value::operator!() const -> Value { return Value(!this->AsBool()); }

// Logical operators
auto Value::operator&&(const Value &other) const -> Value { return Value(this->AsBool() && other.AsBool()); }

auto Value::operator||(const Value &other) const -> Value { return Value(this->AsBool() || other.AsBool()); }

auto Value::operator==(const Value &other) const -> Value {
  if (NeedFloat(other)) {
    return Value(this->AsFloat() == other.AsFloat());
  }
  return Value(this->AsInt() == other.AsInt());
}

auto Value::operator!=(const Value &other) const -> Value {
  if (NeedFloat(other)) {
    return Value(this->AsFloat() != other.AsFloat());
  }
  return Value(this->AsInt() != other.AsInt());
}

auto Value::operator<(const Value &other) const -> Value {
  if (NeedFloat(other)) {
    return Value(this->AsFloat() < other.AsFloat());
  }
  return Value(this->AsInt() < other.AsInt());
}

auto Value::operator>(const Value &other) const -> Value {
  if (NeedFloat(other)) {
    return Value(this->AsFloat() > other.AsFloat());
  }
  return Value(this->AsInt() > other.AsInt());
}

auto Value::operator<=(const Value &other) const -> Value {
  if (NeedFloat(other)) {
    return Value(this->AsFloat() <= other.AsFloat());
  }
  return Value(this->AsInt() <= other.AsInt());
}

auto Value::operator>=(const Value &other) const -> Value {
  if (NeedFloat(other)) {
    return Value(this->AsFloat() >= other.AsFloat());
  }
  return Value(this->AsInt() >= other.AsInt());
}

}  // namespace frontend::ast