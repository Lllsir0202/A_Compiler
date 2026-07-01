#include "backend/targets/rv64/lower/lowerhelper.h"
#include <stdexcept>
#include "backend/mir/inst/inst.h"
#include "backend/targets/rv64/inst/arithmetic/arithmetic.h"
#include "backend/targets/rv64/inst/pseudo/pseudo.h"
#include "backend/targets/rv64/utils/checker.h"
#include "backend/targets/utils/operand/targetliteral.h"
#include "backend/targets/utils/operand/targetoperand.h"
#include "context/compiler_context.h"

#define GET_BACKEND_TARGET() compiler_context_->GetBackendContext()
namespace backend::targetir::rv64 {

auto LowerHelper::LowerDispatch(backend::inst::Instruction *inst) -> void {
  if (inst == nullptr) {
    throw std::invalid_argument("RV64 lower received a null MIR instruction");
  }

#define DISPATCH_MIR_INST(MIR_INST, LOWER_METHOD)           \
  if (auto *casted = inst::DynamicOrNull<MIR_INST>(inst)) { \
    LOWER_METHOD(casted);                                   \
    return;                                                 \
  }

  DISPATCH_MIR_INST(backend::inst::AddInst, LowerAdd)
  DISPATCH_MIR_INST(backend::inst::SubInst, LowerSub)
  DISPATCH_MIR_INST(backend::inst::MulInst, LowerMul)
  DISPATCH_MIR_INST(backend::inst::DivInst, LowerDiv)
  DISPATCH_MIR_INST(backend::inst::ModInst, LowerMod)

  DISPATCH_MIR_INST(backend::inst::AddFInst, LowerAddF)
  DISPATCH_MIR_INST(backend::inst::SubFInst, LowerSubF)
  DISPATCH_MIR_INST(backend::inst::MulFInst, LowerMulF)
  DISPATCH_MIR_INST(backend::inst::DivFInst, LowerDivF)

  DISPATCH_MIR_INST(backend::inst::ICmpInst, LowerICmp)
  DISPATCH_MIR_INST(backend::inst::FCmpInst, LowerFCmp)
  DISPATCH_MIR_INST(backend::inst::XorInst, LowerXor)

  DISPATCH_MIR_INST(backend::inst::LoadInst, LowerLoad)
  DISPATCH_MIR_INST(backend::inst::StoreInst, LowerStore)
  DISPATCH_MIR_INST(backend::inst::GlobalDefInst, LowerGlobalDef)

  DISPATCH_MIR_INST(backend::inst::BrInst, LowerBr)
  DISPATCH_MIR_INST(backend::inst::BrCondInst, LowerCondBr)
  DISPATCH_MIR_INST(backend::inst::RetInst, LowerRet)

  DISPATCH_MIR_INST(backend::inst::FuncDefInst, LowerFuncDef)
  DISPATCH_MIR_INST(backend::inst::FuncDeclareInst, LowerFuncDeclare)
  DISPATCH_MIR_INST(backend::inst::CallInst, LowerCall)

  DISPATCH_MIR_INST(backend::inst::ZextInst, LowerZext)
  DISPATCH_MIR_INST(backend::inst::Int2FloatInst, LowerInt2Float)
  DISPATCH_MIR_INST(backend::inst::Float2IntInst, LowerFloat2Int)
  DISPATCH_MIR_INST(backend::inst::Bool2FloatInst, LowerBool2Float)

  DISPATCH_MIR_INST(backend::inst::CopyInst, LowerCopy)
  DISPATCH_MIR_INST(backend::inst::PhiInst, LowerPhi)

#undef DISPATCH_MIR_INST
  throw std::runtime_error("Unsupported MIR instruction in RV64 lower dispatch");
}

// Lowering arithmetic instructions
auto LowerHelper::LowerAdd(backend::inst::AddInst *inst) -> void {
  auto *lhs = inst->GetLHS();
  auto *rhs = inst->GetRHS();
  auto *dst = inst->GetResult();

  // If all operands are literals, we can compute the result as a literal and avoid generating any instructions
  if (lhs->IsLiteral() && rhs->IsLiteral()) {
    // And they should both be integer literals
    auto *lhs_literal = static_cast<utils::IntLiteral *>(lhs);
    auto *rhs_literal = static_cast<utils::IntLiteral *>(rhs);

    // Compute the result and set it as a literal
    auto result_value = lhs_literal->GetValue() + rhs_literal->GetValue();
    auto *result_literal = GET_BACKEND_TARGET()->CreateIntLiteral(result_value);
    // Map the result to result literal
    GET_BACKEND_TARGET()->MapOperand(dst, result_literal);
    return;
  }
  // If one of the operands is a literal, we can use an immediate instruction if the literal fits in 12 bits
  if (lhs->IsLiteral() || rhs->IsLiteral()) {
    auto literal_operand = lhs->IsLiteral() ? lhs : rhs;
    auto non_literal_operand = lhs->IsLiteral() ? rhs : lhs;

    // Check if the literal fits in 12 bits
    auto literal = static_cast<utils::IntLiteral *>(literal_operand);
    if (Is12BitSignedInteger(literal->GetValue())) {
      // Use an immediate instruction
      // Create an immediate instruction and map the result
      // the result operand should be a virtual register and the size should be 32bits
      auto target_result = GET_BACKEND_TARGET()->CreateVirtualRegister(TargetRegSize::DWord, TargetRegClass::General);
      // Map the result operand to the target result
      GET_BACKEND_TARGET()->MapOperand(dst, target_result);
      // Create an addi instruction
      // Create the operands first
      auto target_literal = GET_BACKEND_TARGET()->CreateIntLiteral(literal->GetValue());
      auto target_non_literal = GET_BACKEND_TARGET()->GetMappedOperand(non_literal_operand);
      if (target_non_literal == nullptr) {
        throw std::runtime_error("Non-literal operand is not mapped to a target operand");
      }
      // Create the addi instruction now
      auto addi_inst =
          GET_BACKEND_TARGET()->CreateInstruction<RVADDIInst>(target_result, target_non_literal, target_literal);
      // Emplace back this instruction to the current basic block
      GET_BACKEND_TARGET()->GetCurrentBlock()->AddInst(addi_inst);
    } else {
      // If can not be held on a 12-bit signed integer, need to load it into a register via 'li' instruction
      auto target_result = GET_BACKEND_TARGET()->CreateVirtualRegister(TargetRegSize::DWord, TargetRegClass::General);
      // Map the result operand to the target result
      GET_BACKEND_TARGET()->MapOperand(dst, target_result);
      // Create a li instruction to load the literal into the target result register
      auto target_literal = GET_BACKEND_TARGET()->CreateIntLiteral(literal->GetValue());
      auto li_inst = GET_BACKEND_TARGET()->CreateInstruction<RVLIInst>(target_result, target_literal);
      // Emplace back this instruction to the current basic block
      GET_BACKEND_TARGET()->GetCurrentBlock()->AddInst(li_inst);
      // Then get the li result and add it to the non-literal operand via an add instruction
      auto target_non_literal = GET_BACKEND_TARGET()->GetMappedOperand(non_literal_operand);
      if (target_non_literal == nullptr) {
        throw std::runtime_error("Non-literal operand is not mapped to a target operand");
      }

      // Create an add instruction to add the li result and the non-literal operand
      auto add_inst =
          GET_BACKEND_TARGET()->CreateInstruction<RVADDInst>(target_result, target_non_literal, target_result);
      GET_BACKEND_TARGET()->GetCurrentBlock()->AddInst(add_inst);
    }
    return;
  }

  /**
   * @brief If both operands are not literals, we need to consider more complicated cases.
   * For address calculation, it will be handled in the following senario:
   * 1. Function call with a pointer argument
   * 2. Load/Store with a pointer operand
   * Currently, we have not implemented the address calculation for pointer arithmetic calcualtion,
   * so it would be much simpler.
   */
  // We can just use a normal add instruction to add the two operands
  auto target_lhs = GET_BACKEND_TARGET()->GetMappedOperand(lhs);
  auto target_rhs = GET_BACKEND_TARGET()->GetMappedOperand(rhs);
  if (target_lhs == nullptr || target_rhs == nullptr) {
    throw std::runtime_error("One of the operands is not mapped to a target operand");
  }

  // It could always be two virtual registers,
  // but we need to check the size of the operands and make sure they are the same
  if (!target_lhs->IsVirtualReg() || !target_rhs->IsVirtualReg()) {
    throw std::runtime_error("One of the operands is not a virtual register");
  }
  auto target_lhs_vreg = static_cast<TargetVirtualReg *>(target_lhs);
  auto target_rhs_vreg = static_cast<TargetVirtualReg *>(target_rhs);
  if (target_lhs_vreg->GetRegSize() != target_rhs_vreg->GetRegSize()) {
    throw std::runtime_error("The two operands have different sizes");
  }
  // Create a target result virtual register
  auto target_result =
      GET_BACKEND_TARGET()->CreateVirtualRegister(target_lhs_vreg->GetRegSize(), TargetRegClass::General);
  // Map the result operand to the target result
  GET_BACKEND_TARGET()->MapOperand(dst, target_result);
  // Create an add instruction to add the two operands
  auto add_inst = GET_BACKEND_TARGET()->CreateInstruction<RVADDInst>(target_result, target_lhs, target_rhs);
  GET_BACKEND_TARGET()->GetCurrentBlock()->AddInst(add_inst);
}
auto LowerHelper::LowerSub([[maybe_unused]] backend::inst::SubInst *inst) -> void {}
auto LowerHelper::LowerMul([[maybe_unused]] backend::inst::MulInst *inst) -> void {}
auto LowerHelper::LowerDiv([[maybe_unused]] backend::inst::DivInst *inst) -> void {}
auto LowerHelper::LowerMod([[maybe_unused]] backend::inst::ModInst *inst) -> void {}

auto LowerHelper::LowerAddF([[maybe_unused]] backend::inst::AddFInst *inst) -> void {}
auto LowerHelper::LowerSubF([[maybe_unused]] backend::inst::SubFInst *inst) -> void {}
auto LowerHelper::LowerMulF([[maybe_unused]] backend::inst::MulFInst *inst) -> void {}
auto LowerHelper::LowerDivF([[maybe_unused]] backend::inst::DivFInst *inst) -> void {}

auto LowerHelper::LowerICmp([[maybe_unused]] backend::inst::ICmpInst *inst) -> void {}
auto LowerHelper::LowerFCmp([[maybe_unused]] backend::inst::FCmpInst *inst) -> void {}
auto LowerHelper::LowerXor([[maybe_unused]] backend::inst::XorInst *inst) -> void {}

auto LowerHelper::LowerLoad([[maybe_unused]] backend::inst::LoadInst *inst) -> void {}
auto LowerHelper::LowerStore([[maybe_unused]] backend::inst::StoreInst *inst) -> void {}
auto LowerHelper::LowerGlobalDef([[maybe_unused]] backend::inst::GlobalDefInst *inst) -> void {}

auto LowerHelper::LowerBr([[maybe_unused]] backend::inst::BrInst *inst) -> void {}
auto LowerHelper::LowerCondBr([[maybe_unused]] backend::inst::BrCondInst *inst) -> void {}
auto LowerHelper::LowerRet([[maybe_unused]] backend::inst::RetInst *inst) -> void {}

auto LowerHelper::LowerFuncDef([[maybe_unused]] backend::inst::FuncDefInst *inst) -> void {}
auto LowerHelper::LowerFuncDeclare([[maybe_unused]] backend::inst::FuncDeclareInst *inst) -> void {}
auto LowerHelper::LowerCall([[maybe_unused]] backend::inst::CallInst *inst) -> void {}

auto LowerHelper::LowerZext([[maybe_unused]] backend::inst::ZextInst *inst) -> void {}
auto LowerHelper::LowerInt2Float([[maybe_unused]] backend::inst::Int2FloatInst *inst) -> void {}
auto LowerHelper::LowerFloat2Int([[maybe_unused]] backend::inst::Float2IntInst *inst) -> void {}
auto LowerHelper::LowerBool2Float([[maybe_unused]] backend::inst::Bool2FloatInst *inst) -> void {}

auto LowerHelper::LowerCopy([[maybe_unused]] backend::inst::CopyInst *inst) -> void {}
auto LowerHelper::LowerPhi([[maybe_unused]] backend::inst::PhiInst *inst) -> void {}

}  // namespace backend::targetir::rv64
