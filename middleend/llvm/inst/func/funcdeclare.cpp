#include "middleend/llvm/inst/func/funcdeclare.h"

#include <utility>
#include "context/compiler_context.h"

namespace middleend::llvm::inst {
auto FuncDeclareInst::Create([[maybe_unused]] context::LLVMIRContext *context, const std::string &func_name,
                             type::Type *return_type, std::vector<type::Type *> arg_types) -> FuncDeclareInst * {
  auto *inst = new FuncDeclareInst(func_name, return_type, std::move(arg_types));
  return inst;
}

auto FuncDeclareInst::Verify(context::CompilerContext *context) const -> bool {
  if (func_name_.empty()) {
    context->Print("Function name cannot be empty");
    return false;
  }
  if (return_type_ == nullptr) {
    context->Print("Return type cannot be null");
    return false;
  }
  for (const auto &arg_type : arg_types_) {
    if (arg_type == nullptr) {
      context->Print("Argument type cannot be null");
      return false;
    }
  }
  return true;
}

auto FuncDeclareInst::Emit(std::ofstream *out) const -> void {
  // Emit the function declaration instruction in llvm ir format
  *out << "declare " << utils::Operand::GetTypeInLLVMIR(return_type_) << " @" << func_name_ << "(";
  for (size_t i = 0; i < arg_types_.size(); ++i) {
    *out << utils::Operand::GetTypeInLLVMIR(arg_types_[i]);
    if (i != arg_types_.size() - 1) {
      *out << ", ";
    }
  }
  *out << ")\n";
}

}  // namespace middleend::llvm::inst