#include "backend/targets/rv64/inst/immediate/immediate.h"
#include "backend/targets/utils/operand/targetoperand.h"

namespace backend::targetir::rv64 {
// Pesudo instruction, not a real instruction,
// used to load an immediate value into a register
auto RVLIInst::Create(TargetOperand *rd, TargetOperand *imm) -> RVLIInst * {
  auto *inst = new RVLIInst();
  inst->operands_.reserve(1);
  inst->AddOperand(imm);
  inst->SetResult(rd);
  return inst;
}

auto RVLIInst::Emit(std::ofstream *out) -> void {
  *out << "    li ";
  result_->Emit(out);
  *out << ", ";
  operands_[0]->Emit(out);
  *out << '\n';
}

auto RVLUIInst::Create(TargetOperand *rd, TargetOperand *imm) -> RVLUIInst * {
  auto *inst = new RVLUIInst();
  inst->operands_.reserve(1);
  inst->AddOperand(imm);
  inst->SetResult(rd);
  return inst;
}

auto RVLUIInst::Emit(std::ofstream *out) -> void {
  *out << "    lui ";
  result_->Emit(out);
  *out << ", ";
  operands_[0]->Emit(out);
  *out << '\n';
}
}  // namespace backend::targetir::rv64