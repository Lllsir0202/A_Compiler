#ifndef FRONTEND_AST_VALUE_VALUE_H
#define FRONTEND_AST_VALUE_VALUE_H

#include <cstdint>
#include <string>
namespace frontend::ast {
// Since we need to store the value of constant expressions
// We design this class to store the value.

/**
 * @brief ValueType enum to distinguish between different types of values
 * @note In ast phase, we only need to consider int, float and boolean.
 */
enum class ValueType : uint8_t { INT, FLOAT, BOOL, UNKNOWN };
/**
 * @brief Value class to store the value of constexpr
 * @method:
 */
class Value {
 public:
  Value() : int_value_(0), type_(ValueType::UNKNOWN){};
  explicit Value(int int_value) : int_value_(int_value), type_(ValueType::INT) {}
  explicit Value(double float_value) : float_value_(float_value), type_(ValueType::FLOAT) {}
  explicit Value(bool bool_value) : int_value_(static_cast<int>(bool_value)), type_(ValueType::BOOL) {}

  auto GetType() const -> ValueType { return type_; }

  auto GetIntValue() const -> int;
  auto GetFloatValue() const -> double;
  auto GetBoolValue() const -> bool;
  auto SetIntValue(int int_value) -> void;
  auto SetFloatValue(double float_value) -> void;
  auto SetBoolValue(bool bool_value) -> void;

  auto ToString() const -> std::string;

  // Override operators for basic arithmetic and logical operations
  auto operator+(const Value &other) const -> Value;
  auto operator-(const Value &other) const -> Value;
  auto operator*(const Value &other) const -> Value;
  auto operator/(const Value &other) const -> Value;
  auto operator%(const Value &other) const -> Value;
  auto operator-() const -> Value;                     // Unary minus
  auto operator+() const -> Value;                     // Unary plus
  auto operator!() const -> Value;                     // Logical NOT
  auto operator&&(const Value &other) const -> Value;  // Logical AND
  auto operator||(const Value &other) const -> Value;  // Logical OR
  auto operator==(const Value &other) const -> Value;  // Equality
  auto operator!=(const Value &other) const -> Value;  // Inequality
  auto operator<(const Value &other) const -> Value;   // Less than
  auto operator>(const Value &other) const -> Value;   // Greater than
  auto operator<=(const Value &other) const -> Value;  // Less than or equal
  auto operator>=(const Value &other) const -> Value;  // Greater than

 private:
  union {
    int int_value_;
    double float_value_;
  };
  enum ValueType type_;

  /**
   * @brief Helper function to convert
   */
  auto AsInt() const -> int;
  auto AsFloat() const -> double;
  auto AsBool() const -> bool;

  auto NeedFloat(const Value &other) const -> bool;
};
}  // namespace frontend::ast

#endif