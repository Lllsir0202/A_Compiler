#include "middleend/llvm/inst/memory/store.h"
#include "context/compiler_context.h"

namespace middleend::llvm::inst {
auto StoreInst::Emit(std::ofstream *out) const -> void {
  // Emit the instruction in llvm ir format
  *out << "  store ";
  auto value_type = operands_[0]->GetType();
  *out << utils::Operand::GetTypeInLLVMIR(value_type) << " ";
  operands_[0]->Emit(out);
  *out << ", ptr ";
  operands_[1]->Emit(out);
  *out << ", align " << operands_[0]->GetType()->GetAlign() << "\n";
}

auto StoreInst::Verify(context::CompilerContext *context) const -> bool {
  // Verify the instruction, check if the value type is compatible with the pointer type
  auto value_type = operands_[0]->GetType();
  auto ptr_type = operands_[1]->GetType();
  if (ptr_type == nullptr || ptr_type->GetKind() != type::TypeKind::PTR) {
    context->Print("Pointer operand of store instruction must be of pointer type\n");
    return false;
  }
  auto base_type = static_cast<type::PtrType *>(ptr_type)->GetBaseType();

  if (value_type == nullptr || value_type != base_type) {
    printf("Value type of the store instruction: %s, and expect %s\n", value_type->ToString().c_str(),
           base_type->ToString().c_str());  // --- IGNORE ---
    context->Print("Value type of store instruction must be compatible with the base type of the pointer\n");
    return false;
  }
  return true;
}

auto StoreInst::Create([[maybe_unused]] context::LLVMIRContext *context, utils::Operand *value,
                       utils::Operand *ptr) -> StoreInst * {
  // Create a new store instruction
  auto *inst = new StoreInst();
  inst->AddOperand(value);
  inst->AddOperand(ptr);
  // No result
  return inst;
}
}  // namespace middleend::llvm::inst