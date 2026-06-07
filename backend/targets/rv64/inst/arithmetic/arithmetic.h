#ifndef BACKEND_TARGETS_RV64_INST_ARITHMETIC_ARITHMETIC_H
#define BACKEND_TARGETS_RV64_INST_ARITHMETIC_ARITHMETIC_H

/**
 * @brief This file defines the arithmetic instructions for the RV64 target.
 * currently includes:
 * - ADD, SUB, MUL, DIV, REM
 * - ADDI
 * - ADDW
 */

#include "backend/targets/utils/inst/targetinst.h"
namespace backend::targetir::rv64 {

#define RV64_ARITHMETIC_DEFINE(InstName)                                                               \
  class InstName##Inst : public TargetInst {                                                           \
   public:                                                                                             \
    auto Emit(std::ofstream *out) -> void override;                                                    \
                                                                                                       \
   private:                                                                                            \
    static auto Create(TargetOperand *rd, TargetOperand *rs1, TargetOperand *rs2) -> InstName##Inst *; \
    friend class InstFactory;                                                                          \
  };

RV64_ARITHMETIC_DEFINE(RVADD)
RV64_ARITHMETIC_DEFINE(RVSUB)
RV64_ARITHMETIC_DEFINE(RVMUL)
RV64_ARITHMETIC_DEFINE(RVDIV)
RV64_ARITHMETIC_DEFINE(RVREM)
RV64_ARITHMETIC_DEFINE(RVADDW)

class RVADDIInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

 private:
  static auto Create(TargetOperand *rd, TargetOperand *rs1, TargetOperand *imm) -> RVADDIInst *;
  friend class InstFactory;
};

// Float arithmetic instructions
RV64_ARITHMETIC_DEFINE(RVFADDS)
RV64_ARITHMETIC_DEFINE(RVFSUBS)
RV64_ARITHMETIC_DEFINE(RVFMULS)
RV64_ARITHMETIC_DEFINE(RVFDIVS)

}  // namespace backend::targetir::rv64

#endif