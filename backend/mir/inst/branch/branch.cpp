#include "backend/mir/inst/branch/branch.h"
#include "backend/mir/utils/operand/operand.h"
#include "context/compiler_context.h"

namespace backend::inst {
auto BrInst::Verify(context::CompilerContext *context) const -> bool {
  // The br instruction should have exactly one operand, which is the label operand
  if (operands_.size() != 1) {
    context->Print("Operand size of br instruction should be 1, but got " + std::to_string(operands_.size()));
    return false;
  }
  if (operands_[0]->GetOpType() != utils::OpType::Label) {
    context->Print("First operand of br instruction should be a label");
    return false;
  }
  return true;
}

auto BrInst::Create([[maybe_unused]] context::MIRContext *context, utils::LabelOperand *label) -> BrInst * {
  auto *inst = new BrInst();
  inst->AddOperand(label);
  return inst;
}

auto BrInst::Emit(std::ofstream *out) const -> void {
  *out << "  goto ";
  operands_[0]->Emit(out);
  *out << "\n";
}

auto BrCondInst::Verify(context::CompilerContext *context) const -> bool {
  // The condbr instruction should have exactly three operands, which are the condition, the true label and the false
  // label
  if (operands_.size() != 3) {
    context->Print("Operand size of condbr instruction should be 3, but got " + std::to_string(operands_.size()));
    return false;
  }
  if (!operands_[0]->IsRegister() && !operands_[0]->IsLiteral()) {
    context->Print("First operand of condbr instruction should be a register or a literal");
    return false;
  }
  if (!operands_[1]->IsLabel()) {
    context->Print("Second operand of condbr instruction should be a label");
    return false;
  }
  if (!operands_[2]->IsLabel()) {
    context->Print("Third operand of condbr instruction should be a label");
    return false;
  }
  return true;
}

auto BrCondInst::Create([[maybe_unused]] context::MIRContext *context, utils::Operand *condition,
                        utils::LabelOperand *true_label, utils::LabelOperand *false_label) -> BrCondInst * {
  auto *inst = new BrCondInst();
  inst->AddOperand(condition);
  inst->AddOperand(true_label);
  inst->AddOperand(false_label);
  return inst;
}

auto BrCondInst::Emit(std::ofstream *out) const -> void {
  *out << "  if ";
  operands_[0]->Emit(out);
  *out << " goto ";
  operands_[1]->Emit(out);
  *out << " else goto ";
  operands_[2]->Emit(out);
  *out << "\n";
}

}  // namespace backend::inst