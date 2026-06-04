#include "backend/mir/inst/cast/cast.h"
#include "backend/mir/utils/operand/operand.h"
#include "context/compiler_context.h"

namespace backend::inst {
auto ZextInst::Verify(context::CompilerContext *context) const -> bool {
  // The zext instruction should have exactly one operand, which is the source operand
  if (operands_.size() != 1) {
    context->Print("Operand size of zext instruction should be 1, but got " + std::to_string(operands_.size()));
    return false;
  }
  if (!operands_[0]->IsRegister() && !operands_[0]->IsLiteral()) {
    context->Print("Operand of zext instruction should be a register or a literal");
    return false;
  }
  return true;
}

auto ZextInst::Create(context::MIRContext *context, utils::Operand *src) -> ZextInst * {
  auto *inst = new ZextInst();
  inst->AddOperand(src);
  auto res = context->GetNewRegOperand(context->GetIntegerType());
  inst->SetResult(res);
  res->SetDefInst(inst);
  return inst;
}

auto ZextInst::Emit(std::ofstream *out) const -> void {
  *out << "  ";
  result_->Emit(out);
  *out << " = zext ";
  operands_[0]->Emit(out);
  *out << "\n";
}

auto Int2FloatInst::Verify(context::CompilerContext *context) const -> bool {
  // The int2float instruction should have exactly one operand, which is the source operand
  if (operands_.size() != 1) {
    context->Print("Operand size of int2float instruction should be 1, but got " + std::to_string(operands_.size()));
    return false;
  }
  if (!operands_[0]->IsRegister() && !operands_[0]->IsLiteral()) {
    context->Print("Operand of int2float instruction should be a register or a literal");
    return false;
  }
  return true;
}

auto Int2FloatInst::Create(context::MIRContext *context, utils::Operand *src) -> Int2FloatInst * {
  auto *inst = new Int2FloatInst();
  inst->AddOperand(src);
  auto res = context->GetNewRegOperand(context->GetFloatType());
  inst->SetResult(res);
  res->SetDefInst(inst);
  return inst;
}

auto Int2FloatInst::Emit(std::ofstream *out) const -> void {
  *out << "  ";
  result_->Emit(out);
  *out << " = int2float ";
  operands_[0]->Emit(out);
  *out << "\n";
}

auto Float2IntInst::Verify(context::CompilerContext *context) const -> bool {
  // The float2int instruction should have exactly one operand, which is the source operand
  if (operands_.size() != 1) {
    context->Print("Operand size of float2int instruction should be 1, but got " + std::to_string(operands_.size()));
    return false;
  }
  if (!operands_[0]->IsRegister() && !operands_[0]->IsLiteral()) {
    context->Print("Operand of float2int instruction should be a register or a literal");
    return false;
  }
  return true;
}

auto Float2IntInst::Create(context::MIRContext *context, utils::Operand *src) -> Float2IntInst * {
  auto *inst = new Float2IntInst();
  inst->AddOperand(src);
  auto res = context->GetNewRegOperand(context->GetIntegerType());
  inst->SetResult(res);
  res->SetDefInst(inst);

  return inst;
}

auto Float2IntInst::Emit(std::ofstream *out) const -> void {
  *out << "  ";
  result_->Emit(out);
  *out << " = float2int ";
  operands_[0]->Emit(out);
  *out << "\n";
}

auto Bool2FloatInst::Verify(context::CompilerContext *context) const -> bool {
  // The bool2float instruction should have exactly one operand, which is the source operand
  if (operands_.size() != 1) {
    context->Print("Operand size of bool2float instruction should be 1, but got " + std::to_string(operands_.size()));
    return false;
  }
  if (!operands_[0]->IsRegister() && !operands_[0]->IsLiteral()) {
    context->Print("Operand of bool2float instruction should be a register or a literal");
    return false;
  }
  return true;
}

auto Bool2FloatInst::Create(context::MIRContext *context, utils::Operand *src) -> Bool2FloatInst * {
  auto *inst = new Bool2FloatInst();
  inst->AddOperand(src);
  auto res = context->GetNewRegOperand(context->GetFloatType());
  inst->SetResult(res);
  res->SetDefInst(inst);
  return inst;
}

auto Bool2FloatInst::Emit(std::ofstream *out) const -> void {
  *out << "  ";
  result_->Emit(out);
  *out << " = bool2float ";
  operands_[0]->Emit(out);
  *out << "\n";
}

}  // namespace backend::inst