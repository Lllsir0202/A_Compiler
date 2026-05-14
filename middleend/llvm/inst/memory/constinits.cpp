#include "middleend/llvm/inst/memory/constinits.h"
#include "context/compiler_context.h"

namespace middleend::llvm::inst {
auto ConstInitInst::Emit(std::ofstream *out) const -> void {
  // Emit the instruction in llvm ir format
  *out << "  ";
  GetResult()->Emit(out);
  *out << " = " << "constant ";
  auto type = operands_[0]->GetType();
  *out << utils::Operand::GetTypeInLLVMIR(type) << " ";
  operands_[0]->Emit(out);
  *out << ", align " << type->GetAlign() << "\n";
}

auto ConstInitInst::Verify(context::CompilerContext *context) const -> bool {
  // Verify the instruction, check if the operand is a literal and the result type is compatible with the operand type
  if (operands_[0]->IsGlobal()) {
    return true;
  }
  auto operand_type = operands_[0]->GetType();
  if (operand_type == nullptr || !operands_[0]->IsLiteral()) {
    context->Print("Operand of constinit instruction must be a literal\n");
    return false;
  }
  if (result_->GetType() == nullptr || result_->GetType() != operand_type) {
    context->Print("Result type of constinit instruction must be compatible with the operand type\n");
    return false;
  }
  if (result_->GetInstruction() != this) {
    context->Print("Result operand of constinit instruction must be defined by the instruction itself\n");
    return false;
  }
  return true;
}
}  // namespace middleend::llvm::inst