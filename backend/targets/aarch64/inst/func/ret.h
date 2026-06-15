#ifndef BACKEND_TARGETS_AARCH64_INST_FUNC_RET_H
#define BACKEND_TARGETS_AARCH64_INST_FUNC_RET_H

#include "backend/targets/utils/inst/targetinst.h"
#include "targets/utils/operand/targetoperand.h"
namespace backend::targetir::aarch64 {

// Ret instruction
/**
 * @brief In armv8,
 * to be honest, it can be used as ret <reg> to set return value,
 * but to keep it the same as bl(bl will set the return value in x0),
 * we just use ret without operand.
 */
class ARMRetInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

 private:
  ARMRetInst() = default;
  static auto Create(TargetOperand *rd) -> ARMRetInst *;
  friend class InstFactory;
};

}  // namespace backend::targetir::aarch64

#endif