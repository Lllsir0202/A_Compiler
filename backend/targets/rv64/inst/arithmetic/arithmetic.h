#ifndef BACKEND_TARGETS_RV64_INST_ARITHMETIC_ARITHMETIC_H
#define BACKEND_TARGETS_RV64_INST_ARITHMETIC_ARITHMETIC_H

/**
 * @brief This file defines the arithmetic instructions for the RV64 target.
 * currently includes:
 * - ADD, SUB, MUL, DIV, REM
 * - ADDI, ADDIW
 * - ADDW, SUBW, MULW, DIVW, REMW
 * - SLLI, SLLIW
 * - XOR, XORI
 */

#include "backend/targets/utils/factory/instfactory.h"
#include "backend/targets/utils/inst/targetinst.h"
namespace backend::targetir::rv64 {

#define RV64_ARITHMETIC_DEFINE(InstName)                                                               \
  class InstName##Inst : public TargetInst {                                                           \
   public:                                                                                             \
    auto Emit(std::ofstream *out) -> void override;                                                    \
                                                                                                       \
   private:                                                                                            \
    InstName##Inst() = default;                                                                        \
    static auto Create(TargetOperand *rd, TargetOperand *rs1, TargetOperand *rs2) -> InstName##Inst *; \
    friend class targetir::InstFactory;                                                                \
  };

RV64_ARITHMETIC_DEFINE(RVADD)
RV64_ARITHMETIC_DEFINE(RVSUB)
RV64_ARITHMETIC_DEFINE(RVMUL)
RV64_ARITHMETIC_DEFINE(RVDIV)
RV64_ARITHMETIC_DEFINE(RVREM)
RV64_ARITHMETIC_DEFINE(RVADDW)
RV64_ARITHMETIC_DEFINE(RVSUBW)
RV64_ARITHMETIC_DEFINE(RVMULW)
RV64_ARITHMETIC_DEFINE(RVDIVW)
RV64_ARITHMETIC_DEFINE(RVREMW)

// Xor instruction
RV64_ARITHMETIC_DEFINE(RVXOR)

class RVXORIInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

 private:
  RVXORIInst() = default;
  static auto Create(TargetOperand *rd, TargetOperand *rs1, TargetOperand *imm) -> RVXORIInst *;
  friend class targetir::InstFactory;
  ;
};

class RVADDIInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

 private:
  RVADDIInst() = default;
  static auto Create(TargetOperand *rd, TargetOperand *rs1, TargetOperand *imm) -> RVADDIInst *;
  friend class targetir::InstFactory;
  ;
};

class RVADDIWInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

 private:
  RVADDIWInst() = default;
  static auto Create(TargetOperand *rd, TargetOperand *rs1, TargetOperand *imm) -> RVADDIWInst *;
  friend class targetir::InstFactory;
  ;
};

class RVSLLIInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

 private:
  RVSLLIInst() = default;
  static auto Create(TargetOperand *rd, TargetOperand *rs1, TargetOperand *imm) -> RVSLLIInst *;
  friend class targetir::InstFactory;
  ;
};

class RVSLLIWInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

 private:
  RVSLLIWInst() = default;
  static auto Create(TargetOperand *rd, TargetOperand *rs1, TargetOperand *imm) -> RVSLLIWInst *;
  friend class targetir::InstFactory;
  ;
};

class RVSRLIInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

 private:
  RVSRLIInst() = default;
  static auto Create(TargetOperand *rd, TargetOperand *rs1, TargetOperand *imm) -> RVSRLIInst *;
  friend class targetir::InstFactory;
  ;
};

class RVSRAIInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

 private:
  RVSRAIInst() = default;
  static auto Create(TargetOperand *rd, TargetOperand *rs1, TargetOperand *imm) -> RVSRAIInst *;
  friend class targetir::InstFactory;
  ;
};

class RVSRLIWInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

 private:
  RVSRLIWInst() = default;
  static auto Create(TargetOperand *rd, TargetOperand *rs1, TargetOperand *imm) -> RVSRLIWInst *;
  friend class targetir::InstFactory;
  ;
};

class RVSRAIWInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

 private:
  RVSRAIWInst() = default;
  static auto Create(TargetOperand *rd, TargetOperand *rs1, TargetOperand *imm) -> RVSRAIWInst *;
  friend class targetir::InstFactory;
  ;
};

// Float arithmetic instructions
RV64_ARITHMETIC_DEFINE(RVFADDS)
RV64_ARITHMETIC_DEFINE(RVFSUBS)
RV64_ARITHMETIC_DEFINE(RVFMULS)
RV64_ARITHMETIC_DEFINE(RVFDIVS)

#undef RV64_ARITHMETIC_DEFINE
}  // namespace backend::targetir::rv64

#endif
