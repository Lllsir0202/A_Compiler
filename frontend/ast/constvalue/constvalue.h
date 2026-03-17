#ifndef FRONTEND_AST_CONSTVALUE_CONSTVALUE_H
#define FRONTEND_AST_CONSTVALUE_CONSTVALUE_H

#include <cstdint>
#include <stdexcept>
namespace frontend::ast {
/**
 * @brief The ConstValue class is aiming to represent the initializer values
 * in constant variable declarations, which can be either a integer or a float.
 *
 */
class ConstValue {
 public:
  explicit ConstValue() : int_value_(0) {}
  /**
   * @method: SetIntValue(int)
   * @use: Set the value of this ConstValue to an integer value, and set its type to INT
   */
  auto SetIntValue(int value) -> void {
    int_value_ = value;
    type_ = ValueType::INT;
  }

  /**
   * @method: SetFloatValue(float)
   * @use: Set the value of this ConstValue to a float value, and set its type to FLOAT
   */
  auto SetFloatValue(float value) -> void {
    float_value_ = value;
    type_ = ValueType::FLOAT;
  }

  /**
   * @method: GetIntValue()
   * @use: Get the integer value of this ConstValue, if its type is not
   */
  auto GetIntValue() const -> int {
    if (type_ != ValueType::INT) {
      throw std::logic_error("ConstValue does not hold an integer");
    }
    return int_value_;
  }

  /**
   * @method: GetFloatValue()
   * @use: Get the float value of this ConstValue, if its type is not FLOAT, it will throw a logic error
   */
  auto GetFloatValue() const -> float {
    if (type_ != ValueType::FLOAT) {
      throw std::logic_error("ConstValue does not hold a float");
    }
    return float_value_;
  }

 private:
  // Used in storing the value
  union {
    int int_value_;
    float float_value_;
  };
  // Used to indicate the type of the value
  enum class ValueType : uint8_t { INT, FLOAT, UNKNOWN } type_{ValueType::UNKNOWN};
};

}  // namespace frontend::ast

#endif