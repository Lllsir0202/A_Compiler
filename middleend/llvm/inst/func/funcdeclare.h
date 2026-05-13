#ifndef MIDDLEEND_LLVM_INST_FUNC_FUNCDECLARE_H
#define MIDDLEEND_LLVM_INST_FUNC_FUNCDECLARE_H
/**
 * @brief This file defines the function declaration instruction,
 * in llvm, it represents the library function as well as some other
 * helper functions
 */
#include "middleend/llvm/inst/inst.h"
#include "utils/type/type.h"

namespace middleend::llvm::inst {
class FuncDeclareInst : public Instruction {
 public:
  // Interfaces to get function information
  auto GetFuncName() const -> const std::string & { return func_name_; }
  auto GetReturnType() const -> type::Type * { return return_type_; }

  auto GetArgTypes() const -> const std::vector<type::Type *> & { return arg_types_; }

  // Override the Emit and Verify functions
  auto Emit(std::ofstream *out) const -> void override;
  auto Verify(context::CompilerContext *context) const -> bool override;

 private:
  explicit FuncDeclareInst(std::string func_name, type::Type *return_type, std::vector<type::Type *> arg_types)
      : Instruction(OpKind::FuncDeclare),
        func_name_(std::move(func_name)),
        return_type_(return_type),
        arg_types_(std::move(arg_types)) {}
  static auto Create(context::LLVMIRContext *context, const std::string &func_name, type::Type *return_type,
                     std::vector<type::Type *> arg_types) -> FuncDeclareInst *;

  friend class utils::factory::InstFactory;

  // Function name
  std::string func_name_;
  // Return type of the function
  type::Type *return_type_;
  // Arguments of the function
  /**
   * @note all types and operands should be corresponding
   */
  std::vector<type::Type *> arg_types_;
};

}  // namespace middleend::llvm::inst

#endif