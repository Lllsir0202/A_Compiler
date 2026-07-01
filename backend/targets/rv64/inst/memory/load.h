#ifndef BACKEND_TARGETS_RV64_INST_MEMORY_LOAD_H
#define BACKEND_TARGETS_RV64_INST_MEMORY_LOAD_H

#include "backend/targets/utils/factory/instfactory.h"
#include "backend/targets/utils/inst/targetinst.h"
namespace backend::targetir::rv64 {
class RVLWInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

 private:
  RVLWInst() = default;
  static auto Create(TargetOperand *rd, TargetOperand *imm, TargetOperand *rs) -> RVLWInst *;
  friend class targetir::InstFactory;
};

class RVFLWInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

 private:
  RVFLWInst() = default;
  static auto Create(TargetOperand *rd, TargetOperand *imm, TargetOperand *rs) -> RVFLWInst *;
  friend class targetir::InstFactory;
};

class RVLDInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

 private:
  RVLDInst() = default;
  static auto Create(TargetOperand *rd, TargetOperand *imm, TargetOperand *rs) -> RVLDInst *;
  friend class targetir::InstFactory;
};

class RVFLDInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

 private:
  RVFLDInst() = default;
  static auto Create(TargetOperand *rd, TargetOperand *imm, TargetOperand *rs) -> RVFLDInst *;
  friend class targetir::InstFactory;
};

}  // namespace backend::targetir::rv64

#endif