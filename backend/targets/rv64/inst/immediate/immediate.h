#ifndef BACKEND_TARGETS_RV64_INST_IMMEDIATE_IMMEDIATE_H
#define BACKEND_TARGETS_RV64_INST_IMMEDIATE_IMMEDIATE_H

#include "backend/targets/utils/inst/targetinst.h"
namespace backend::targetir::rv64 {
class RVLUIInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

 private:
  RVLUIInst() = default;
  static auto Create(TargetOperand *rd, TargetOperand *imm) -> RVLUIInst *;
  friend class InstFactory;
};

class RVAUIPCInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

 private:
  RVAUIPCInst() = default;
  static auto Create(TargetOperand *rd, TargetOperand *imm) -> RVAUIPCInst *;
  friend class InstFactory;
};

}  // namespace backend::targetir::rv64

#endif