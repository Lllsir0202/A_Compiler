#include "backend/mir/inst/arithm/widemulmod.h"

#include "backend/mir/utils/operand/operand.h"
#include "context/compiler_context.h"
#include "context/mir_context.h"

namespace backend::inst {

auto WideMulModInst::Create(context::MIRContext *context, utils::Operand *lhs, utils::Operand *rhs,
                            utils::Operand *modulus) -> WideMulModInst * {
  auto *inst = new WideMulModInst();
  inst->operands_ = {lhs, rhs, modulus};
  auto *result = context->GetNewRegOperand(context->GetIntegerType());
  result->SetDefInst(inst);
  inst->result_ = result;
  return inst;
}

auto WideMulModInst::Emit(std::ofstream *out) const -> void {
  *out << "  ";
  result_->Emit(out);
  *out << " = wide_mul_mod ";
  operands_[0]->Emit(out);
  *out << ", ";
  operands_[1]->Emit(out);
  *out << ", ";
  operands_[2]->Emit(out);
  *out << '\n';
}

auto WideMulModInst::Verify(context::CompilerContext *context) const -> bool {
  if (operands_.size() != 3 || result_ == nullptr || !result_->IsInt()) {
    context->Print("MIR WideMulModInst: invalid operands/result\n");
    return false;
  }
  for (auto *operand : operands_) {
    if (operand == nullptr || !operand->IsInt()) return false;
  }
  return true;
}

}  // namespace backend::inst
