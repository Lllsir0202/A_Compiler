#ifndef BACKEND_MIR_INST_BRANCH_BRANCH_H
#define BACKEND_MIR_INST_BRANCH_BRANCH_H

#include "backend/mir/inst/inst.h"
#include "backend/mir/utils/operand/labeloperand.h"

namespace backend::inst {
class BrInst : public Instruction {
 public:
  auto Verify(context::CompilerContext *context) const -> bool override;
  auto Emit(std::ofstream *out) const -> void override;
  auto IsBranch() const -> bool override { return true; }
  auto IsTerminal() const -> bool override { return true; }

 private:
  explicit BrInst() : Instruction(OpKind::Br) {}
  static auto Create(context::MIRContext *context, utils::LabelOperand *label) -> BrInst *;
  friend class utils::factory::InstFactory;
};

class BrCondInst : public Instruction {
 public:
  auto Verify(context::CompilerContext *context) const -> bool override;
  auto Emit(std::ofstream *out) const -> void override;
  auto IsBranch() const -> bool override { return true; }
  auto IsTerminal() const -> bool override { return true; }

 private:
  explicit BrCondInst() : Instruction(OpKind::CondBr) {}
  static auto Create(context::MIRContext *context, utils::Operand *condition, utils::LabelOperand *true_label,
                     utils::LabelOperand *false_label) -> BrCondInst *;
  friend class utils::factory::InstFactory;
};

}  // namespace backend::inst

#endif