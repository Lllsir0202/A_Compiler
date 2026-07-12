#include "middleend/llvm/inst/memory/allocainst.h"
#include <cstddef>
#include "context/compiler_context.h"
#include "middleend/llvm/utils/operand/operand.h"
#include "utils/type/type.h"
namespace middleend::llvm::inst {
auto AllocaInst::Create(context::LLVMIRContext *context, type::Type *result_type, type::Type *allocated_type,
                        std::vector<int> dimensions) -> AllocaInst * {
  auto inst = new AllocaInst(allocated_type, std::move(dimensions));
  // The allocated type should be the element type of the allocated memory
  // For example, if we allocate an array of int,
  // the allocated type should be int, not the array type
  auto type_context = context->GetTypeContext();
  // ptr to the res's original type
  auto ptr_type = type_context->GetPtrType(result_type);
  auto res = context->GetNewRegOperand(ptr_type);
  inst->SetResult(res);
  res->SetInstruction(inst);
  return inst;
}

auto AllocaInst::Emit(std::ofstream *out) const -> void {
  // Emit the instruction in llvm ir format
  *out << "  ";
  GetResult()->Emit(out);
  *out << " = alloca ";
  // If the allocated type is an array type, we need to emit the dimensions
  if (!dimensions_.empty()) {
    for (const auto &dim : dimensions_) {
      *out << "[ " << dim << " x ";
    }
    *out << utils::Operand::GetTypeInLLVMIR(allocated_type_);
    for (size_t i = 0; i < dimensions_.size(); ++i) {
      *out << "]";
    }
  } else {
    *out << utils::Operand::GetTypeInLLVMIR(allocated_type_);
  }
  auto type = GetResult()->GetType();
  if (type->GetKind() == type::TypeKind::PTR) {
    type = static_cast<type::PtrType *>(type)->GetBaseType();
  }
  *out << ", align " << type->GetAlign() << "\n";
}

auto AllocaInst::Verify(context::CompilerContext *context) const -> bool {
  // Verify the instruction, check if the allocated type is valid
  if (allocated_type_ == nullptr) {
    context->Print("Allocated type of alloca instruction cannot be null");
    return false;
  }
  if (result_ == nullptr) {
    context->Print("Result operand of alloca instruction cannot be null");
    return false;
  }
  if (GetParentBlock() == nullptr) {
    context->Print("Parent block of alloca instruction cannot be null");
    return false;
  }
  if (result_->GetInstruction() != this) {
    context->Print("Result operand of alloca instruction must point to the instruction itself");
    return false;
  }
  if (!result_->IsRegister()) {
    context->Print("Result operand of alloca instruction must be a register operand");
    return false;
  }
  return true;
}

}  // namespace middleend::llvm::inst