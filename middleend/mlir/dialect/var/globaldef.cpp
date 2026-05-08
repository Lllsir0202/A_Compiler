#include "middleend/mlir/dialect/var/globaldef.h"
#include "context/compiler_context.h"

namespace middleend::mlir::var {
auto GlobalVarDefOp::Verify(context::CompilerContext *context) -> bool {
  // Verify the global variable definition, check if the dimensions are valid and the initializers are valid
  for (const auto &dim : dimensions_) {
    if (dim <= 0) {
      context->Print("Dimension of the global variable must be positive");
      return false;
    }
  }
  if (!int_init_values_.empty() && !float_init_values_.empty()) {
    context->Print("Global variable cannot have both integer and float initializers");
    return false;
  }
  return true;
}

auto GlobalVarDefOp::Emit(std::ofstream *out) -> void {
  PrintIndent(out);
  results_[0]->Emit(out);
  if (!dimensions_.empty()) {
    for (const auto &dim : dimensions_) {
      *out << "[" << dim << "]";
    }
  }
  *out << " " << results_[0]->GetType()->ToString();
  *out << " = ";
  if (is_const_) {
    *out << "const ";
  }
  *out << Name() << " init: ";
  if (int_init_values_.empty() && float_init_values_.empty()) {
    *out << "default_init";
  } else if (!int_init_values_.empty()) {
    for (size_t i = 0; i < int_init_values_.size(); ++i) {
      *out << int_init_values_[i];
      if (i != int_init_values_.size() - 1) {
        *out << ", ";
      }
    }
  } else if (!float_init_values_.empty()) {
    for (size_t i = 0; i < float_init_values_.size(); ++i) {
      *out << float_init_values_[i];
      if (i != float_init_values_.size() - 1) {
        *out << ", ";
      }
    }
  }
  *out << "\n";
}

}  // namespace middleend::mlir::var