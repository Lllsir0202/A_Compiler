#include "middleend/mlir/dialect/base/branch.h"
#include "context/compiler_context.h"
namespace middleend::mlir::base {
auto BrOp::Emit(std::ofstream *out) -> void {
  PrintIndent(out);
  *out << Name() << " " << target_->Name() << "\n";
}

auto BrOp::Verify(context::CompilerContext *context) -> bool {
  if (target_ == nullptr) {
    context->Print("BrOp must have a target block");
    return false;
  }
  return true;
}

auto CondBrOp::Emit(std::ofstream *out) -> void {
  PrintIndent(out);
  *out << Name() << " ";
  // Emit the condition operand
  operands_[0]->Emit(out);
  *out << ", " << true_target_->Name() << ", " << false_target_->Name() << "\n";
}

auto CondBrOp::Verify(context::CompilerContext *context) -> bool {
  if (true_target_ == nullptr) {
    context->Print("CondBrOp must have a true target block");
    return false;
  }
  if (false_target_ == nullptr) {
    context->Print("CondBrOp must have a false target block");
    return false;
  }
  return true;
}

}  // namespace middleend::mlir::base