#include "middleend/llvm/inst/memory/alloca.h"
#include <cstddef>
#include "context/compiler_context.h"
#include "llvm/utils/operand/operand.h"
#include "type/type.h"
namespace middleend::llvm::inst {
auto AllocaInst::Create(context::LLVMIRContext *context, type::Type *allocated_type,
                        std::vector<int> dimensions) -> AllocaInst * {
  auto inst = new AllocaInst(allocated_type, std::move(dimensions));
  // To be simpler, we just record the instruction result as a pointer to the allocated_type
  auto ptr_type = context->GetTypeContext()->GetPtrType(allocated_type);
  auto res = context->GetNewRegOperand(ptr_type);
  inst->SetResult(res);
  return inst;
}

auto AllocaInst::Emit(std::ofstream *out) const -> void {
  // Emit the instruction in llvm ir format
  *out << "  ";
  GetResult()->Emit(out);
  *out << " = alloca " << utils::Operand::GetTypeInLLVMIR(allocated_type_);
  // If the allocated type is an array type, we need to emit the dimensions
  if (!dimensions_.empty()) {
    for (const auto &dim : dimensions_) {
      *out << "[ " << dim << " x ";
    }
    *out << utils::Operand::GetTypeInLLVMIR(allocated_type_);
    for (size_t i = 0; i < dimensions_.size(); ++i) {
      *out << "]";
    }
  }
  *out << ", align " << utils::Operand::GetTypeInLLVMIR(result_->GetType()) << "\n";
}

}  // namespace middleend::llvm::inst