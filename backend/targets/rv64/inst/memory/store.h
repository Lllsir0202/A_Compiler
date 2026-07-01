#ifndef BACKEND_TARGETS_RV64_INST_MEMORY_STORE_H
#define BACKEND_TARGETS_RV64_INST_MEMORY_STORE_H

#include "backend/targets/utils/factory/instfactory.h"
#include "backend/targets/utils/inst/targetinst.h"
namespace backend::targetir::rv64 {
class RVSWInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

 private:
  RVSWInst() = default;
  static auto Create(TargetOperand *value, TargetOperand *imm, TargetOperand *rs) -> RVSWInst *;
  friend class targetir::InstFactory;
};

class RVFSWInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

 private:
  RVFSWInst() = default;
  static auto Create(TargetOperand *value, TargetOperand *imm, TargetOperand *rs) -> RVFSWInst *;
  friend class targetir::InstFactory;
};

class RVSDInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

 private:
  RVSDInst() = default;
  static auto Create(TargetOperand *value, TargetOperand *imm, TargetOperand *rs) -> RVSDInst *;
  friend class targetir::InstFactory;
};

class RVFSDInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

 private:
  RVFSDInst() = default;
  static auto Create(TargetOperand *value, TargetOperand *imm, TargetOperand *rs) -> RVFSDInst *;
  friend class targetir::InstFactory;
};

}  // namespace backend::targetir::rv64

#endif