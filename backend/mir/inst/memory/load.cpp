#include "backend/mir/inst/memory/load.h"
#include "context/compiler_context.h"
namespace backend::inst {
auto LoadInst::Verify(context::CompilerContext *context) const -> bool {
  // The load instruction should have exactly one operand, which is the pointer operand
  if (operands_.size() != 1) {
    context->Print("Operand size of load instruction should be 1, but got " + std::to_string(operands_.size()));
    return false;
  }
  // Make sure the operand is a register or a memory operand
  if (!operands_[0]->IsRegister() && !operands_[0]->IsMemory()) {
    context->Print("Operand of load instruction should be a register or a memory operand");
    return false;
  }
  return true;
}

auto LoadInst::Create(context::MIRContext *context, utils::Operand *ptr_operand) -> LoadInst * {
  auto *inst = new LoadInst();
  inst->AddOperand(ptr_operand);
  auto res = context->GetNewRegOperand(ptr_operand->GetType());
  inst->SetResult(res);
  res->SetDefInst(inst);
  return inst;
}

}  // namespace backend::inst