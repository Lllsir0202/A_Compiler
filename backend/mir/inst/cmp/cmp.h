#ifndef BACKEND_MIR_INST_CMP_CMP_H
#define BACKEND_MIR_INST_CMP_CMP_H

#include "backend/mir/inst/inst.h"
#include "mir_context.h"
namespace backend::inst {
enum class CmpPredicate : uint8_t {
  EQ,  // equal
  NE,  // not equal
  LT,  // less than
  LE,  // less than or equal
  GT,  // greater than
  GE   // greater than or equal
};

class ICmpInst : public Instruction {
 public:
  auto Verify(context::CompilerContext *context) const -> bool override;

 private:
  explicit ICmpInst(CmpPredicate predicate) : Instruction(OpKind::ICmp), predicate_(predicate) {}
  static auto Create(context::MIRContext *context, utils::Operand *lhs, utils::Operand *rhs,
                     CmpPredicate predicate) -> ICmpInst *;
  CmpPredicate predicate_;
  friend class utils::factory::InstFactory;
};

class FCmpInst : public Instruction {
 public:
  auto Verify(context::CompilerContext *context) const -> bool override;

 private:
  explicit FCmpInst(CmpPredicate predicate) : Instruction(OpKind::FCmp), predicate_(predicate) {}
  static auto Create(context::MIRContext *context, utils::Operand *lhs, utils::Operand *rhs,
                     CmpPredicate predicate) -> FCmpInst *;
  CmpPredicate predicate_;
  friend class utils::factory::InstFactory;
};

}  // namespace backend::inst

#endif