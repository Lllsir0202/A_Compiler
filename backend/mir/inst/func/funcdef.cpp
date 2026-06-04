#include "backend/mir/inst/func/funcdef.h"
#include "context/compiler_context.h"

namespace backend::inst {
auto FuncDefInst::Verify(context::CompilerContext *context) const -> bool {
  // The funcdef instruction should have no operand
  if (!operands_.empty()) {
    context->Print("Operand size of funcdef instruction should be 0, but got " + std::to_string(operands_.size()));
    return false;
  }
  if (func_name_.empty()) {
    context->Print("Function name of funcdef instruction should not be empty");
    return false;
  }
  return true;
}

auto FuncDefInst::Create([[maybe_unused]] context::MIRContext *context, const std::string &func_name,
                         type::Type *return_type, const std::vector<utils::Operand *> &params) -> FuncDefInst * {
  auto *inst = new FuncDefInst(func_name, return_type);
  inst->operands_ = params;  // Set the operands of the instruction to be the parameters of the function
  return inst;
}

auto FuncDefInst::Emit(std::ofstream *out) const -> void {
  *out << "funcdef " << func_name_ << " : " << return_type_->ToString() << "(";
  for (size_t i = 0; i < operands_.size(); ++i) {
    auto operand = operands_[i];
    *out << "param" << i << ": " << operand->GetType()->ToString() << " ";
  }
  *out << ")\n";
}

}  // namespace backend::inst