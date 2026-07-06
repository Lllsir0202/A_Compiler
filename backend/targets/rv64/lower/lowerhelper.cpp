#include "backend/targets/rv64/lower/lowerhelper.h"
#include <stdexcept>
#include "backend/mir/inst/inst.h"
#include "backend/targets/rv64/inst/arithmetic/arithmetic.h"
#include "backend/targets/rv64/inst/pseudo/pseudo.h"
#include "backend/targets/rv64/utils/checker.h"
#include "backend/targets/utils/operand/targetoperand.h"
#include "context/compiler_context.h"
#include "mir/utils/operand/literaloperand.h"

#define GET_BACKEND_TARGET() compiler_context_->GetBackendContext()
namespace backend::targetir::rv64 {
/**
 * @note Need to map all literal operands which type is int and value is zero
 * to a special zero register, which is x0 in RV64.
 * In ra(graph coloring), it will be regarded as pre-colored, and will not be allocated to any physical register.
 */

auto LowerHelper::LowerDispatch(backend::inst::Instruction *inst) -> void {
  if (inst == nullptr) {
    throw std::invalid_argument("RV64 lower received a null MIR instruction");
  }

#define DISPATCH_MIR_INST(MIR_INST, LOWER_METHOD)           \
  if (auto *casted = inst::DynamicOrNull<MIR_INST>(inst)) { \
    LOWER_METHOD(casted);                                   \
    return;                                                 \
  }

  try {
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
  } catch (const std::exception &e) {
    throw std::runtime_error(std::string("Error lowering instruction: ") + e.what());
  }
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
    // Should never happen, throw an exception for debugging purposes
    compiler_context_->ReportError("Both operands are literals, should have been constant folded");
    return;
  }
  // If one of the operands is a literal, we can use an immediate instruction if the literal fits in 12 bits
  if (lhs->IsLiteral() || rhs->IsLiteral()) {
    auto literal_operand = lhs->IsLiteral() ? lhs : rhs;
    auto non_literal_operand = lhs->IsLiteral() ? rhs : lhs;

    // Check if the literal fits in 12 bits
    auto literal = static_cast<utils::IntLiteral *>(literal_operand);
    // If the literal is 0, then we can just reuse the register
    if (literal->GetValue() == 0) {
      // If it is zero, we can just map the non-literal operand to the result and return
      auto target_non_literal = GET_BACKEND_TARGET()->GetMappedOperand(non_literal_operand);
      GET_BACKEND_TARGET()->MapOperand(dst, target_non_literal);
      return;
    }
    // Otherwise, we need to check if it fits in 12 bits
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
        compiler_context_->ReportError("Non-literal operand is not mapped to a target operand");
        return;
      }
      // Create the addi instruction now
      auto addi_inst =
          GET_BACKEND_TARGET()->CreateInstruction<RVADDIWInst>(target_result, target_non_literal, target_literal);
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
        compiler_context_->ReportError("Non-literal operand is not mapped to a target operand");
        return;
      }

      // Create an add instruction to add the li result and the non-literal operand
      auto add_inst =
          GET_BACKEND_TARGET()->CreateInstruction<RVADDWInst>(target_result, target_non_literal, target_result);
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
    compiler_context_->ReportError("One of the operands is not mapped to a target operand");
    return;
  }

  // Will never happen

  // // If one of them are zero register, we can just map the other one to the result and return
  // // Maybe both of them would be zero registers, but it doesn't matter
  // if (IsZeroRegister(target_lhs) || IsZeroRegister(target_rhs)) {
  //   auto target_non_zero = IsZeroRegister(target_lhs) ? target_rhs : target_lhs;
  //   GET_BACKEND_TARGET()->MapOperand(dst, target_non_zero);
  //   return;
  // }

  // Create a target result virtual register
  auto target_result = GET_BACKEND_TARGET()->CreateVirtualRegister(TargetRegSize::DWord, TargetRegClass::General);
  // Map the result operand to the target result
  GET_BACKEND_TARGET()->MapOperand(dst, target_result);
  // Create an add instruction to add the two operands
  auto add_inst = GET_BACKEND_TARGET()->CreateInstruction<RVADDWInst>(target_result, target_lhs, target_rhs);
  GET_BACKEND_TARGET()->GetCurrentBlock()->AddInst(add_inst);
}

