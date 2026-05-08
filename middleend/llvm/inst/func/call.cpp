#include "middleend/llvm/inst/func/call.h"
#include <utility>
#include "context/compiler_context.h"
#include "middleend/llvm/inst/func/funcdef.h"
#include "middleend/llvm/utils/operand/operand.h"
#include "type/type.h"
namespace middleend::llvm::inst {
auto CallInst::Create(context::LLVMIRContext *context, std::string callee, const std::vector<utils::Operand *> &args,
                      type::Type *return_type) -> CallInst * {
  // Create a new call instruction
  auto *inst = new CallInst();
  inst->callee_ = std::move(callee);
  for (const auto &arg : args) {
    inst->AddOperand(arg);
  }
  inst->return_type_ = return_type;
  if (inst->return_type_->GetKind() != type::TypeKind::VOID) {  // Create register operand for the return value
    auto res = context->GetNewRegOperand(return_type);
    // Set the result operand of the instruction
    inst->SetResult(res);
    res->SetInstruction(inst);
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
  for (size_t i = 0; i < operands_.size(); ++i) {
    *out << utils::Operand::GetTypeInLLVMIR(operands_[i]->GetType()) << " ";
    operands_[i]->Emit(out);
    if (i != operands_.size() - 1) {
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
    context->Print("Callee function is not defined");
    return false;
  }
  if (func_def->GetArgTypes().size() != operands_.size()) {
    // If the number of arguments does not match, report error
    context->Print("Number of arguments of the call instruction does not match the function definition");
    return false;
  }
  for (size_t i = 0; i < operands_.size(); ++i) {
    if (operands_[i]->GetType() == nullptr || operands_[i]->GetType() != func_def->GetArgTypes()[i]) {
      // If the argument type is not compatible, report error
      context->Print("Argument type of the call instruction does not match the function definition");
      return false;
    }
  }

  if (return_type_ == nullptr || return_type_ != func_def->GetReturnType()) {
    // If the return type is not compatible, report error
    context->Print("Return type of the call instruction does not match the function definition");
    return false;
  }

  if (result_ != nullptr && result_->GetInstruction() != this) {
    // If the result operand is not set correctly, report error
    context->Print("Result operand of the call instruction is not set correctly");
    return false;
  }

  return true;
}

}  // namespace middleend::llvm::inst