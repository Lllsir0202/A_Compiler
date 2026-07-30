#include "middleend/mlir/dialect/arithm/widemulmod.h"

#include "context/compiler_context.h"
#include "middleend/mlir/utils/value/value.h"
#include "utils/type/type.h"

namespace middleend::mlir::arithm {

auto WideMulModOp::Emit(std::ofstream *out) -> void {
  PrintIndent(out);
  results_[0]->Emit(out);
  *out << " = " << Name() << " ";
  operands_[0]->Emit(out);
  *out << ", ";
  operands_[1]->Emit(out);
  *out << ", ";
  operands_[2]->Emit(out);
  *out << '\n';
}

auto WideMulModOp::Verify(context::CompilerContext *context) -> bool {
  if (operands_.size() != 3 || results_.size() != 1) {
    context->Print("WideMulModOp: expected three operands and one result\n");
    return false;
  }
  for (auto *value : operands_) {
    if (value == nullptr || value->GetType() == nullptr || value->GetType()->GetKind() != type::TypeKind::INT) {
      context->Print("WideMulModOp: operands must be Int\n");
      return false;
    }
  }
  return results_[0] != nullptr && results_[0]->GetType() != nullptr &&
         results_[0]->GetType()->GetKind() == type::TypeKind::INT;
}

}  // namespace middleend::mlir::arithm
