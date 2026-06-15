#ifndef BACKEND_TARGETS_RV64_INST_PSEUDO_PSEUDO_H
#define BACKEND_TARGETS_RV64_INST_PSEUDO_PSEUDO_H

#include "backend/targets/utils/inst/targetinst.h"
namespace backend::targetir::rv64 {
class RVLIInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

 private:
  RVLIInst() = default;
  static auto Create(TargetOperand *rd, TargetOperand *imm) -> RVLIInst *;
  friend class InstFactory;
};

class RVMVInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

 private:
  RVMVInst() = default;
  static auto Create(TargetOperand *rd, TargetOperand *rs1, TargetOperand *rs2) -> RVMVInst *;
  friend class InstFactory;
};

}  // namespace backend::targetir::rv64

#endif