auto LowerHelper::LowerSub(backend::inst::SubInst *inst) -> void {
  /**
   * @brief Lowering sub instruction is similar to add instruction,
   * it can be divided into following cases:
   * 1. Both operands are literals, we can compute the result as a literal
   * 2. Rhs is a literal, if its negative representation fits in 12 bits,
   * it can be lowered to an addi instruction, otherwise,
   * we need to load it into a register via 'li' instruction.
   * 3. Both operands are not literals, we can just use a normal sub instruction
   * to subtract the two operands
   */
  auto lhs = inst->GetLHS();
  auto rhs = inst->GetRHS();
  auto dst = inst->GetResult();

  // If both operands are literals, we can compute the result as a literal
  if (lhs->IsLiteral() && rhs->IsLiteral()) {
    compiler_context_->ReportError("Both operands are literals, should have been constant folded");
    return;
  }

  // If rhs is a literal
  if (rhs->IsLiteral()) {
    auto literal_value = static_cast<utils::IntLiteral *>(rhs)->GetValue();
    // If rhs equals to zero, we can just map lhs to the result and return
    if (literal_value == 0) {
      auto target_lhs = GET_BACKEND_TARGET()->GetMappedOperand(lhs);
      GET_BACKEND_TARGET()->MapOperand(dst, target_lhs);
      return;
    }

    // If it is not zero, maybe can transform it into an addi instruction.
    // Check if the negative representation fits in 12 bits.
    // Equals to rd = rs - C, which is equivalent to rd = rs + (-C)
    if (Is12BitSignedInteger(-literal_value)) {
      // If the literal_value is 0, do nothing.
      if (literal_value == 0) {
        auto target_lhs = GET_BACKEND_TARGET()->GetMappedOperand(lhs);
        GET_BACKEND_TARGET()->MapOperand(dst, target_lhs);
        return;
      }
      // Use an addi instruction with the negative literal
      auto target_result = GET_BACKEND_TARGET()->CreateVirtualRegister(TargetRegSize::DWord, TargetRegClass::General);
      GET_BACKEND_TARGET()->MapOperand(dst, target_result);
      auto target_literal = GET_BACKEND_TARGET()->CreateIntLiteral(-literal_value);
      auto target_lhs = GET_BACKEND_TARGET()->GetMappedOperand(lhs);
      if (target_lhs == nullptr) {
        compiler_context_->ReportError("LHS operand is not mapped to a target operand");
        return;
      }
      auto addi_inst =
          GET_BACKEND_TARGET()->CreateInstruction<RVADDIWInst>(target_result, target_lhs, target_literal);
      GET_BACKEND_TARGET()->GetCurrentBlock()->AddInst(addi_inst);
    } else {
      auto target_lhs = GET_BACKEND_TARGET()->GetMappedOperand(lhs);

      if (target_lhs == nullptr) {
        compiler_context_->ReportError("LHS operand is not mapped to a target operand");
        return;
      }

      // Load the literal into a register via 'li' instruction and then subtract
      auto target_result = GET_BACKEND_TARGET()->CreateVirtualRegister(TargetRegSize::DWord, TargetRegClass::General);
      GET_BACKEND_TARGET()->MapOperand(dst, target_result);
      auto target_literal = GET_BACKEND_TARGET()->CreateIntLiteral(literal_value);
      auto li_inst = GET_BACKEND_TARGET()->CreateInstruction<RVLIInst>(target_result, target_literal);
      GET_BACKEND_TARGET()->GetCurrentBlock()->AddInst(li_inst);

      // Now subtract the lhs from the loaded literal
      auto sub_inst = GET_BACKEND_TARGET()->CreateInstruction<RVSUBWInst>(target_result, target_lhs, target_result);
      GET_BACKEND_TARGET()->GetCurrentBlock()->AddInst(sub_inst);
    }
    return;
  }

  // If lhs is a literal, we can just use a normal sub instruction to subtract the two operands
  if (lhs->IsLiteral()) {
    // If it do not overflow 12-bit signed integer, we can use addi instruction to avoid li.
    /**
     * @brief If lhs is a literal, it should be handled in following cases:
     * 1. lhs == 0
     * 2. lhs itself in 12-bit signed range, rd = -x + C
     * 3. -lhs in 12-bit signed range, rd = -(x - C) = - ( x + (-C) )
     * 4. Otherwise, we need to load it into a register via 'li'
     */
    auto literal_value = static_cast<utils::IntLiteral *>(lhs)->GetValue();
    auto target_rhs = GET_BACKEND_TARGET()->GetMappedOperand(rhs);
    if (target_rhs == nullptr) {
      compiler_context_->ReportError("RHS operand is not mapped to a target operand");
      return;
    }
    // 1. lhs = 0
    if (literal_value == 0) {
      // If lhs is zero, we can just map the negation of rhs to the result
      // Create a target result virtual register
      auto target_result = GET_BACKEND_TARGET()->CreateVirtualRegister(TargetRegSize::DWord, TargetRegClass::General);
      GET_BACKEND_TARGET()->MapOperand(dst, target_result);
      // Create a sub instruction to subtract rhs from zero
      auto zero_register = rv64_getter_.GetZeroRegister(GET_BACKEND_TARGET(), TargetRegSize::DWord);
      auto sub_inst = GET_BACKEND_TARGET()->CreateInstruction<RVSUBWInst>(target_result, zero_register, target_rhs);
      GET_BACKEND_TARGET()->GetCurrentBlock()->AddInst(sub_inst);
      return;
    }
    // 2. lhs itself in 12-bit signed range, rd = -x + C
    if (Is12BitSignedInteger(literal_value)) {
      auto target_result = GET_BACKEND_TARGET()->CreateVirtualRegister(TargetRegSize::DWord, TargetRegClass::General);
      GET_BACKEND_TARGET()->MapOperand(dst, target_result);
      // First use a sub instruction to negate rhs
      auto zero_register = rv64_getter_.GetZeroRegister(GET_BACKEND_TARGET(), TargetRegSize::DWord);
      auto sub_inst = GET_BACKEND_TARGET()->CreateInstruction<RVSUBWInst>(target_result, zero_register, target_rhs);
      GET_BACKEND_TARGET()->GetCurrentBlock()->AddInst(sub_inst);

      // Then use an addi instruction to add the negated rhs and the literal
      auto target_literal = GET_BACKEND_TARGET()->CreateIntLiteral(literal_value);
      auto addi_inst =
          GET_BACKEND_TARGET()->CreateInstruction<RVADDIWInst>(target_result, target_result, target_literal);
      GET_BACKEND_TARGET()->GetCurrentBlock()->AddInst(addi_inst);
      return;
    }
    // 3. -lhs in 12-bit signed range, rd = -(x - C) = - ( x + (-C) )
    if (Is12BitSignedInteger(-literal_value)) {
      auto target_result = GET_BACKEND_TARGET()->CreateVirtualRegister(TargetRegSize::DWord, TargetRegClass::General);
      GET_BACKEND_TARGET()->MapOperand(dst, target_result);

      // First use an addi instruction to add rhs and the negated literal
      auto target_literal = GET_BACKEND_TARGET()->CreateIntLiteral(-literal_value);
      auto addi_inst =
          GET_BACKEND_TARGET()->CreateInstruction<RVADDIWInst>(target_result, target_rhs, target_literal);
      GET_BACKEND_TARGET()->GetCurrentBlock()->AddInst(addi_inst);
      // Then use a sub instruction to negate the result of the addi instruction

      auto zero_register = rv64_getter_.GetZeroRegister(GET_BACKEND_TARGET(), TargetRegSize::DWord);
      auto sub_inst = GET_BACKEND_TARGET()->CreateInstruction<RVSUBWInst>(target_result, zero_register, target_result);
      GET_BACKEND_TARGET()->GetCurrentBlock()->AddInst(sub_inst);
      return;
    }
    // 4. Otherwise, need to li
    // lhs is a literal
    // li target_result, literal_value
    // sub target_result, target_result, rhs
    auto target_result = GET_BACKEND_TARGET()->CreateVirtualRegister(TargetRegSize::DWord, TargetRegClass::General);
    GET_BACKEND_TARGET()->MapOperand(dst, target_result);
    auto target_literal = GET_BACKEND_TARGET()->CreateIntLiteral(literal_value);
    auto li_inst = GET_BACKEND_TARGET()->CreateInstruction<RVLIInst>(target_result, target_literal);
    GET_BACKEND_TARGET()->GetCurrentBlock()->AddInst(li_inst);
    // Then use a sub instruction to subtract rhs from the loaded literal
    auto sub_inst = GET_BACKEND_TARGET()->CreateInstruction<RVSUBWInst>(target_result, target_result, target_rhs);
    GET_BACKEND_TARGET()->GetCurrentBlock()->AddInst(sub_inst);
    return;
  }

  // Otherwise, both operands are not literals,
  // normally they should both be virtual registers
  auto target_lhs = GET_BACKEND_TARGET()->GetMappedOperand(lhs);
  auto target_rhs = GET_BACKEND_TARGET()->GetMappedOperand(rhs);

  if (target_lhs == nullptr || target_rhs == nullptr) {
    compiler_context_->ReportError("One of the operands is not mapped to a target operand");
    return;
  }

  // Create a target result virtual register
  auto target_result = GET_BACKEND_TARGET()->CreateVirtualRegister(TargetRegSize::DWord, TargetRegClass::General);
  GET_BACKEND_TARGET()->MapOperand(dst, target_result);
  // Create a sub instruction to subtract the two operands
  auto sub_inst = GET_BACKEND_TARGET()->CreateInstruction<RVSUBWInst>(target_result, target_lhs, target_rhs);
  GET_BACKEND_TARGET()->GetCurrentBlock()->AddInst(sub_inst);
}

