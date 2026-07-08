#ifndef BACKEND_TARGETS_RV64_SYMBOL_RV64GLOBAL_H
#define BACKEND_TARGETS_RV64_SYMBOL_RV64GLOBAL_H

#include "backend/targets/utils/struct/targetglobalobject.h"

namespace backend::targetir {
class RV64GlobalObject : public TargetGlobalObject {
 public:
  RV64GlobalObject(std::string name, type::Type *allocated_type, std::vector<int> dimensions,
                   std::vector<int> int_initial_values, bool is_constant)
      : TargetGlobalObject(std::move(name), allocated_type, std::move(dimensions), std::move(int_initial_values),
                           is_constant) {}

  RV64GlobalObject(std::string name, type::Type *allocated_type, std::vector<int> dimensions,
                   std::vector<float> float_initial_values, bool is_constant)
      : TargetGlobalObject(std::move(name), allocated_type, std::move(dimensions), std::move(float_initial_values),
                           is_constant) {}

  auto Emit(std::ofstream *out) const -> void override;

 private:
  auto GetAlignmentLog2() const -> size_t;
  auto EmitInitializer(std::ofstream *out) const -> void;
};

}  // namespace backend::targetir

#endif
