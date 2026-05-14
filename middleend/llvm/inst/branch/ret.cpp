#include "middleend/llvm/inst/branch/ret.h"
#include "context/compiler_context.h"
#include "middleend/llvm/utils/operand/operand.h"

namespace middleend::llvm::inst {
auto RetInst::Emit(std::ofstream *out) const -> void {
  *out << "  ";
  *out << "ret ";
  if (!operands_.empty()) {
    auto type = operands_[0]->GetType();
    *out << utils::Operand::GetTypeInLLVMIR(type) << " ";
    operands_[0]->Emit(out);
  } else {
    *out << "void";
  }
  *out << "\n";
}

auto RetInst::Verify(context::CompilerContext *context) const -> bool {
  if (GetParentBlock() == nullptr) {
    context->Print("Ret instruction must have a parent block");
    return false;
  }
  // If there is a return value operand, it must be valid
  if (!operands_.empty() && operands_[0]->GetType() == nullptr) {
    context->Print("Ret instruction has an invalid return value operand");
    return false;
  }
  return true;
}

auto RetInst::Create([[maybe_unused]] context::LLVMIRContext *context, utils::Operand *ret_value) -> RetInst * {
  auto *inst = new RetInst();
  if (ret_value != nullptr) {
    inst->AddOperand(ret_value);
  }
  return inst;
}

}  // namespace middleend::llvm::inst