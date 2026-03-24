#ifndef UTILS_FLATTEN_ASTINIT_H
#define UTILS_FLATTEN_ASTINIT_H
#include <optional>
#include <vector>
#include "utils/flatten/initializer.h"

namespace initializer {

/**
 * @brief This class represents a scalar constant initializer
 * @method: SetIntValue: set the integer value of the initializer
 * @method: SetFloatValue: set the float value of the initializer
 * @method: GetIntValue: get the integer value of the initializer,
 * if the initializer is not an integer, return std::nullopt
 * @method: GetFloatValue: get the float value of the initializer,
 * if the initializer is not a float, return std::nullopt
 *
 * @note: This class is used for representing the constant scalar initializer
 * while in the phase of semantic analysis
 */
class ConstInitializer : public Initializer {
 public:
  ConstInitializer() = default;
  auto SetIntValue(int value) -> void;
  auto SetFloatValue(float value) -> void;
  auto GetIntValue() const -> std::optional<int>;
  auto GetFloatValue() const -> std::optional<float>;
  auto IsScalar() const -> bool override { return true; }
  auto IsConst() const -> bool override { return true; }

 private:
  std::optional<int> int_value_;
  std::optional<float> float_value_;
};

auto HandleInAst(frontend::ast::Initializer *init_node) -> InitializerPtr;

auto CheckInAst(frontend::ast::Initializer *init_node) -> InitializerPtr;

}  // namespace initializer

#endif