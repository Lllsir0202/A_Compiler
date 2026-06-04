#include "backend/mir/inst/pseudo/copy.h"
#include "context/compiler_context.h"

namespace backend::inst {
auto CopyInst::Verify(context::CompilerContext *context) const -> bool {
  // The copy instruction should have exactly one operand, which is the source operand
  if (operands_.size() != 1) {
    context->Print("Operand size of copy instruction should be 1, but got " + std::to_string(operands_.size()));
    return false;
  }
  if (!operands_[0]->IsRegister() && !operands_[0]->IsLiteral()) {
    context->Print("Operand of copy instruction should be a register or a literal");
    return false;
  }
  return true;
}

auto CopyInst::Create(context::MIRContext *context, utils::Operand *src) -> CopyInst * {
  auto *inst = new CopyInst();
  inst->AddOperand(src);
  auto res = context->GetNewRegOperand(src->GetType());
  inst->SetResult(res);
  res->SetDefInst(inst);
  return inst;
}

auto CopyInst::Emit(std::ofstream *out) const -> void {
  *out << "  ";
  result_->Emit(out);
  *out << " = copy ";
  operands_[0]->Emit(out);
  *out << "\n";
}

}  // namespace backend::inst