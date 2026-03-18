#ifndef UTILS_FLATTEN_CONSTINIT_H
#define UTILS_FLATTEN_CONSTINIT_H
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
 */
class ConstInitializer : public Initializer {
 public:
  ConstInitializer() = default;
  auto SetIntValue(int value) -> void;
  auto SetFloatValue(float value) -> void;
  auto GetIntValue() const -> std::optional<int>;
  auto GetFloatValue() const -> std::optional<float>;
  auto IsScalar() const -> bool override { return true; }

 private:
  std::optional<int> int_value_;
  std::optional<float> float_value_;
};

/**
 * @brief This class represents a constant array initializer
 */
class ConstArrayInitializer : public Initializer {
 public:
  auto AddInitializer(InitializerPtr init) -> void;
  auto GetInitializers() const -> const std::vector<InitializerPtr> & { return init_values_; }
  auto IsScalar() const -> bool override { return false; }

 private:
  std::vector<InitializerPtr> init_values_;
};

}  // namespace initializer

#endif