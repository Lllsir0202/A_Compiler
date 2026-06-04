#include "backend/mir/inst/pseudo/phi.h"
#include "context/compiler_context.h"
#include "mir/utils/operand/labeloperand.h"

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

auto PhiInst::Create(context::MIRContext *context, type::Type *result_type) -> PhiInst * {
  auto *inst = new PhiInst();
  auto res = context->GetNewRegOperand(result_type);
  inst->SetResult(res);
  res->SetDefInst(inst);
  return inst;
}

auto PhiInst::Emit(std::ofstream *out) const -> void {
  *out << "  ";
  result_->Emit(out);
  *out << " = phi " << result_->GetType()->ToString() << " ";
  for (size_t i = 0; i < operands_.size(); ++i) {
    auto operand = operands_[i];
    auto block = incoming_blocks_[i];
    *out << "[ ";
    operand->Emit(out);
    *out << ", ";
    block->Emit(out);
    *out << " ]";
    if (i != operands_.size() - 1) {
      *out << ", ";
    }
  }
  *out << "\n";
}

}  // namespace backend::inst