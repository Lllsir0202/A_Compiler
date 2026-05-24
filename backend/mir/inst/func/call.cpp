#include "backend/mir/inst/func/call.h"
#include "context/compiler_context.h"
#include "context/mir_context.h"

namespace backend::inst {
auto CallInst::Verify(context::CompilerContext *context) const -> bool {
  auto func_def = context->GetMIRContext()->GetFuncDefByName(callee_);

  if (func_def == nullptr) {
    auto func_declare = context->GetMIRContext()->GetFuncDeclareByName(callee_);
    if (func_declare == nullptr) {
      context->Print("Function " + callee_ + " is not declared");
      return false;
    }
  }
  return true;
}

auto CallInst::Create(context::MIRContext *context, const std::string &callee, type::Type *return_type,
                      const std::vector<utils::Operand *> &params) -> CallInst * {
  auto inst = new CallInst(callee, return_type);
  for (auto param : params) {
    inst->AddOperand(param);
  }
  if (return_type->GetKind() != type::TypeKind::VOID) {
    auto ret_reg = context->GetNewRegOperand(return_type);
    inst->AddOperand(ret_reg);
  }
  return inst;
}

}  // namespace backend::inst