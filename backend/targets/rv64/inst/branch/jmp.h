#ifndef BACKEND_TARGETS_RV64_INST_BRANCH_JMP_H
#define BACKEND_TARGETS_RV64_INST_BRANCH_JMP_H

#include "backend/targets/utils/inst/targetinst.h"
namespace backend::targetir::rv64 {
class RVJMPInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

 private:
  RVJMPInst() = default;
  static auto Create(TargetOperand *target) -> RVJMPInst *;
  friend class InstFactory;
};

class RVJALInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

 private:
  RVJALInst() = default;
  static auto Create(TargetOperand *rd, TargetOperand *target) -> RVJALInst *;
  friend class InstFactory;
};

class RVJALRInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

 private:
  RVJALRInst() = default;
  static auto Create(TargetOperand *rd, TargetOperand *rs1, TargetOperand *imm) -> RVJALRInst *;
  friend class InstFactory;
};

#define RV64_BRANCH_DEFINE(InstName)                                                  \
  class InstName##Inst : public TargetInst {                                          \
   public:                                                                            \
    auto Emit(std::ofstream *out) -> void override;                                   \
                                                                                      \
   private:                                                                           \
    InstName##Inst() = default;                                                       \
    static auto Create(TargetOperand *rd, TargetOperand *target) -> InstName##Inst *; \
    friend class InstFactory;                                                         \
  };

RV64_BRANCH_DEFINE(RVBEQ)
RV64_BRANCH_DEFINE(RVBNE)
RV64_BRANCH_DEFINE(RVBLT)
RV64_BRANCH_DEFINE(RVBGE)

}  // namespace backend::targetir::rv64

#endif