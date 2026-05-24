#ifndef BACKEND_MIR_INST_PSEUDO_PHI_H
#define BACKEND_MIR_INST_PSEUDO_PHI_H

#include "backend/mir/inst/inst.h"
#include "utils/type/type.h"
namespace backend::inst {
class PhiInst : public Instruction {
 public:
  auto Verify(context::CompilerContext *context) const -> bool override;

 private:
  explicit PhiInst() : Instruction(OpKind::Phi) {}
  static auto Create(context::MIRContext *context, type::Type *result_type,
                     const std::vector<utils::Operand *> &operands,
                     std::vector<utils::Block *> incoming_blocks) -> PhiInst *;
  friend class utils::factory::InstFactory;
  std::vector<utils::Block *> incoming_blocks_;
};

}  // namespace backend::inst

#endif