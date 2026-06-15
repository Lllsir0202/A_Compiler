#ifndef BACKEND_TARGETS_AARCH64_MEMORY_LOAD_H
#define BACKEND_TARGETS_AARCH64_MEMORY_LOAD_H

#include "backend/targets/utils/inst/targetinst.h"
namespace backend::targetir::aarch64 {

// Load a register from memory
// Two types, one is register offset, the other is immediate offset
class ARMLDRWInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

 private:
  ARMLDRWInst() = default;
  static auto Create(TargetOperand *rd, TargetOperand *rs1, TargetOperand *rs2) -> ARMLDRWInst *;
  friend class InstFactory;
};

class ARMLDRWimmInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

 private:
  ARMLDRWimmInst() = default;
  static auto Create(TargetOperand *rd, TargetOperand *rs1, TargetOperand *imm) -> ARMLDRWimmInst *;
  friend class InstFactory;
};

}  // namespace backend::targetir::aarch64

#endif