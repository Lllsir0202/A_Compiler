#include "backend/mir/inst/branch/ret.h"
#include "backend/mir/utils/operand/operand.h"
#include "context/compiler_context.h"
#include "mir_context.h"
namespace backend::inst {
auto RetInst::Verify(context::CompilerContext *context) const -> bool {
  // The ret instruction should have at most one operand, which is the return value
  if (operands_.size() > 1) {
    context->Print("Operand size of ret instruction should be at most 1, but got " + std::to_string(operands_.size()));
    return false;
  }
  if (operands_.size() == 1 && !operands_[0]->IsRegister() && !operands_[0]->IsLiteral()) {
    context->Print("Operand of ret instruction should be a register or a literal");
    return false;
  }
  return true;
}

auto RetInst::Create([[maybe_unused]] context::MIRContext *context, utils::Operand *ret_value) -> RetInst * {
  auto *inst = new RetInst();
  if (ret_value != nullptr) {
    inst->AddOperand(ret_value);
  }
  return inst;
}

auto RetInst::Emit(std::ofstream *out) const -> void {
  *out << "  return";
  if (!operands_.empty()) {
    *out << " ";
    operands_[0]->Emit(out);
  }
  *out << "\n";
}

}  // namespace backend::inst