#include "backend/mir/inst/memory/store.h"
#include "context/compiler_context.h"

namespace backend::inst {
auto StoreInst::Verify(context::CompilerContext *context) const -> bool {
  // The store instruction should have exactly two operands, which are the value operand and the pointer operand
  if (operands_.size() != 2) {
    context->Print("Operand size of store instruction should be 2, but got " + std::to_string(operands_.size()));
    return false;
  }
  // Make sure the first operand is a register or a literal, and the second operand is a register or a memory operand
  if ((!operands_[0]->IsRegister() && !operands_[0]->IsLiteral()) ||
      (!operands_[1]->IsRegister() && !operands_[1]->IsMemory())) {
    context->Print(
        "First operand of store instruction should be a register or a literal, and second operand should be a register "
        "or a memory operand");
    return false;
  }
  return true;
}

auto StoreInst::Create([[maybe_unused]] context::MIRContext *context, utils::Operand *value_operand,
                       utils::Operand *ptr_operand) -> StoreInst * {
  auto *inst = new StoreInst();
  inst->AddOperand(value_operand);
  inst->AddOperand(ptr_operand);
  return inst;
}

auto StoreInst::Emit(std::ofstream *out) const -> void {
  *out << "  store ";
  operands_[0]->Emit(out);
  *out << ", ";
  operands_[1]->Emit(out);
  *out << "\n";
}

}  // namespace backend::inst