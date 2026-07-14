#ifndef BACKEND_TARGETS_RISCV_PASS_ADDRFOLD_H
#define BACKEND_TARGETS_RISCV_PASS_ADDRFOLD_H

#include <string>
#include <unordered_map>
#include "backend/targets/utils/pass/machinepass.h"

namespace backend::targetir {
class TargetFunction;
class TargetInst;
class TargetOperand;
}  // namespace backend::targetir

namespace backend::targetir::riscv {

/** Fold `addi address, base, immediate` into RV64 load/store offsets. */
class RISCVAddrFold : public MachineFunctionPass {
 public:
  explicit RISCVAddrFold(TargetContext *ctx) : MachineFunctionPass(ctx) {}

  auto Name() const -> std::string override { return "riscv-addr-fold"; }
  auto RunOnFunction(TargetFunction *func) -> bool override;

 private:
  auto BuildDefs(TargetFunction *func) -> void;
  auto SingleDef(TargetOperand *operand) const -> TargetInst *;

  std::unordered_map<TargetOperand *, TargetInst *> defs_;
  std::unordered_map<TargetOperand *, int> def_count_;
};

}  // namespace backend::targetir::riscv

#endif
