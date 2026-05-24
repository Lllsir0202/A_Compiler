#include "backend/mir/inst/func/funcdeclare.h"

#include <utility>
#include "context/compiler_context.h"

namespace backend::inst {
auto FuncDeclareInst::Verify(context::CompilerContext *context) const -> bool {
  // The funcdeclare instruction should have no operand
  if (!operands_.empty()) {
    context->Print("Operand size of funcdeclare instruction should be 0, but got " + std::to_string(operands_.size()));
    return false;
  }
  if (func_name_.empty()) {
    context->Print("Function name of funcdeclare instruction should not be empty");
    return false;
  }
  return true;
}

auto FuncDeclareInst::Create([[maybe_unused]] context::MIRContext *context, const std::string &func_name,
                             type::Type *return_type, std::vector<type::Type *> arg_types) -> FuncDeclareInst * {
  return new FuncDeclareInst(func_name, return_type, std::move(arg_types));
}

}  // namespace backend::inst