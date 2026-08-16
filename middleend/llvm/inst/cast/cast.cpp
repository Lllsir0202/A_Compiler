#include "middleend/llvm/inst/cast/cast.h"
#include "context/compiler_context.h"
#include "llvm/utils/operand/operand.h"

namespace middleend::llvm::inst {
namespace {
auto EmitIntegerCast(std::ofstream *out, const char *name, utils::Operand *result, utils::Operand *src) -> void {
  *out << "  ";
  result->Emit(out);
  *out << " = " << name << " " << utils::Operand::GetTypeInLLVMIR(src->GetType()) << " ";
  src->Emit(out);
  *out << " to " << utils::Operand::GetTypeInLLVMIR(result->GetType()) << "\n";
}
}  // namespace

auto SextInst::Emit(std::ofstream *out) const -> void { EmitIntegerCast(out, "sext", result_, operands_[0]); }

auto SextInst::Verify(context::CompilerContext *context) const -> bool {
  if (operands_.size() != 1 || operands_[0] == nullptr || !operands_[0]->IsInt32() || result_ == nullptr ||
      !result_->IsLong() || result_->GetInstruction() != this) {
    context->Print("Sext instruction requires an i32 source and i64 result");
    return false;
  }
  return true;
}

auto SextInst::Create(context::LLVMIRContext *context, utils::Operand *src) -> SextInst * {
  auto *inst = new SextInst();
  inst->AddOperand(src);
  auto *res = context->GetNewRegOperand(context->GetLongType());
  inst->SetResult(res);
  res->SetInstruction(inst);
  return inst;
}

auto TruncInst::Emit(std::ofstream *out) const -> void { EmitIntegerCast(out, "trunc", result_, operands_[0]); }

auto TruncInst::Verify(context::CompilerContext *context) const -> bool {
  if (operands_.size() != 1 || operands_[0] == nullptr || !operands_[0]->IsLong() || result_ == nullptr ||
      !result_->IsInt32() || result_->GetInstruction() != this) {
    context->Print("Trunc instruction requires an i64 source and i32 result");
    return false;
  }
  return true;
}

auto TruncInst::Create(context::LLVMIRContext *context, utils::Operand *src) -> TruncInst * {
  auto *inst = new TruncInst();
  inst->AddOperand(src);
  auto *res = context->GetNewRegOperand(context->GetIntegerType());
  inst->SetResult(res);
  res->SetInstruction(inst);
  return inst;
}

auto ZextInst::Emit(std::ofstream *out) const -> void {
  // Emit the instruction in LLVM IR format
  // For example: %result = zext i1 %src to i32
  *out << "  ";
  result_->Emit(out);
  *out << " = zext ";
  *out << utils::Operand::GetTypeInLLVMIR(operands_[0]->GetType());
  *out << " ";
  operands_[0]->Emit(out);
  *out << " to ";
  *out << utils::Operand::GetTypeInLLVMIR(result_->GetType());
  *out << "\n";
}

auto ZextInst::Verify(context::CompilerContext *context) const -> bool {
  // Verify the instruction, check if the source operand is of boolean type
  if (operands_.size() != 1) {
    context->Print("Zext instruction should have exactly one operand");
    return false;
  }
  auto *src = operands_[0];
  if (!src->IsBool()) {
    context->Print("Zext instruction requires a boolean source operand");
    return false;
  }
  if (!result_->IsInt32()) {
    context->Print("Zext instruction requires an integer result operand");
    return false;
  }

  if (result_->GetInstruction() != this) {
    context->Print("Zext instruction's result operand should point back to the instruction");
    return false;
  }

  return true;
}

auto ZextInst::Create(context::LLVMIRContext *context, utils::Operand *src) -> ZextInst * {
  auto *inst = new ZextInst();
  inst->AddOperand(src);
  auto res = context->GetNewRegOperand(context->GetIntegerType());
  inst->SetResult(res);
  res->SetInstruction(inst);
  return inst;
}

auto SitoFpInst::Emit(std::ofstream *out) const -> void {
  // Emit the instruction in LLVM IR format
  // For example: %result = sitofp i32 %src to float
  *out << "  ";
  result_->Emit(out);
  *out << " = sitofp ";
  *out << utils::Operand::GetTypeInLLVMIR(operands_[0]->GetType());
  *out << " ";
  operands_[0]->Emit(out);
  *out << " to ";
  *out << utils::Operand::GetTypeInLLVMIR(result_->GetType());
  *out << "\n";
}

auto SitoFpInst::Verify(context::CompilerContext *context) const -> bool {
  // Verify the instruction, check if the source operand is of integer type
  if (operands_.size() != 1) {
    context->Print("Sitofp instruction should have exactly one operand");
    return false;
  }
  auto *src = operands_[0];
  if (!src->IsInt32()) {
    context->Print("Sitofp instruction requires an integer source operand");
    return false;
  }
  if (!result_->IsFloat()) {
    context->Print("Sitofp instruction requires a float result operand");
    return false;
  }

  if (result_->GetInstruction() != this) {
    context->Print("Sitofp instruction's result operand should point back to the instruction");
    return false;
  }

  return true;
}

auto SitoFpInst::Create(context::LLVMIRContext *context, utils::Operand *src) -> SitoFpInst * {
  auto *inst = new SitoFpInst();
  inst->AddOperand(src);
  auto res = context->GetNewRegOperand(context->GetFloatType());
  inst->SetResult(res);
  res->SetInstruction(inst);
  return inst;
}

auto FptoSiInst::Emit(std::ofstream *out) const -> void {
  // Emit the instruction in LLVM IR format
  // For example: %result = fptosi float %src to i32
  *out << "  ";
  result_->Emit(out);
  *out << " = fptosi ";
  *out << utils::Operand::GetTypeInLLVMIR(operands_[0]->GetType());
  *out << " ";
  operands_[0]->Emit(out);
  *out << " to ";
  *out << utils::Operand::GetTypeInLLVMIR(result_->GetType());
  *out << "\n";
}

auto FptoSiInst::Verify(context::CompilerContext *context) const -> bool {
  // Verify the instruction, check if the source operand is of float type
  if (operands_.size() != 1) {
    context->Print("Fptosi instruction should have exactly one operand");
    return false;
  }
  auto *src = operands_[0];
  if (!src->IsFloat()) {
    context->Print("Fptosi instruction requires a float source operand");
    return false;
  }
  if (!result_->IsInt32()) {
    context->Print("Fptosi instruction requires an integer result operand");
    return false;
  }

  if (result_->GetInstruction() != this) {
    context->Print("Fptosi instruction's result operand should point back to the instruction");
    return false;
  }

  return true;
}

auto FptoSiInst::Create(context::LLVMIRContext *context, utils::Operand *src) -> FptoSiInst * {
  auto *inst = new FptoSiInst();
  inst->AddOperand(src);
  auto res = context->GetNewRegOperand(context->GetIntegerType());
  inst->SetResult(res);
  res->SetInstruction(inst);
  return inst;
}

auto UitoFpInst::Emit(std::ofstream *out) const -> void {
  // Emit the instruction in LLVM IR format
  // For example: %result = uitofp i32 %src to float
  *out << "  ";
  result_->Emit(out);
  *out << " = uitofp ";
  *out << utils::Operand::GetTypeInLLVMIR(operands_[0]->GetType());
  *out << " ";
  operands_[0]->Emit(out);
  *out << " to ";
  *out << utils::Operand::GetTypeInLLVMIR(result_->GetType());
  *out << "\n";
}

auto UitoFpInst::Verify(context::CompilerContext *context) const -> bool {
  // Verify the instruction, check if the source operand is of integer type
  if (operands_.size() != 1) {
    context->Print("Uitofp instruction should have exactly one operand");
    return false;
  }
  auto *src = operands_[0];
  if (!src->IsBool()) {
    context->Print("Uitofp instruction requires a boolean source operand");
    return false;
  }
  if (!result_->IsFloat()) {
    context->Print("Uitofp instruction requires a float result operand");
    return false;
  }

  if (GetParentBlock() == nullptr) {
    context->Print("Uitofp instruction should be in a basic block");
    return false;
  }

  if (result_->GetInstruction() != this) {
    context->Print("Uitofp instruction's result operand should point back to the instruction");
    return false;
  }

  return true;
}

auto UitoFpInst::Create(context::LLVMIRContext *context, utils::Operand *src) -> UitoFpInst * {
  auto *inst = new UitoFpInst();
  inst->AddOperand(src);
  auto res = context->GetNewRegOperand(context->GetFloatType());
  inst->SetResult(res);
  res->SetInstruction(inst);
  return inst;
}

}  // namespace middleend::llvm::inst
