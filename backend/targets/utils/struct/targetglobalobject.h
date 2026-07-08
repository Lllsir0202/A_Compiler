#ifndef BACKEND_TARGETS_UTILS_STRUCT_TARGETGLOBALOBJECT_H
#define BACKEND_TARGETS_UTILS_STRUCT_TARGETGLOBALOBJECT_H

#include <fstream>
#include <string>
#include <vector>
#include "utils/type/type.h"

namespace backend::targetir {
class TargetGlobalObject {
 public:
  virtual ~TargetGlobalObject() = default;
  TargetGlobalObject(std::string name, type::Type *allocated_type, std::vector<int> dimensions,
                     std::vector<int> int_initial_values, bool is_constant);
  TargetGlobalObject(std::string name, type::Type *allocated_type, std::vector<int> dimensions,
                     std::vector<float> float_initial_values, bool is_constant);

  virtual auto Emit(std::ofstream *out) const -> void = 0;

 protected:
  auto GetName() const -> const std::string & { return name_; }
  auto GetAllocatedType() const -> type::Type * { return allocated_type_; }
  auto GetDimensions() const -> const std::vector<int> & { return dimensions_; }
  auto GetIntInitialValues() const -> const std::vector<int> & { return int_initial_values_; }
  auto GetFloatInitialValues() const -> const std::vector<float> & { return float_initial_values_; }
  auto IsConstant() const -> bool { return is_constant_; }

  auto GetSize() const -> size_t;
  auto GetAlignment() const -> size_t;
  auto IsZeroInitialized() const -> bool;

 private:
  std::string name_;
  type::Type *allocated_type_;
  std::vector<int> dimensions_;
  std::vector<int> int_initial_values_;
  std::vector<float> float_initial_values_;
  bool is_constant_;
};

}  // namespace backend::targetir

#endif
