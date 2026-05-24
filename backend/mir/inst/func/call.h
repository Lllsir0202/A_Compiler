#ifndef BACKEND_MIR_INST_FUNC_CALL_H
#define BACKEND_MIR_INST_FUNC_CALL_H

#include <vector>
#include "backend/mir/inst/inst.h"
#include "backend/mir/utils/operand/operand.h"
#include "type/type.h"
namespace backend::inst {
class CallInst : public Instruction {
 public:
  auto Verify(context::CompilerContext *context) const -> bool override;

 private:
  explicit CallInst(std::string callee, type::Type *return_type)
      : Instruction(OpKind::Call), callee_(std::move(callee)), return_type_(return_type) {}
  static auto Create(context::MIRContext *context, const std::string &callee, type::Type *return_type,
                     const std::vector<utils::Operand *> &params) -> CallInst *;
  friend class utils::factory::InstFactory;
  std::string callee_;
  type::Type *return_type_;
};

}  // namespace backend::inst

#endif