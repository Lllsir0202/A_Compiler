#ifndef FRONTEND_AST_VALUE_VALUE_H
#define FRONTEND_AST_VALUE_VALUE_H

#include <cstdint>
#include <string>
namespace frontend::ast {
// Since we need to store the value of constant expressions
// We design this class to store the value.

/**
 * @brief ValueType enum to distinguish between different types of values
 * @note In ast phase, we only need to consider int, float and boolen.
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
  explicit Value(float float_value) : float_value_(float_value), type_(ValueType::FLOAT) {}
  explicit Value(bool bool_value) : int_value_(static_cast<int>(bool_value)), type_(ValueType::BOOL) {}

  auto GetType() const -> ValueType { return type_; }

  auto GetIntValue() const -> int;
  auto GetFloatValue() const -> float;
  auto GetBoolValue() const -> bool;
  auto SetIntValue(int int_value) -> void;
  auto SetFloatValue(float float_value) -> void;
  auto SetBoolValue(bool bool_value) -> void;

  auto ToString() const -> std::string;

 private:
  union {
    int int_value_;
    float float_value_;
  };
  enum ValueType type_;
};
}  // namespace frontend::ast

#endif