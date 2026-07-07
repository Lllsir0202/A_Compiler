#ifndef BACKEND_MIR_INST_BRANCH_RET_H
#define BACKEND_MIR_INST_BRANCH_RET_H

#include "backend/mir/inst/inst.h"
namespace backend::inst {
class RetInst : public Instruction {
 public:
  auto Verify(context::CompilerContext *context) const -> bool override;
  auto Emit(std::ofstream *out) const -> void override;
  auto IsTerminal() const -> bool override { return true; }
  // Check if the instruction has a return value
  auto HasReturnValue() const -> bool { return GetOperands().size() == 1; }
  auto GetReturnValue() const -> utils::Operand * {
    if (!HasReturnValue()) {
      return nullptr;
    }
    return GetOperands()[0];
  }

 private:
  explicit RetInst() : Instruction(OpKind::Ret) {}
  static auto Create(context::MIRContext *context, utils::Operand *ret_value) -> RetInst *;
  friend class utils::factory::InstFactory;
};

}  // namespace backend::inst

#endif