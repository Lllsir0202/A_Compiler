#ifndef BACKEND_MIR_INST_PSEUDO_PHI_H
#define BACKEND_MIR_INST_PSEUDO_PHI_H

#include "backend/mir/inst/inst.h"
#include "mir/utils/operand/labeloperand.h"
#include "utils/type/type.h"
namespace backend::inst {
class PhiInst : public Instruction {
 public:
  auto Verify(context::CompilerContext *context) const -> bool override;
  auto Emit(std::ofstream *out) const -> void override;
  auto IsPhi() const -> bool override { return true; }

  auto GetIncomingBlocks() const -> const std::vector<utils::LabelOperand *> & { return incoming_blocks_; }
  auto AddIncoming(utils::Operand *value, utils::LabelOperand *block) -> void {
    operands_.emplace_back(value);
    incoming_blocks_.emplace_back(block);
  }
  auto GetIncomingValues() const -> std::vector<utils::Operand *> { return operands_; }
  auto GetIncomingSize() const -> size_t { return operands_.size(); }

 private:
  explicit PhiInst() : Instruction(OpKind::Phi) {}
  static auto Create(context::MIRContext *context, type::Type *result_type) -> PhiInst *;

  friend class utils::factory::InstFactory;
  std::vector<utils::LabelOperand *> incoming_blocks_;
};

}  // namespace backend::inst

#endif