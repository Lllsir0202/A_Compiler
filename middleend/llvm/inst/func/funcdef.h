#ifndef MIDDLEEND_LLVM_INST_FUNC_FUNCDEF_H
#define MIDDLEEND_LLVM_INST_FUNC_FUNCDEF_H

#include <utility>
#include "middleend/llvm/inst/inst.h"
#include "middleend/llvm/utils/operand/operand.h"
#include "utils/type/type.h"
namespace middleend::llvm::inst {
class FuncDefInst : public Instruction {
 public:
  // Interfaces to get function information
  auto GetFuncName() const -> const std::string & { return func_name_; }
  auto GetReturnType() const -> type::Type * { return return_type_; }

  auto GetArgTypes() const -> const std::vector<type::Type *> & { return arg_types_; }
  auto GetArgOperands() const -> const std::vector<utils::Operand *> & { return arg_operands_; }

  // Override the Emit and Verify functions
  auto Emit(std::ofstream *out) const -> void override;
  auto Verify(context::CompilerContext *context) const -> bool override;

 private:
  explicit FuncDefInst(std::string func_name, type::Type *return_type, std::vector<type::Type *> arg_types,
                       std::vector<utils::Operand *> arg_operands)
      : Instruction(OpKind::FuncDef),
        func_name_(std::move(func_name)),
        return_type_(return_type),
        arg_types_(std::move(arg_types)),
        arg_operands_(std::move(arg_operands)) {}
  static auto Create(context::LLVMIRContext *context, const std::string &func_name, type::Type *return_type,
                     std::vector<type::Type *> arg_types, std::vector<utils::Operand *> arg_operands) -> FuncDefInst *;
  // Function name
  std::string func_name_;
  // Return type of the function
  type::Type *return_type_;
  // Arguments of the function
  /**
   * @note all types and operands should be corresponding
   */
  std::vector<type::Type *> arg_types_;
  std::vector<utils::Operand *> arg_operands_;
};

}  // namespace middleend::llvm::inst

#endif