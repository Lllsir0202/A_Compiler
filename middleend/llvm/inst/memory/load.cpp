#include "middleend/llvm/inst/memory/load.h"
#include "context/compiler_context.h"
#include "type/type.h"

namespace middleend::llvm::inst {
auto LoadInst::Emit(std::ofstream *out) const -> void {
  // Emit the instruction in llvm ir format
  *out << "  ";
  GetResult()->Emit(out);
  *out << " = load ";
  auto ptr_type = operands_[0]->GetType();
  if (ptr_type == nullptr || ptr_type->GetKind() != type::TypeKind::PTR) {
    throw std::logic_error("Operand of load instruction must be of pointer type");
  }
  auto base_type = static_cast<type::PtrType *>(ptr_type)->GetBaseType();
  *out << utils::Operand::GetTypeInLLVMIR(base_type) << ", ptr ";
  operands_[0]->Emit(out);
  *out << ", align " << static_cast<type::PtrType *>(ptr_type)->GetAlign() << "\n";
  *out << "\n";
}

auto LoadInst::Verify(context::CompilerContext *context) const -> bool {
  // Verify the instruction, check if the operand is of pointer type and the result type is compatible with the base
  // type of the pointer
  auto ptr_type = operands_[0]->GetType();
  if (ptr_type == nullptr || ptr_type->GetKind() != type::TypeKind::PTR) {
    context->Print("Operand of load instruction must be of pointer type");
    return false;
  }
  auto base_type = static_cast<type::PtrType *>(ptr_type)->GetBaseType();
  if (result_->GetType() == nullptr || result_->GetType() != base_type) {
    context->Print("Result type of load instruction must be compatible with the base type of the pointer");
    return false;
  }
  if (result_->GetInstruction() != this) {
    context->Print("Result operand of load instruction must be defined by the instruction itself");
    return false;
  }
  return true;
}

auto LoadInst::Create(context::LLVMIRContext *context, type::Type *result_type, utils::Operand *ptr) -> LoadInst * {
  // Create a new load instruction
  auto *inst = new LoadInst();
  // Create result operand for the instruction
  auto *res = context->GetNewRegOperand(result_type);
  inst->SetResult(res);
  res->SetInstruction(inst);
  inst->AddOperand(ptr);
  return inst;
}
}  // namespace middleend::llvm::inst