#ifndef UTILS_FLATTEN_INITIALIZER_H
#define UTILS_FLATTEN_INITIALIZER_H
#include <cstddef>
#include <optional>
#include <vector>

namespace initializer {

class Initializer {
 public:
  // Whether this initializer is a scalar value
  auto IsScalar() const -> bool { return init_list_.empty(); }

  /**
   * @method: SetIntValue(int)
   * @use: Set the value of this Initializer to an integer value
   * and clear other values in this Initializer
   */
  auto SetIntValue(int value) -> void;

  /**
   * @method: SetFloatValue(float)
   * @use: Set the value of this Initializer to a float value
   * and clear other values in this Initializer
   */
  auto SetFloatValue(float value) -> void;

  /**
   * @method: PushItem(Initializer)
   * @use: Push an item to the initializer list, used for array initializer
   */
  auto PushItem(const Initializer &item) -> void { init_list_.emplace_back(item); }

  /**
   * @method: GetIntValue()
   * @use: Get the integer value of this Initializer,
   * if it does not hold, return nullopt
   */
  auto GetIntValue() const -> int;

  /**
   * @method: GetFloatValue()
   * @use: Get the float value of this Initializer,
   * if it does not hold, return nullopt
   */
  auto GetFloatValue() const -> float;

 private:
  /**
   * @brief Use optional to represent the value
   * @note only used to represent constant value
   */
  std::optional<int> int_value_;
  std::optional<float> float_value_;

  /**
  TODO(loki): add a member variable to represent the IR stage variable
  */

  // init_list: used to represent array initializer
  std::vector<std::optional<Initializer>> init_list_;

  // dimensions of the array
  // used to calculate the offset when flattening the initializer list
  std::vector<size_t> dims_;
};

}  // namespace initializer

#endif