#ifndef MIDDLEEND_LLVM_INST_CMP_H
#define MIDDLEEND_LLVM_INST_CMP_H
#include "middleend/llvm/inst/inst.h"
namespace middleend::llvm::inst {
enum class CmpPredicate {
  EQ,  // equal
  NE,  // not equal
  LT,  // less than
  LE,  // less than or equal
  GT,  // greater than
  GE   // greater than or equal
};

class ICmpInst : public Instruction {
 public:
  auto Emit(std::ofstream *out) const -> void override;
  auto Verify(context::CompilerContext *context) const -> bool override;

 private:
  explicit ICmpInst(OpKind op_kind) : Instruction(op_kind) {}
  static auto Create(context::LLVMIRContext *context, CmpPredicate predicate, utils::Operand *operand1,
                     utils::Operand *operand2) -> ICmpInst *;
  friend class utils::factory::InstFactory;

  // Predicate type
  CmpPredicate predicate_;
};

class FCmpInst : public Instruction {
 public:
  auto Emit(std::ofstream *out) const -> void override;
  auto Verify(context::CompilerContext *context) const -> bool override;

 private:
  explicit FCmpInst(OpKind op_kind) : Instruction(op_kind) {}
  static auto Create(context::LLVMIRContext *context, CmpPredicate predicate, utils::Operand *operand1,
                     utils::Operand *operand2) -> FCmpInst *;
  friend class utils::factory::InstFactory;

  // Predicate type
  CmpPredicate predicate_;
};

}  // namespace middleend::llvm::inst

#endif