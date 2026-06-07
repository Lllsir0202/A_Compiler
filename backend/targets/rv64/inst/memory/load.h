#ifndef BACKEND_TARGETS_RV64_INST_MEMORY_LOAD_H
#define BACKEND_TARGETS_RV64_INST_MEMORY_LOAD_H

#include "backend/targets/utils/inst/targetinst.h"
namespace backend::targetir::rv64 {
class RVLWInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

 private:
  static auto Create(TargetOperand *rd, TargetOperand *rs1, TargetOperand *imm) -> RVLWInst *;
  friend class InstFactory;
};

class RVFLWInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

 private:
  static auto Create(TargetOperand *rd, TargetOperand *rs1, TargetOperand *imm) -> RVFLWInst *;
  friend class InstFactory;
};

class RVLDInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

 private:
  static auto Create(TargetOperand *rd, TargetOperand *rs1, TargetOperand *imm) -> RVLDInst *;
  friend class InstFactory;
};

class RVFLDInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

 private:
  static auto Create(TargetOperand *rd, TargetOperand *rs1, TargetOperand *imm) -> RVFLDInst *;
  friend class InstFactory;
};

}  // namespace backend::targetir::rv64

#endif