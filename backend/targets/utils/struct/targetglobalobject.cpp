#include "backend/targets/utils/struct/targetglobalobject.h"
#include <algorithm>

namespace backend::targetir {
TargetGlobalObject::TargetGlobalObject(std::string name, type::Type *allocated_type, std::vector<int> dimensions,
                                       std::vector<int> int_initial_values, bool is_constant)
    : name_(std::move(name)),
      allocated_type_(allocated_type),
      dimensions_(std::move(dimensions)),
      int_initial_values_(std::move(int_initial_values)),
      is_constant_(is_constant) {}

TargetGlobalObject::TargetGlobalObject(std::string name, type::Type *allocated_type, std::vector<int> dimensions,
                                       std::vector<float> float_initial_values, bool is_constant)
    : name_(std::move(name)),
      allocated_type_(allocated_type),
      dimensions_(std::move(dimensions)),
      float_initial_values_(std::move(float_initial_values)),
      is_constant_(is_constant) {}

auto TargetGlobalObject::GetSize() const -> size_t {
  size_t count = 1;
  for (auto dim : dimensions_) {
    count *= static_cast<size_t>(dim);
  }
  return count * allocated_type_->GetDataSize();
}

auto TargetGlobalObject::GetAlignment() const -> size_t { return allocated_type_->GetAlign(); }

auto TargetGlobalObject::IsZeroInitialized() const -> bool {
  if (int_initial_values_.empty() && float_initial_values_.empty()) {
    return true;
  }
  if (!int_initial_values_.empty()) {
    return std::all_of(int_initial_values_.begin(), int_initial_values_.end(), [](int value) { return value == 0; });
  }
  if (!float_initial_values_.empty()) {
    return std::all_of(float_initial_values_.begin(), float_initial_values_.end(),
                       [](float value) { return value == 0.0F; });
  }
  // Should not reach here, as either int_initial_values_ or float_initial_values_ should be empty
  return false;
}

}  // namespace backend::targetir
