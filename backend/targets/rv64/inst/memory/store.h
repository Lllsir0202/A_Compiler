#ifndef BACKEND_TARGETS_RV64_INST_MEMORY_STORE_H
#define BACKEND_TARGETS_RV64_INST_MEMORY_STORE_H

#include "backend/targets/utils/inst/targetinst.h"
namespace backend::targetir::rv64 {
class RVSWInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

 private:
  RVSWInst() = default;
  static auto Create(TargetOperand *rs2, TargetOperand *rs1, TargetOperand *imm) -> RVSWInst *;
  friend class InstFactory;
};

class RVFSWInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

 private:
  RVFSWInst() = default;
  static auto Create(TargetOperand *rs2, TargetOperand *rs1, TargetOperand *imm) -> RVFSWInst *;
  friend class InstFactory;
};

class RVSDInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

 private:
  RVSDInst() = default;
  static auto Create(TargetOperand *rs2, TargetOperand *rs1, TargetOperand *imm) -> RVSDInst *;
  friend class InstFactory;
};

class RVFSDInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

 private:
  RVFSDInst() = default;
  static auto Create(TargetOperand *rs2, TargetOperand *rs1, TargetOperand *imm) -> RVFSDInst *;
  friend class InstFactory;
};

}  // namespace backend::targetir::rv64

#endif