#include "middleend/llvm/inst/branch/ret.h"
#include "context/compiler_context.h"
#include "middleend/llvm/utils/operand/operand.h"

namespace middleend::llvm::inst {
auto RetInst::Emit(std::ofstream *out) const -> void {
  *out << "ret ";
  if (ret_value_ != nullptr) {
    auto type = ret_value_->GetType();
    *out << utils::Operand::GetTypeInLLVMIR(type) << " ";
    ret_value_->Emit(out);
  } else {
    *out << "void";
  }
}

auto RetInst::Verify(context::CompilerContext *context) const -> bool {
  if (GetParentBlock() == nullptr) {
    context->Print("Ret instruction must have a parent block");
    return false;
  }
  // If ret_value_ is not nullptr, it must be a valid operand
  if (ret_value_ != nullptr && ret_value_->GetType() == nullptr) {
    context->Print("Ret instruction has an invalid return value operand");
    return false;
  }
  return true;
}

auto RetInst::Create([[maybe_unused]] context::LLVMIRContext *context, utils::Operand *ret_value) -> RetInst * {
  auto *inst = new RetInst();
  inst->ret_value_ = ret_value;
  return inst;
}

}  // namespace middleend::llvm::inst