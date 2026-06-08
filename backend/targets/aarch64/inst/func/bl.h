#ifndef BACKEND_TARGETS_AARCH64_INST_FUNC_BL_H
#define BACKEND_TARGETS_AARCH64_INST_FUNC_BL_H

#include "backend/targets/utils/inst/targetinst.h"
namespace backend::targetir::aarch64 {
class ARMBLInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

 private:
  static auto Create(TargetOperand *label) -> ARMBLInst *;
  friend class InstFactory;
};

}  // namespace backend::targetir::aarch64

#endif