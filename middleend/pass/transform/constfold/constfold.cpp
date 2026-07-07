#include "middleend/pass/transform/constfold/constfold.h"
#include <cstddef>
#include <stdexcept>
#include "context/compiler_context.h"
#include "middleend/llvm/inst/arithm/arithm.h"
#include "middleend/llvm/inst/cast/cast.h"
#include "middleend/llvm/inst/cmp/cmp.h"
#include "middleend/llvm/utils/operand/literaloperand.h"
#include "middleend/llvm/utils/operand/operand.h"
#include "pass/pass.h"

namespace middleend::pass::transform {
namespace {
using middleend::llvm::inst::AddFInst;
using middleend::llvm::inst::AddInst;
using middleend::llvm::inst::CmpPredicate;
using middleend::llvm::inst::DivFInst;
using middleend::llvm::inst::DivInst;
using middleend::llvm::inst::FCmpInst;
using middleend::llvm::inst::FptoSiInst;
using middleend::llvm::inst::ICmpInst;
using middleend::llvm::inst::Instruction;
using middleend::llvm::inst::ModInst;
using middleend::llvm::inst::MulFInst;
using middleend::llvm::inst::MulInst;
using middleend::llvm::inst::SitoFpInst;
using middleend::llvm::inst::SubFInst;
using middleend::llvm::inst::SubInst;
using middleend::llvm::inst::UitoFpInst;
using middleend::llvm::inst::XorInst;
using middleend::llvm::inst::ZextInst;

using middleend::llvm::utils::BoolLiteral;
using middleend::llvm::utils::FloatLiteral;
using middleend::llvm::utils::IntLiteral;
using middleend::llvm::utils::Operand;

auto AsIntLiteral(Operand *operand) -> IntLiteral * { return dynamic_cast<IntLiteral *>(operand); }
auto AsFloatLiteral(Operand *operand) -> FloatLiteral * { return dynamic_cast<FloatLiteral *>(operand); }
auto AsBoolLiteral(Operand *operand) -> BoolLiteral * { return dynamic_cast<BoolLiteral *>(operand); }

auto GetIntValue(Operand *operand) -> int {
  auto *int_literal = AsIntLiteral(operand);
  if (int_literal == nullptr) {
    throw std::logic_error("Operand is not an int literal");
  }
  return int_literal->GetValue();
}

auto GetFloatValue(Operand *operand) -> float {
  auto *float_literal = AsFloatLiteral(operand);
  if (float_literal == nullptr) {
    throw std::logic_error("Operand is not a float literal");
  }
  return float_literal->GetValue();
}

auto GetBoolValue(Operand *operand) -> bool {
  auto *bool_literal = AsBoolLiteral(operand);
  if (bool_literal == nullptr) {
    throw std::logic_error("Operand is not a bool literal");
  }
  return bool_literal->GetValue();
}
}  // namespace

auto ConstFoldPass::Run() -> void {
  for (auto *function : compiler_context_->GetIRContext()->GetLLVMIRContext()->GetModule()->GetFunctions()) {
    FoldConstantsInFunction(function);
  }
}

auto ConstFoldPass::FoldConstantsInFunction(llvm::utils::Function *function) -> void {
  for (auto *basic_block : function->GetBasicBlocks()) {
    for (auto *inst : basic_block->GetInstructions()) {
      auto *literal_result = TryComputeConstantResult(inst);
      if (literal_result == nullptr) {
        continue;
      }
      // If it is a literal result, we can replace all users of the instruction with the literal result
      inst->ReplaceAllUsersWith(literal_result);
      // Then we can remove the instruction from the basic block
      basic_block->RemoveInst(inst);
    }
  }
}

auto ConstFoldPass::TryComputeConstantResult(Instruction *inst) -> Operand * {
  if (inst == nullptr || inst->GetResult() == nullptr) {
    return nullptr;
  }

  if (auto *result = ComputeArithmResult(inst)) {
    return result;
  }
  if (auto *result = ComputeCmpResult(inst)) {
    return result;
  }
  if (auto *result = ComputeCastResult(inst)) {
    return result;
  }
  return nullptr;
}

auto ConstFoldPass::ComputeArithmResult(Instruction *inst) -> Operand * {
  auto *llvmir_context = compiler_context_->GetIRContext()->GetLLVMIRContext();

  if (auto *add = middleend::llvm::inst::DynamicOrNull<AddInst>(inst)) {
    if (!add->AreAllOperandsConstants()) {
      return nullptr;
    }
    return llvmir_context->GetIntLiteral(GetIntValue(add->GetLHS()) + GetIntValue(add->GetRHS()));
  }

  if (auto *addf = middleend::llvm::inst::DynamicOrNull<AddFInst>(inst)) {
    if (!addf->AreAllOperandsConstants()) {
      return nullptr;
    }
    return llvmir_context->GetFloatLiteral(GetFloatValue(addf->GetLHS()) + GetFloatValue(addf->GetRHS()));
  }

  if (auto *sub = middleend::llvm::inst::DynamicOrNull<SubInst>(inst)) {
    if (!sub->AreAllOperandsConstants()) {
      return nullptr;
    }
    return llvmir_context->GetIntLiteral(GetIntValue(sub->GetLHS()) - GetIntValue(sub->GetRHS()));
  }

  if (auto *subf = middleend::llvm::inst::DynamicOrNull<SubFInst>(inst)) {
    if (!subf->AreAllOperandsConstants()) {
      return nullptr;
    }
    return llvmir_context->GetFloatLiteral(GetFloatValue(subf->GetLHS()) - GetFloatValue(subf->GetRHS()));
  }

  if (auto *mul = middleend::llvm::inst::DynamicOrNull<MulInst>(inst)) {
    if (!mul->AreAllOperandsConstants()) {
      return nullptr;
    }
    return llvmir_context->GetIntLiteral(GetIntValue(mul->GetLHS()) * GetIntValue(mul->GetRHS()));
  }

  if (auto *mulf = middleend::llvm::inst::DynamicOrNull<MulFInst>(inst)) {
    if (!mulf->AreAllOperandsConstants()) {
      return nullptr;
    }
    return llvmir_context->GetFloatLiteral(GetFloatValue(mulf->GetLHS()) * GetFloatValue(mulf->GetRHS()));
  }

  if (auto *div = middleend::llvm::inst::DynamicOrNull<DivInst>(inst)) {
    if (!div->AreAllOperandsConstants()) {
      return nullptr;
    }
    auto rhs = GetIntValue(div->GetRHS());
    if (rhs == 0) {
      // If the divisor is zero, we cannot compute the result,
      // return nullptr to indicate that.
      return nullptr;
    }
    return llvmir_context->GetIntLiteral(GetIntValue(div->GetLHS()) / rhs);
  }

  if (auto *divf = middleend::llvm::inst::DynamicOrNull<DivFInst>(inst)) {
    if (!divf->AreAllOperandsConstants()) {
      return nullptr;
    }
    auto rhs = GetFloatValue(divf->GetRHS());
    if (rhs == 0.0F) {
      // If the divisor is zero, we cannot compute the result,
      // return nullptr to indicate that.
      return nullptr;
    }
    return llvmir_context->GetFloatLiteral(GetFloatValue(divf->GetLHS()) / rhs);
  }

  if (auto *mod = middleend::llvm::inst::DynamicOrNull<ModInst>(inst)) {
    if (mod->AreAllOperandsConstants()) {
      auto rhs = GetIntValue(mod->GetRHS());
      return llvmir_context->GetIntLiteral(GetIntValue(mod->GetLHS()) % rhs);
    }
    return nullptr;
  }

  if (auto *xor_inst = middleend::llvm::inst::DynamicOrNull<XorInst>(inst)) {
    if (!xor_inst->AreAllOperandsConstants()) {
      return nullptr;
    }
    if (xor_inst->GetResult()->IsBool()) {
      return llvmir_context->GetBoolLiteral(GetBoolValue(xor_inst->GetLHS()) ^ GetBoolValue(xor_inst->GetRHS()));
    }
    if (xor_inst->GetResult()->IsInt32()) {
      return llvmir_context->GetIntLiteral(GetIntValue(xor_inst->GetLHS()) ^ GetIntValue(xor_inst->GetRHS()));
    }
  }
  return nullptr;
}

auto ConstFoldPass::ComputeCmpResult(Instruction *inst) -> Operand * {
  auto *llvmir_context = compiler_context_->GetIRContext()->GetLLVMIRContext();

  if (auto *icmp = middleend::llvm::inst::DynamicOrNull<ICmpInst>(inst)) {
    if (!icmp->AreAllOperandsConstants()) {
      return nullptr;
    }
    auto lhs = GetIntValue(icmp->GetLHS());
    auto rhs = GetIntValue(icmp->GetRHS());
    bool result = false;
    switch (icmp->GetPredicate()) {
      case CmpPredicate::EQ:
        result = lhs == rhs;
        break;
      case CmpPredicate::NE:
        result = lhs != rhs;
        break;
      case CmpPredicate::LT:
        result = lhs < rhs;
        break;
      case CmpPredicate::LE:
        result = lhs <= rhs;
        break;
      case CmpPredicate::GT:
        result = lhs > rhs;
        break;
      case CmpPredicate::GE:
        result = lhs >= rhs;
        break;
      default:
        return nullptr;
    }
    return llvmir_context->GetBoolLiteral(result);
  }

  if (auto *fcmp = middleend::llvm::inst::DynamicOrNull<FCmpInst>(inst)) {
    if (!fcmp->AreAllOperandsConstants()) {
      return nullptr;
    }
    auto lhs = GetFloatValue(fcmp->GetLHS());
    auto rhs = GetFloatValue(fcmp->GetRHS());
    bool result = false;
    switch (fcmp->GetPredicate()) {
      case CmpPredicate::EQ:
        result = lhs == rhs;
        break;
      case CmpPredicate::NE:
        result = lhs != rhs;
        break;
      case CmpPredicate::LT:
        result = lhs < rhs;
        break;
      case CmpPredicate::LE:
        result = lhs <= rhs;
        break;
      case CmpPredicate::GT:
        result = lhs > rhs;
        break;
      case CmpPredicate::GE:
        result = lhs >= rhs;
        break;
      default:
        return nullptr;
    }
    return llvmir_context->GetBoolLiteral(result);
  }

  return nullptr;
}

auto ConstFoldPass::ComputeCastResult(Instruction *inst) -> Operand * {
  auto *llvmir_context = compiler_context_->GetIRContext()->GetLLVMIRContext();

  if (auto *zext = middleend::llvm::inst::DynamicOrNull<ZextInst>(inst)) {
    if (zext->AreAllOperandsConstants()) {
      return llvmir_context->GetIntLiteral(GetBoolValue(zext->GetSrc()) ? 1 : 0);
    }
  }

  if (auto *sitofp = middleend::llvm::inst::DynamicOrNull<SitoFpInst>(inst)) {
    if (sitofp->AreAllOperandsConstants()) {
      return llvmir_context->GetFloatLiteral(static_cast<float>(GetIntValue(sitofp->GetSrc())));
    }
  }

  if (auto *fptosi = middleend::llvm::inst::DynamicOrNull<FptoSiInst>(inst)) {
    if (fptosi->AreAllOperandsConstants()) {
      return llvmir_context->GetIntLiteral(static_cast<int>(GetFloatValue(fptosi->GetSrc())));
    }
  }

  if (auto *uitofp = middleend::llvm::inst::DynamicOrNull<UitoFpInst>(inst)) {
    if (uitofp->AreAllOperandsConstants()) {
      return llvmir_context->GetFloatLiteral(GetBoolValue(uitofp->GetSrc()) ? 1.0F : 0.0F);
    }
  }

  return nullptr;
}
}  // namespace middleend::pass::transform
