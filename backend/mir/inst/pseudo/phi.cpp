#include "backend/mir/inst/pseudo/phi.h"
#include "context/compiler_context.h"

namespace backend::inst {
auto PhiInst::Verify(context::CompilerContext *context) const -> bool {
  // The phi instruction should have at least two operands, which are the incoming values and the corresponding incoming
  // blocks
  if (operands_.size() < 2) {
    context->Print("Operand size of phi instruction should be at least 2, but got " + std::to_string(operands_.size()));
    return false;
  }
  // Make sure all operands are registers or literals
  for (const auto &operand : operands_) {
    if (!operand->IsRegister() && !operand->IsLiteral()) {
      context->Print("All operands of phi instruction should be registers or literals");
      return false;
    }
  }
  return true;
}

auto PhiInst::Create(context::MIRContext *context, type::Type *result_type,
                     const std::vector<utils::Operand *> &operands,
                     std::vector<utils::Block *> incoming_blocks) -> PhiInst * {
  auto *inst = new PhiInst();
  inst->operands_ = operands;
  inst->incoming_blocks_ = std::move(incoming_blocks);
  auto res = context->GetNewRegOperand(result_type);
  inst->SetResult(res);
  res->SetDefInst(inst);
  return inst;
}

}  // namespace backend::inst