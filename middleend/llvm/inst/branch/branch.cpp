#include "middleend/llvm/inst/branch/branch.h"
#include "context/compiler_context.h"

namespace middleend::llvm::inst {
auto BrInst::Emit(std::ofstream *out) const -> void {
  *out << " ";
  *out << "br ";
  target_->Emit(out);
  *out << "\n";
}

auto BrInst::Verify(context::CompilerContext *context) const -> bool {
  if (target_ == nullptr) {
    context->Print("Br instruction must have a target label");
    return false;
  }
  if (GetParentBlock() == nullptr) {
    context->Print("Br instruction must have a parent block");
    return false;
  }
  return true;
}

auto BrInst::Create([[maybe_unused]] context::LLVMIRContext *context, utils::LabelOperand *target) -> BrInst * {
  auto *inst = new BrInst();
  inst->target_ = target;
  return inst;
}

// Condition branch
auto BrCondInst::Emit(std::ofstream *out) const -> void {
  *out << " ";
  *out << "br ";
  // Has checked in verify that condition_ is a boolean
  *out << "i1 ";
  GetCondition()->Emit(out);
  *out << ", ";
  true_target_->Emit(out);
  *out << ", ";
  false_target_->Emit(out);
  *out << "\n";
}

auto BrCondInst::Verify(context::CompilerContext *context) const -> bool {
  if (operands_[0] == nullptr) {
    context->Print("BrCond instruction must have a condition operand");
    return false;
  }
  if (true_target_ == nullptr || false_target_ == nullptr) {
    context->Print("BrCond instruction must have both true and false target labels");
    return false;
  }
  if (!operands_[0]->IsBool()) {
    context->Print("BrCond instruction condition operand must be of type i1");
    return false;
  }
  return true;
}

auto BrCondInst::Create([[maybe_unused]] context::LLVMIRContext *context, utils::Operand *condition,
                        utils::LabelOperand *true_target, utils::LabelOperand *false_target) -> BrCondInst * {
  auto *inst = new BrCondInst();
  inst->AddOperand(condition);
  inst->true_target_ = true_target;
  inst->false_target_ = false_target;
  return inst;
}

}  // namespace middleend::llvm::inst