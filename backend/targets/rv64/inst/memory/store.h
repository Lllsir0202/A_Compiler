#ifndef BACKEND_TARGETS_RV64_INST_MEMORY_STORE_H
#define BACKEND_TARGETS_RV64_INST_MEMORY_STORE_H

#include "backend/targets/utils/inst/targetinst.h"
namespace backend::targetir::rv64 {
class RVSWInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

 private:
  static auto Create(TargetOperand *rs2, TargetOperand *rs1, TargetOperand *imm) -> RVSWInst *;
  friend class InstFactory;
};

class RVFSWInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

 private:
  static auto Create(TargetOperand *rs2, TargetOperand *rs1, TargetOperand *imm) -> RVFSWInst *;
  friend class InstFactory;
};

}  // namespace backend::targetir::rv64

#endif