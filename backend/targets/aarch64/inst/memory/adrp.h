#ifndef BACKEND_TARGETS_AARCH64_INST_MEMORY_ADRP_H
#define BACKEND_TARGETS_AARCH64_INST_MEMORY_ADRP_H

#include "targets/utils/inst/targetinst.h"
namespace backend::targetir::aarch64 {
class ARMAdrpInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

 private:
  static auto Create(TargetOperand *dst, TargetOperand *label) -> ARMAdrpInst *;
  friend class InstFatory;
};

}  // namespace backend::targetir::aarch64

#endif