#include "middleend/llvm/inst/memory/globaldef.h"
#include <fstream>
#include "context/compiler_context.h"
#include "type/type.h"

namespace middleend::llvm::inst {
auto GlobalDefInst::Create(context::LLVMIRContext *context, type::Type *allocated_type, std::string name,
                           std::vector<int> initial_values) -> GlobalDefInst * {
  auto *inst = new GlobalDefInst(allocated_type, std::move(name), std::move(initial_values));
  auto res = context->GetNewGlobalOperand(name, allocated_type);
  res->SetInstruction(inst);
  inst->SetResult(res);
  return inst;
}

auto GlobalDefInst::Create(context::LLVMIRContext *context, type::Type *allocated_type, std::string name,
                           std::vector<float> initial_values) -> GlobalDefInst * {
  auto *inst = new GlobalDefInst(allocated_type, std::move(name), std::move(initial_values));
  auto res = context->GetNewGlobalOperand(name, allocated_type);
  res->SetInstruction(inst);
  inst->SetResult(res);
  return inst;
}

auto GlobalDefInst::Verify(context::CompilerContext *context) const -> bool {
  // Verify the global variable definition, check if the dimensions are valid and the initializers are valid
  for (const auto &dim : dimensions_) {
    if (dim <= 0) {
      context->Print("Dimension of the global variable must be positive");
      return false;
    }
  }
  if (!int_initial_values_.empty() && !float_initial_values_.empty()) {
    context->Print("Global variable cannot have both integer and float initializers");
    return false;
  }
  auto type = allocated_type_;
  if (type->GetKind() == type::TypeKind::ARRAY) {
    type = static_cast<type::ArrayType *>(allocated_type_)->GetElementType();
  }
  if (!int_initial_values_.empty() && type->GetKind() != type::TypeKind::INT) {
    context->Print("Type of the global variable does not match the type of the initializers");
    return false;
  }
  if (!float_initial_values_.empty() && type->GetKind() != type::TypeKind::FLOAT) {
    context->Print("Type of the global variable does not match the type of the initializers");
    return false;
  }

  return true;
}

auto GlobalDefInst::Emit(std::ofstream *out) const -> void {
  *out << "@" << name_ << " = global ";
  if (!dimensions_.empty()) {
    for (size_t i = 0; i < dimensions_.size(); ++i) {
      *out << "[";
      *out << dimensions_[i];
      if (i != dimensions_.size() - 1) {
        *out << " x ";
      }
    }
    // output the element type
    auto type = allocated_type_;
    if (type->GetKind() == type::TypeKind::ARRAY) {
      type = static_cast<type::ArrayType *>(allocated_type_)->GetElementType();
    }
    *out << utils::Operand::GetTypeInLLVMIR(type);
    for (size_t i = 0; i < dimensions_.size(); ++i) {
      *out << "]";
    }
    *out << " ";
  }

  if (!int_initial_values_.empty()) {
    for (size_t i = 0; i < int_initial_values_.size(); ++i) {
      *out << "i32 " << int_initial_values_[i];
      if (i != int_initial_values_.size() - 1) {
        *out << ", ";
      }
    }
  } else if (!float_initial_values_.empty()) {
    for (size_t i = 0; i < float_initial_values_.size(); ++i) {
      *out << "float " << float_initial_values_[i];
      if (i != float_initial_values_.size() - 1) {
        *out << ", ";
      }
    }
  } else {
    // If the initial value is empty, provide a default initializer
    auto type = allocated_type_;
    if (type->GetKind() == type::TypeKind::ARRAY) {
      type = static_cast<type::ArrayType *>(allocated_type_)->GetElementType();
    }
    if (type->GetKind() == type::TypeKind::INT) {
      *out << "i32 0";
    } else if (type->GetKind() == type::TypeKind::FLOAT) {
      *out << "float 0.000000e+00";
    } else {
      throw std::runtime_error("Unsupported type for global variable initializer");
    }
  }
  *out << "\n";
}

}  // namespace middleend::llvm::inst