#include "backend/targets/rv64/inst/immediate/immediate.h"
#include "backend/targets/utils/operand/targetoperand.h"

namespace backend::targetir::rv64 {
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

auto RVAUIPCInst::Create(TargetOperand *rd, TargetOperand *imm) -> RVAUIPCInst * {
  auto *inst = new RVAUIPCInst();
  inst->operands_.reserve(1);
  inst->AddOperand(imm);
  inst->SetResult(rd);
  return inst;
}

auto RVAUIPCInst::Emit(std::ofstream *out) -> void {
  *out << "    auipc ";
  result_->Emit(out);
  *out << ", ";
  operands_[0]->Emit(out);
  *out << '\n';
}

}  // namespace backend::targetir::rv64