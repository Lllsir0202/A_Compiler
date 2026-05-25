#ifndef BACKEND_MIR_INST_FUNC_FUNCDECLARE_H
#define BACKEND_MIR_INST_FUNC_FUNCDECLARE_H

#include <string>
#include "backend/mir/inst/inst.h"
#include "utils/type/type.h"
namespace backend::inst {
class FuncDeclareInst : public Instruction {
 public:
  auto Verify(context::CompilerContext *context) const -> bool override;
  auto GetFuncName() const -> const std::string & { return func_name_; }
  auto GetReturnType() const -> type::Type * { return return_type_; }
  auto GetArgTypes() const -> const std::vector<type::Type *> & { return arg_types_; }

 private:
  explicit FuncDeclareInst(std::string func_name, type::Type *return_type, std::vector<type::Type *> arg_types)
      : Instruction(OpKind::FuncDeclare),
        func_name_(std::move(func_name)),
        return_type_(return_type),
        arg_types_(std::move(arg_types)) {}
  static auto Create(context::MIRContext *context, const std::string &func_name, type::Type *return_type,
                     std::vector<type::Type *> arg_types) -> FuncDeclareInst *;
  std::string func_name_;
  // Return type of the function
  type::Type *return_type_;
  // Arguments of the function
  /**
   * @note all types and operands should be corresponding
   */
  std::vector<type::Type *> arg_types_;
  friend class utils::factory::InstFactory;
};

}  // namespace backend::inst

#endif