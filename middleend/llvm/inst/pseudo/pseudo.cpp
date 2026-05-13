#include "middleend/llvm/inst/pseudo/pseudo.h"
#include "context/compiler_context.h"
#include "utils/type/type.h"

namespace middleend::llvm::inst {
auto CopyInst::Emit([[maybe_unused]] std::ofstream *out) const -> void {
  // Should never be emitted
  //   throw std::logic_error("Copy instruction should never be emitted");
  result_[0].Emit(out);
  *out << " = copy ";
  operands_[0]->Emit(out);
  *out << "\n";
}

auto CopyInst::Verify([[maybe_unused]] context::CompilerContext *context) const -> bool {
  // Should never be verified
  //   throw std::logic_error("Copy instruction should never be verified");

  // Since the copy instruction is only used as a pseudo instruction in the middle of the optimization,
  // we can just return true here, and it will be removed in the final code generation phase
  return true;
}

auto CopyInst::Create(context::LLVMIRContext *context, type::Type *result_type, utils::Operand *src) -> CopyInst * {
  // Create a new copy instruction
  auto *inst = new CopyInst();
  auto res = context->GetNewRegOperand(result_type);
  inst->AddOperand(src);
  inst->SetResult(res);
  res->SetInstruction(inst);
  // No result
  return inst;
}

}  // namespace middleend::llvm::inst