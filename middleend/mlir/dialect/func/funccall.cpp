#include "middleend/mlir/dialect/func/funccall.h"
#include "context/compiler_context.h"
#include "middleend/mlir/dialect/func/funcdef.h"

namespace middleend::mlir::func {
auto FuncCallOp::Verify(context::CompilerContext *context) -> bool {
  // Check if the function definition operation exists
  auto func_def_op = context->GetIRContext()->GetMLIRContext()->GetResourceContext()->GetFuncDefOpFromFuncName(name_);
  if (func_def_op == nullptr) {
    // If in lowering stage
    auto func_def = context->GetIRContext()->GetMLIRContext()->GetLowerContext()->GetFuncDefOpFromFuncName(name_);
    if (func_def == nullptr) {
      context->Print("FuncCallOp: function definition not found for function name " + name_ + "\n");
      return false;
    }

    // Has verified in high-level MLIR
    return true;
  }

  // Currently only one result
  if (results_.size() != 1) {
    context->Print("FuncCallOp: results size should be 1\n");
    return false;
  }

  // Check if the number of arguments matches
  if (operands_.size() != func_def_op->GetArguments().size()) {
    context->Print("FuncCallOp: argument count mismatch\n");
    return false;
  }

  // Check if the type of each argument matches
  for (size_t i = 0; i < operands_.size(); ++i) {
    if (operands_[i]->GetType()->GetKind() != func_def_op->GetArguments()[i]->GetType()->GetKind()) {
      context->Print("FuncCallOp: argument type mismatch\n");
      return false;
    }
  }

  // Check if the return type matches
  if (results_[0]->GetType() != func_def_op->GetReturnType()) {
    context->Print("FuncCallOp: return type mismatch\n");
    return false;
  }
  return true;
}

}  // namespace middleend::mlir::func