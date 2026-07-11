#ifndef BACKEND_TARGETS_RISCV_REGS_RISCVREGINFO_H
#define BACKEND_TARGETS_RISCV_REGS_RISCVREGINFO_H

#include <vector>
#include "backend/targets/riscv/regs/riscvreg.h"
#include "backend/targets/utils/regalloc/registerinfo.h"

namespace backend::targetir::riscv {

/**
 * @brief RISC-V (rv64, lp64d) implementation of the RegisterInfo contract that
 * drives the target-independent graph-coloring allocator. Precomputes the
 * allocatable and callee-saved id lists per class once at construction.
 */
class RISCVRegisterInfo : public RegisterInfo {
 public:
  RISCVRegisterInfo() {
    for (auto r : ALLOCATABLE_GPR) {
      gpr_alloc_.push_back(static_cast<uint8_t>(r));
    }
    for (auto r : ALLOCATABLE_FPR) {
      fpr_alloc_.push_back(static_cast<uint8_t>(r));
    }
    for (auto id : gpr_alloc_) {
      if (!IsGPRCallerSaved(id)) {
        gpr_callee_.push_back(id);
      }
    }
    for (auto id : fpr_alloc_) {
      if (!IsFPRCallerSaved(id)) {
        fpr_callee_.push_back(id);
      }
    }
  }

  auto AllocatableRegs(TargetRegClass reg_class) const -> const std::vector<uint8_t> & override {
    return reg_class == TargetRegClass::Float ? fpr_alloc_ : gpr_alloc_;
  }

  auto NumAllocatable(TargetRegClass reg_class) const -> size_t override {
    return AllocatableRegs(reg_class).size();
  }

  auto IsCallerSaved(TargetRegClass reg_class, uint8_t id) const -> bool override {
    return reg_class == TargetRegClass::Float ? IsFPRCallerSaved(id) : IsGPRCallerSaved(id);
  }

  auto CalleeSavedRegs(TargetRegClass reg_class) const -> const std::vector<uint8_t> & override {
    return reg_class == TargetRegClass::Float ? fpr_callee_ : gpr_callee_;
  }

 private:
  std::vector<uint8_t> gpr_alloc_;
  std::vector<uint8_t> fpr_alloc_;
  std::vector<uint8_t> gpr_callee_;
  std::vector<uint8_t> fpr_callee_;
};

}  // namespace backend::targetir::riscv

#endif
