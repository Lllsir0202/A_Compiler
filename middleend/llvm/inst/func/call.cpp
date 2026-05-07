#include "middleend/llvm/inst/func/call.h"
#include <utility>
#include "context/compiler_context.h"
#include "middleend/llvm/utils/operand/operand.h"
#include "type/type.h"
namespace middleend::llvm::inst {
auto CallInst::Create(context::LLVMIRContext *context, std::string callee, const std::vector<utils::Operand *> &args,
                      type::Type *return_type) -> CallInst * {
  // Create a new call instruction
  auto *inst = new CallInst();
  inst->callee_ = std::move(callee);
  inst->args_ = args;
  inst->return_type_ = return_type;
  if (inst->return_type_->GetKind() != type::TypeKind::VOID) {  // Create register operand for the return value
    auto res = context->GetNewRegOperand(return_type);
    // Set the result operand of the instruction
    inst->SetResult(res);
  }
  return inst;
}

auto CallInst::Emit(std::ofstream *out) const -> void {
  // Emit the instruction in llvm ir format
  *out << " ";
  if (return_type_->GetKind() != type::TypeKind::VOID) {
    GetResult()->Emit(out);
    *out << " = ";
  }
  *out << "call " << utils::Operand::GetTypeInLLVMIR(return_type_) << " @" << callee_ << "(";
  for (size_t i = 0; i < args_.size(); ++i) {
    *out << utils::Operand::GetTypeInLLVMIR(args_[i]->GetType()) << " ";
    args_[i]->Emit(out);
    if (i != args_.size() - 1) {
      *out << ", ";
    }
  }
  *out << ")\n";
}

auto CallInst::Verify(context::CompilerContext *context) const -> bool {
  // Verify the instruction, check if the callee function is defined and the argument types match
  auto func_def = context->GetIRContext()->GetLLVMIRContext()->GetResourceContext()->GetFunctionDef(callee_);
  if (func_def == nullptr) {
    // If the callee function is not defined, report error
    return false;
  }
  if (func_def->GetArgTypes().size() != args_.size()) {
    // If the number of arguments does not match, report error
    return false;
  }
  for (size_t i = 0; i < args_.size(); ++i) {
    if (args_[i]->GetType() == nullptr || args_[i]->GetType() != func_def->GetArgTypes()[i]) {
      // If the argument type is not compatible, report error
      return false;
    }
  }
  return true;
}

}  // namespace middleend::llvm::inst