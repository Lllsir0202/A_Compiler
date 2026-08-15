#include "middleend/llvm/inst/memory/gep.h"
#include "context/compiler_context.h"
#include "llvm/inst/memory/allocainst.h"
#include "llvm/inst/opkind.h"

namespace middleend::llvm::inst {
auto GetElementPtrInst::Emit(std::ofstream *out) const -> void {
  // Emit the instruction in llvm ir format
  *out << "  ";
  GetResult()->Emit(out);
  *out << " = getelementptr ";
  auto inst = operands_[0]->GetInstruction();

  // if the base pointer is from function parameter,
  // we cannot get the dimensions and allocated type information,
  // so we just emit the pointer type of the base pointer
  if (inst != nullptr && inst->VerifyOpKind(OpKind::Alloca)) {
    auto *alloca_inst = static_cast<AllocaInst *>(inst);
    auto &dimensions = alloca_inst->GetDimensions();
    for (auto &dim : dimensions) {
      *out << "[" << dim << " x ";
    }
    *out << utils::Operand::GetTypeInLLVMIR(alloca_inst->GetAllocatedType());
    for (size_t i = 0; i < dimensions.size(); ++i) {
      *out << "]";
    }
  } else if (inst != nullptr && inst->VerifyOpKind(OpKind::GlobalDef)) {
    auto *global_def_inst = static_cast<GlobalDefInst *>(inst);
    auto &dimensions = global_def_inst->GetDimensions();
    for (auto &dim : dimensions) {
      *out << "[" << dim << " x ";
    }
    *out << utils::Operand::GetTypeInLLVMIR(global_def_inst->GetAllocatedType());
    for (size_t i = 0; i < dimensions.size(); ++i) {
      *out << "]";
    }
  } else {
    // Otherwise, the base is a plain pointer value: either a function parameter
    // (no defining instruction) or a pointer produced by another instruction,
    // e.g. a GEP created when inlining a callee that indexes one of its pointer
    // parameters. In both cases the element type is read from the base pointer's
    // own type, which is exactly what LLVM IR expects.
    auto base_type = operands_[0]->GetType();
    if (base_type != nullptr && base_type->GetKind() == type::TypeKind::PTR) {
      base_type = static_cast<type::PtrType *>(base_type)->GetBaseType();
    }
    // Print the base type of an recursive array type
    int array_dim = 0;
    while (base_type != nullptr && base_type->GetKind() == type::TypeKind::ARRAY) {
      auto array_type = static_cast<type::ArrayType *>(base_type);
      *out << "[" << array_type->GetSize() << " x ";
      base_type = array_type->GetElementType();
      array_dim++;
    }
    *out << utils::Operand::GetTypeInLLVMIR(base_type);
    for (int i = 0; i < array_dim; i++) {
      *out << "]";
    }
  }

  *out << ", ptr ";
  operands_[0]->Emit(out);
  for (size_t i = 1; i < operands_.size(); ++i) {
    *out << ", i32 ";
    operands_[i]->Emit(out);
  }
  *out << "\n";
}

auto GetElementPtrInst::Verify(context::CompilerContext *context) const -> bool {
  if (operands_.empty() || operands_[0] == nullptr || !operands_[0]->IsPtr()) {
    context->Print("Base operand of GEP instruction must be a pointer\n");
    return false;
  }

  auto *base_type = operands_[0]->GetType();
  if (base_type == nullptr || base_type->GetKind() != type::TypeKind::PTR) {
    context->Print("Base operand of GEP instruction has no pointer type\n");
    return false;
  }
  auto *indexed_type = static_cast<type::PtrType *>(base_type)->GetBaseType();

  for (size_t i = 1; i < operands_.size(); ++i) {
    if (operands_[i] == nullptr || operands_[i]->GetType() == nullptr || !operands_[i]->IsInt32()) {
      context->Print("Index of GEP instruction must be of integer type\n");
      return false;
    }
    // LLVM's first GEP index performs pointer arithmetic over the source element
    // type. Every later index must descend through an aggregate layer. Walking
    // the type here catches malformed flattened chains such as
    // `[10 x i32], 0, 9, -7`, whose last index attempts to index into i32.
    if (i == 1) {
      continue;
    }
    if (indexed_type == nullptr || indexed_type->GetKind() != type::TypeKind::ARRAY) {
      context->Print("GEP instruction has too many indices for its source element type\n");
      return false;
    }
    indexed_type = static_cast<type::ArrayType *>(indexed_type)->GetElementType();
  }
  if (result_ == nullptr || result_->GetInstruction() != this) {
    context->Print("Result operand of GEP instruction must be defined by the instruction itself\n");
    return false;
  }
  return true;
}

auto GetElementPtrInst::Create(context::LLVMIRContext *context, type::Type *result_type, utils::Operand *base_ptr,
                               const std::vector<utils::Operand *> &indices) -> GetElementPtrInst * {
  // Create a new GEP instruction
  auto *inst = new GetElementPtrInst(base_ptr, indices);
  // Create result
  // result type will be the pointer type of the allocated type
  auto res = context->GetNewRegOperand(result_type);
  inst->SetResult(res);
  res->SetInstruction(inst);

  return inst;
}

}  // namespace middleend::llvm::inst