auto LowerHelper::LowerMul(backend::inst::MulInst *inst) -> void {
  /**
   * @brief For multiply instruction, sometimes,
   * we can use a shift instruction to handle the case when one of the operands is a power of two,
   * or maybe if this literal can be divided into two parts:
   * 1. power of 2
   * plus or substract
   * 2. the rest
   */
  auto lhs = inst->GetLHS();
  auto rhs = inst->GetRHS();
  auto dst = inst->GetResult();

  // Check if both operands are literals
  if (lhs->IsLiteral() && rhs->IsLiteral()) {
    compiler_context_->ReportError("Both operands are literals, should have been constant folded");
    return;
  }

  // If one of the operands is literal, we might be able to use a shift instruction to optimize.
  if (lhs->IsLiteral() || rhs->IsLiteral()) {
    auto literal_operand = lhs->IsLiteral() ? lhs : rhs;
    auto non_literal_operand = lhs->IsLiteral() ? rhs : lhs;

    auto literal_value = static_cast<utils::IntLiteral *>(literal_operand)->GetValue();
    /**
     * @brief Deal with following cases:
     * 1. If the literal is zero, 1 or -1
     * 2. If the literal is a power of two, we can use a shift instruction
     * 3. If the literal can be expressed as a sum or difference of two powers of two,
     * we can use two shift instructions and an add or sub instruction
     * 4. Otherwise, we can just use a normal mul instruction
     */

    // If it is zero, we can just map the result to a zero literal
    if (literal_value == 0) {
      // Create a zero literal and map the result to it
      auto zero_register = rv64_getter_.GetZeroRegister(GET_BACKEND_TARGET(), TargetRegSize::DWord);
      GET_BACKEND_TARGET()->MapOperand(dst, zero_register);
      return;
    }
    // If it is 1 or -1, we can just map the result to the non-literal operand or its negation
    if (literal_value == 1) {
      auto target_non_literal = GET_BACKEND_TARGET()->GetMappedOperand(non_literal_operand);
      if (target_non_literal == nullptr) {
        compiler_context_->ReportError("Non-literal operand is not mapped to a target operand");
        return;
      }
      GET_BACKEND_TARGET()->MapOperand(dst, target_non_literal);
      return;
    }
    if (literal_value == -1) {
      auto target_non_literal = GET_BACKEND_TARGET()->GetMappedOperand(non_literal_operand);
      if (target_non_literal == nullptr) {
        compiler_context_->ReportError("Non-literal operand is not mapped to a target operand");
        return;
      }

      // Create a neg instruction to negate the non-literal operand
      auto target_result = GET_BACKEND_TARGET()->CreateVirtualRegister(TargetRegSize::DWord, TargetRegClass::General);
      GET_BACKEND_TARGET()->MapOperand(dst, target_result);
      // Create a sub instruction to subtract the non-literal operand from zero

      // For this kind of register, we don't need to pre-color, since it will never be allocated.
      auto zero_register = rv64_getter_.GetZeroRegister(GET_BACKEND_TARGET(), TargetRegSize::DWord);
      auto sub_inst =
          GET_BACKEND_TARGET()->CreateInstruction<RVSUBWInst>(target_result, zero_register, target_non_literal);
      GET_BACKEND_TARGET()->GetCurrentBlock()->AddInst(sub_inst);
      return;
    }

    // Then check if it is a power of two
    if (IsPowerOfTwo(literal_value)) {
      // Use a shift instruction to multiply
      auto target_non_literal = GET_BACKEND_TARGET()->GetMappedOperand(non_literal_operand);
      if (target_non_literal == nullptr) {
        compiler_context_->ReportError("Non-literal operand is not mapped to a target operand");
        return;
      }
      auto target_result = GET_BACKEND_TARGET()->CreateVirtualRegister(TargetRegSize::DWord, TargetRegClass::General);
      GET_BACKEND_TARGET()->MapOperand(dst, target_result);
      // Create a shift left instruction to multiply
      // Since the mul is only for positive power of two, so we can just simply use the exponent
      auto shift_amount = GetPowerOfTwoExponent(literal_value);
      auto target_shift_amount = GET_BACKEND_TARGET()->CreateIntLiteral(shift_amount);
      auto sll_inst =
          GET_BACKEND_TARGET()->CreateInstruction<RVSLLIWInst>(target_result, target_non_literal, target_shift_amount);
      GET_BACKEND_TARGET()->GetCurrentBlock()->AddInst(sll_inst);
      return;
    }
    // If it can be expressed as a sum or difference of two powers of two, we can use two shift instructions and an add
    // or sub instruction
    if (IsSumOrDifferenceOfTwoPowersOfTwo(literal_value)) {
      auto decomposition = GetSumOrDifferenceOfTwoPowersOfTwo(literal_value);
      auto target_non_literal = GET_BACKEND_TARGET()->GetMappedOperand(non_literal_operand);
      auto target_result = GET_BACKEND_TARGET()->CreateVirtualRegister(TargetRegSize::DWord, TargetRegClass::General);
      // Create two shift left instructions to multiply by 2^i and 2^j
      auto target_shift_amount_i = GET_BACKEND_TARGET()->CreateIntLiteral(decomposition.lhs_exponent_);
      auto target_shift_amount_j = GET_BACKEND_TARGET()->CreateIntLiteral(decomposition.rhs_exponent_);
      auto sll_inst_i = GET_BACKEND_TARGET()->CreateInstruction<RVSLLIWInst>(target_result, target_non_literal,
                                                                             target_shift_amount_i);
      GET_BACKEND_TARGET()->GetCurrentBlock()->AddInst(sll_inst_i);
      auto target_temp = GET_BACKEND_TARGET()->CreateVirtualRegister(TargetRegSize::DWord, TargetRegClass::General);
      auto sll_inst_j =
          GET_BACKEND_TARGET()->CreateInstruction<RVSLLIWInst>(target_temp, target_non_literal, target_shift_amount_j);
      GET_BACKEND_TARGET()->GetCurrentBlock()->AddInst(sll_inst_j);
      if (decomposition.op_ == PowerOfTwoBinaryOp::Add) {
        auto add_inst = GET_BACKEND_TARGET()->CreateInstruction<RVADDWInst>(target_result, target_result, target_temp);
        GET_BACKEND_TARGET()->GetCurrentBlock()->AddInst(add_inst);
      } else {
        auto sub_inst =
            GET_BACKEND_TARGET()->CreateInstruction<RVSUBWInst>(target_result, target_result, target_temp);
        GET_BACKEND_TARGET()->GetCurrentBlock()->AddInst(sub_inst);
      }
      GET_BACKEND_TARGET()->MapOperand(dst, target_result);
      return;
    }
  }

  auto target_lhs = GET_BACKEND_TARGET()->GetMappedOperand(lhs);
  auto target_rhs = GET_BACKEND_TARGET()->GetMappedOperand(rhs);

  if (target_lhs == nullptr || target_rhs == nullptr) {
    compiler_context_->ReportError("One of the operands is not mapped to a target operand");
    return;
  }

  auto target_result = GET_BACKEND_TARGET()->CreateVirtualRegister(TargetRegSize::DWord, TargetRegClass::General);
  GET_BACKEND_TARGET()->MapOperand(dst, target_result);
  auto mul_inst = GET_BACKEND_TARGET()->CreateInstruction<RVMULWInst>(target_result, target_lhs, target_rhs);
  GET_BACKEND_TARGET()->GetCurrentBlock()->AddInst(mul_inst);
}

