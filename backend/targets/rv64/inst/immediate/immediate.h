#ifndef BACKEND_TARGETS_RV64_INST_IMMEDIATE_IMMEDIATE_H
#define BACKEND_TARGETS_RV64_INST_IMMEDIATE_IMMEDIATE_H

#include "backend/targets/utils/factory/instfactory.h"
#include "backend/targets/utils/inst/targetinst.h"
namespace backend::targetir::rv64 {
class RVLUIInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

 private:
  RVLUIInst() = default;
  static auto Create(TargetOperand *rd, TargetOperand *imm) -> RVLUIInst *;
  friend class targetir::InstFactory;
};

// Load a symbol's address into a register, which is used for PC-related address calculation.
class RVAUIPCInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

 private:
  RVAUIPCInst() = default;
  static auto Create(TargetOperand *rd, TargetOperand *symbol) -> RVAUIPCInst *;
  friend class targetir::InstFactory;
};

}  // namespace backend::targetir::rv64

#endif
