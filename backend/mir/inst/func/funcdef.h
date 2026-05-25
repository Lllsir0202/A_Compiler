#ifndef BACKEND_MIR_INST_FUNC_FUNCDEF_H
#define BACKEND_MIR_INST_FUNC_FUNCDEF_H

#include <string>
#include <utility>
#include "backend/mir/inst/inst.h"
#include "utils/type/type.h"
namespace backend::inst {
class FuncDefInst : public Instruction {
 public:
  auto Verify(context::CompilerContext *context) const -> bool override;
  auto GetFuncName() const -> const std::string & { return func_name_; }
  auto GetReturnType() const -> type::Type * { return return_type_; }

 private:
  explicit FuncDefInst(std::string func_name, type::Type *return_type)
      : Instruction(OpKind::FuncDef), func_name_(std::move(func_name)), return_type_(return_type) {}
  static auto Create(context::MIRContext *context, const std::string &func_name,
                     type::Type *return_type) -> FuncDefInst *;

  std::string func_name_;
  type::Type *return_type_;
  friend class utils::factory::InstFactory;
};

}  // namespace backend::inst

#endif