#include "middleend/llvm/inst/memory/globaldef.h"
#include <fstream>
#include "context/compiler_context.h"
#include "llvm/utils/operand/operand.h"
#include "type/type.h"

namespace middleend::llvm::inst {
auto GlobalDefInst::Create(context::LLVMIRContext *context, type::Type *result_type, type::Type *allocated_type,
                           const std::string &name, std::vector<int> dimensions, std::vector<int> initial_values,
                           bool is_constant) -> GlobalDefInst * {
  auto *inst = new GlobalDefInst(allocated_type, name, std::move(dimensions), std::move(initial_values));
  inst->is_constant_ = is_constant;
  // Set all result's type to pointer
  auto ptr_type = context->GetTypeContext()->GetPtrType(result_type);
  auto res = context->GetNewGlobalOperand(name, ptr_type);
  res->SetInstruction(inst);
  inst->SetResult(res);
  return inst;
}

auto GlobalDefInst::Create(context::LLVMIRContext *context, type::Type *result_type, type::Type *allocated_type,
                           const std::string &name, std::vector<int> dimensions, std::vector<float> initial_values,
                           bool is_constant) -> GlobalDefInst * {
  auto *inst = new GlobalDefInst(allocated_type, name, std::move(dimensions), std::move(initial_values));
  inst->is_constant_ = is_constant;
  // Set all result's type to pointer
  auto ptr_type = context->GetTypeContext()->GetPtrType(result_type);
  auto res = context->GetNewGlobalOperand(name, ptr_type);
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
  if (allocated_type_->GetKind() == type::TypeKind::ARRAY) {
    context->Print("Global variable cannot be of array type, it should be a pointer to the array type");
    return false;
  }

  auto base_type = result_->GetType();
  if (base_type->GetKind() == type::TypeKind::PTR) {
    base_type = static_cast<type::PtrType *>(base_type)->GetBaseType();
  }

  if (!int_initial_values_.empty() && allocated_type_->GetKind() != type::TypeKind::INT) {
    context->Print("Type of the global variable does not match the type of the initializers");
    return false;
  }
  if (!float_initial_values_.empty() && allocated_type_->GetKind() != type::TypeKind::FLOAT) {
    context->Print("Type of the global variable does not match the type of the initializers");
    return false;
  }

  return true;
}

auto GlobalDefInst::Emit(std::ofstream *out) const -> void {
  *out << "@" << name_ << " = ";
  if (is_constant_) {
    *out << "constant ";
  } else {
    *out << "global ";
  }
  if (!dimensions_.empty()) {
    for (const auto &dim : dimensions_) {
      *out << "[" << dim << " x ";
    }
    // output the element type
    *out << utils::Operand::GetTypeInLLVMIR(allocated_type_);
    for (size_t i = 0; i < dimensions_.size(); ++i) {
      *out << "]";
    }
    *out << " ";
  }

  if (!int_initial_values_.empty() && !dimensions_.empty()) {
    OutputArrayInitializer(out, int_initial_values_, dimensions_);
  } else if (!float_initial_values_.empty() && !dimensions_.empty()) {
    OutputArrayInitializer(out, float_initial_values_, dimensions_);
  } else if (!int_initial_values_.empty() && dimensions_.empty()) {
    *out << "i32 " << int_initial_values_[0];
  } else if (!float_initial_values_.empty() && dimensions_.empty()) {
    *out << "float " << utils::FloatToLLVMIRFormat(float_initial_values_[0]);
  } else {
    // If the initial value is empty, provide a default initializer
    auto type = result_->GetType();
    if (type->GetKind() == type::TypeKind::PTR) {
      type = static_cast<type::PtrType *>(type)->GetBaseType();
    }
    if (type->GetKind() == type::TypeKind::ARRAY) {
      *out << "zeroinitializer";
    } else if (type->GetKind() == type::TypeKind::INT) {
      *out << "i32 0";
    } else if (type->GetKind() == type::TypeKind::FLOAT) {
      *out << "float 0.000000E+00";
    } else {
      throw std::runtime_error("Unsupported type for global variable initializer");
    }
  }
  auto type = result_->GetType();
  if (type->GetKind() == type::TypeKind::PTR) {
    type = static_cast<type::PtrType *>(type)->GetBaseType();
  }
  *out << ", align " << type->GetAlign();
  *out << "\n";
}

auto GlobalDefInst::OutputArrayInitializer(std::ostream *out, const std::vector<int> &values,
                                           const std::vector<int> &dimensions, size_t offset) const -> void {
  if (dimensions.empty()) {
    return;
  }

  auto print_sub_array_type = [&](const std::vector<int> &dims) {
    for (auto dim : dims) {
      *out << "[" << dim << " x ";
    }
    *out << utils::Operand::GetTypeInLLVMIR(allocated_type_);
    for (size_t i = 0; i < dims.size(); ++i) {
      *out << "]";
    }
  };

  *out << "[";
  size_t dim_size = dimensions[0];
  size_t inner_size = 1;
  for (size_t i = 1; i < dimensions.size(); ++i) {
    inner_size *= dimensions[i];
  }

  std::vector<int> sub_dims(dimensions.begin() + 1, dimensions.end());
  for (size_t i = 0; i < dim_size; ++i) {
    if (i != 0) {
      *out << ", ";
    }
    if (dimensions.size() > 1) {
      print_sub_array_type(sub_dims);
      *out << " ";
      OutputArrayInitializer(out, values, sub_dims, offset + i * inner_size);
    } else {
      *out << "i32 " << values[offset + i];
    }
  }
  *out << "]";
}

auto GlobalDefInst::OutputArrayInitializer(std::ostream *out, const std::vector<float> &values,
                                           const std::vector<int> &dimensions, size_t offset) const -> void {
  if (dimensions.empty()) {
    return;
  }

  auto print_sub_array_type = [&](const std::vector<int> &dims) {
    for (auto dim : dims) {
      *out << "[" << dim << " x ";
    }
    *out << utils::Operand::GetTypeInLLVMIR(allocated_type_);
    for (size_t i = 0; i < dims.size(); ++i) {
      *out << "]";
    }
  };

  *out << "[";
  size_t dim_size = dimensions[0];
  size_t inner_size = 1;
  for (size_t i = 1; i < dimensions.size(); ++i) {
    inner_size *= dimensions[i];
  }

  std::vector<int> sub_dims(dimensions.begin() + 1, dimensions.end());
  for (size_t i = 0; i < dim_size; ++i) {
    if (i != 0) {
      *out << ", ";
    }
    if (dimensions.size() > 1) {
      print_sub_array_type(sub_dims);
      *out << " ";
      OutputArrayInitializer(out, values, sub_dims, offset + i * inner_size);
    } else {
      *out << "float " << utils::FloatToLLVMIRFormat(values[offset + i]);
    }
  }
  *out << "]";
}

}  // namespace middleend::llvm::inst