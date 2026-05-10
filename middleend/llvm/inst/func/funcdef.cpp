#include "middleend/llvm/inst/func/funcdef.h"
#include <unordered_set>
#include "context/compiler_context.h"
#include "middleend/llvm/utils/operand/operand.h"

namespace middleend::llvm::inst {
#include "middleend/checklibrary.ck"
auto FuncDefInst::Create([[maybe_unused]] context::LLVMIRContext *context, const std::string &func_name,
                         type::Type *return_type, std::vector<type::Type *> arg_types,
                         std::vector<utils::Operand *> arg_operands) -> FuncDefInst * {
  // Create a new function definition instruction
  auto *inst = new FuncDefInst(func_name, return_type, std::move(arg_types), std::move(arg_operands));
  return inst;
}

auto FuncDefInst::Create([[maybe_unused]] context::LLVMIRContext *context, const std::string &func_name,
                         type::Type *return_type, std::vector<type::Type *> arg_types) -> FuncDefInst * {
  // Create a new function definition instruction for library functions, which do not have argument operands
  auto *inst = new FuncDefInst(func_name, return_type, std::move(arg_types), {});
  return inst;
}

auto FuncDefInst::Verify(context::CompilerContext *context) const -> bool {
  // Verify that the function definition is valid
  // For example, we can check that the function name is not empty,
  // the return type is valid, and the argument types and operands are valid.
  if (func_name_.empty()) {
    context->Print("Function name cannot be empty");
    return false;
  }
  if (return_type_ == nullptr) {
    context->Print("Return type cannot be null");
    return false;
  }

  // Allow library functions to have no argument operands, but still check the argument types
  if (arg_operands_.empty() && IsLibraryFunction(func_name_)) {
    for (const auto &arg_type : arg_types_) {
      if (arg_type == nullptr) {
        context->Print("Argument type cannot be null");
        return false;
      }
    }
    return true;
  }

  if (arg_types_.size() != arg_operands_.size()) {
    context->Print("Argument types and operands size mismatch");
    return false;
  }
  for (size_t i = 0; i < arg_types_.size(); ++i) {
    if (arg_types_[i] == nullptr) {
      context->Print("Argument type cannot be null");
      return false;
    }
    if (arg_operands_[i] == nullptr) {
      context->Print("Argument operand cannot be null");
      return false;
    }
    // Only allow register operands
    if (!arg_operands_[i]->IsRegister()) {
      context->Print("Argument operand must be a register operand");
      return false;
    }
  }
  return true;
}

auto FuncDefInst::Emit(std::ofstream *out) const -> void {
  // Emit the function definition instruction in LLVM IR format
  *out << "define " << utils::Operand::GetTypeInLLVMIR(return_type_) << " @" << func_name_ << "(";
  for (size_t i = 0; i < arg_types_.size(); ++i) {
    *out << utils::Operand::GetTypeInLLVMIR(arg_types_[i]) << " ";
    arg_operands_[i]->Emit(out);
    if (i != arg_types_.size() - 1) {
      *out << ", ";
    }
  }
  *out << ") {" << std::endl;
  // The function body will be emitted later, so we just emit an empty body here
  *out << "}" << std::endl;
}

}  // namespace middleend::llvm::inst