auto LowerHelper::LowerDiv(backend::inst::DivInst *inst) -> void {
  // For division instruction, there is no immediate instruction,
  // so literals must be materialized into registers before emitting `div`.
  auto lhs = inst->GetLHS();
  auto rhs = inst->GetRHS();
  auto dst = inst->GetResult();

  if (lhs->IsLiteral() && rhs->IsLiteral()) {
    compiler_context_->ReportError("Both operands are literals, should have been constant folded");
    return;
  }

  auto materialize_literal = [&](int value) -> backend::targetir::TargetOperand * {
    auto target_reg = GET_BACKEND_TARGET()->CreateVirtualRegister(TargetRegSize::DWord, TargetRegClass::General);
    auto target_literal = GET_BACKEND_TARGET()->CreateIntLiteral(value);
    auto li_inst = GET_BACKEND_TARGET()->CreateInstruction<RVLIInst>(target_reg, target_literal);
    GET_BACKEND_TARGET()->GetCurrentBlock()->AddInst(li_inst);
    return target_reg;
  };

  // A few safe constant divisors are worth handling directly.
  if (rhs->IsLiteral()) {
    auto literal_value = static_cast<utils::IntLiteral *>(rhs)->GetValue();
    auto target_lhs = GET_BACKEND_TARGET()->GetMappedOperand(lhs);
    if (target_lhs == nullptr) {
      compiler_context_->ReportError("LHS operand is not mapped to a target operand");
      return;
    }

    if (literal_value == 1) {
      GET_BACKEND_TARGET()->MapOperand(dst, target_lhs);
      return;
    }

    if (literal_value == -1) {
      auto target_result = GET_BACKEND_TARGET()->CreateVirtualRegister(TargetRegSize::DWord, TargetRegClass::General);
      GET_BACKEND_TARGET()->MapOperand(dst, target_result);
      auto zero_register = rv64_getter_.GetZeroRegister(GET_BACKEND_TARGET(), TargetRegSize::DWord);
      auto sub_inst = GET_BACKEND_TARGET()->CreateInstruction<RVSUBWInst>(target_result, zero_register, target_lhs);
      GET_BACKEND_TARGET()->GetCurrentBlock()->AddInst(sub_inst);
      return;
    }
  }

  auto target_lhs = lhs->IsLiteral() ? materialize_literal(static_cast<utils::IntLiteral *>(lhs)->GetValue())
                                     : GET_BACKEND_TARGET()->GetMappedOperand(lhs);
  auto target_rhs = rhs->IsLiteral() ? materialize_literal(static_cast<utils::IntLiteral *>(rhs)->GetValue())
                                     : GET_BACKEND_TARGET()->GetMappedOperand(rhs);

  if (target_lhs == nullptr || target_rhs == nullptr) {
    compiler_context_->ReportError("One of the operands is not mapped to a target operand");
    return;
  }

  auto target_result = GET_BACKEND_TARGET()->CreateVirtualRegister(TargetRegSize::DWord, TargetRegClass::General);
  GET_BACKEND_TARGET()->MapOperand(dst, target_result);
  auto div_inst = GET_BACKEND_TARGET()->CreateInstruction<RVDIVWInst>(target_result, target_lhs, target_rhs);
  GET_BACKEND_TARGET()->GetCurrentBlock()->AddInst(div_inst);
}
auto LowerHelper::LowerMod(backend::inst::ModInst *inst) -> void {
  auto lhs = inst->GetLHS();
  auto rhs = inst->GetRHS();
  auto dst = inst->GetResult();

  if (lhs->IsLiteral() && rhs->IsLiteral()) {
    compiler_context_->ReportError("Both operands are literals, should have been constant folded");
    return;
  }

  auto materialize_literal = [&](int value) -> backend::targetir::TargetOperand * {
    auto target_reg = GET_BACKEND_TARGET()->CreateVirtualRegister(TargetRegSize::DWord, TargetRegClass::General);
    auto target_literal = GET_BACKEND_TARGET()->CreateIntLiteral(value);
    auto li_inst = GET_BACKEND_TARGET()->CreateInstruction<RVLIInst>(target_reg, target_literal);
    GET_BACKEND_TARGET()->GetCurrentBlock()->AddInst(li_inst);
    return target_reg;
  };

  auto target_lhs = lhs->IsLiteral() ? materialize_literal(static_cast<utils::IntLiteral *>(lhs)->GetValue())
                                     : GET_BACKEND_TARGET()->GetMappedOperand(lhs);
  auto target_rhs = rhs->IsLiteral() ? materialize_literal(static_cast<utils::IntLiteral *>(rhs)->GetValue())
                                     : GET_BACKEND_TARGET()->GetMappedOperand(rhs);

  if (target_lhs == nullptr || target_rhs == nullptr) {
    compiler_context_->ReportError("One of the operands is not mapped to a target operand");
    return;
  }

  auto target_result = GET_BACKEND_TARGET()->CreateVirtualRegister(TargetRegSize::DWord, TargetRegClass::General);
  GET_BACKEND_TARGET()->MapOperand(dst, target_result);
  auto rem_inst = GET_BACKEND_TARGET()->CreateInstruction<RVREMWInst>(target_result, target_lhs, target_rhs);
  GET_BACKEND_TARGET()->GetCurrentBlock()->AddInst(rem_inst);
}

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
