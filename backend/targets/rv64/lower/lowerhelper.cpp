#include "backend/targets/rv64/lower/lowerhelper.h"
#include <stdexcept>
#include "backend/mir/inst/inst.h"
#include "backend/mir/utils/operand/literaloperand.h"
#include "backend/targets/rv64/inst/arithmetic/arithmetic.h"
#include "backend/targets/rv64/inst/branch/jmp.h"
#include "backend/targets/rv64/inst/cast/cast.h"
#include "backend/targets/rv64/inst/cmp/cmp.h"
#include "backend/targets/rv64/inst/func/ret.h"
#include "backend/targets/rv64/inst/immediate/immediate.h"
#include "backend/targets/rv64/inst/memory/load.h"
#include "backend/targets/rv64/inst/pseudo/pseudo.h"
#include "backend/targets/rv64/symbol/rv64symbol.h"
#include "backend/targets/rv64/utils/checker.h"
#include "backend/targets/rv64/utils/materializer.h"
#include "backend/targets/utils/operand/targetoperand.h"
#include "backend/targets/utils/operand/targetreg.h"
#include "context/compiler_context.h"
#include "mir/inst/cmp/cmp.h"
#include "mir/utils/operand/frameoperand.h"

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

    DISPATCH_MIR_INST(backend::inst::BrInst, LowerBr)
    DISPATCH_MIR_INST(backend::inst::BrCondInst, LowerCondBr)
    DISPATCH_MIR_INST(backend::inst::RetInst, LowerRet)

    DISPATCH_MIR_INST(backend::inst::CallInst, LowerCall)

    DISPATCH_MIR_INST(backend::inst::ZextInst, LowerZext)
    DISPATCH_MIR_INST(backend::inst::Int2FloatInst, LowerInt2Float)
    DISPATCH_MIR_INST(backend::inst::Float2IntInst, LowerFloat2Int)
    DISPATCH_MIR_INST(backend::inst::Bool2FloatInst, LowerBool2Float)

    DISPATCH_MIR_INST(backend::inst::CopyInst, LowerCopy)
    // Phi has been lowered.
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
      auto addi_inst = GET_BACKEND_TARGET()->CreateInstruction<RVADDIWInst>(target_result, target_lhs, target_literal);
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
      auto addi_inst = GET_BACKEND_TARGET()->CreateInstruction<RVADDIWInst>(target_result, target_rhs, target_literal);
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
     * (Maybe not beneift the performance, as a single pass in the future)
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

    // In the future, do strength reduction as a single pass

    // // If it can be expressed as a sum or difference of two powers of two, we can use two shift instructions and an
    // add
    // // or sub instruction
    // if (IsSumOrDifferenceOfTwoPowersOfTwo(literal_value)) {
    //   auto decomposition = GetSumOrDifferenceOfTwoPowersOfTwo(literal_value);
    //   auto target_non_literal = GET_BACKEND_TARGET()->GetMappedOperand(non_literal_operand);
    //   auto target_result = GET_BACKEND_TARGET()->CreateVirtualRegister(TargetRegSize::DWord,
    //   TargetRegClass::General);
    //   // Create two shift left instructions to multiply by 2^i and 2^j
    //   auto target_shift_amount_i = GET_BACKEND_TARGET()->CreateIntLiteral(decomposition.lhs_exponent_);
    //   auto target_shift_amount_j = GET_BACKEND_TARGET()->CreateIntLiteral(decomposition.rhs_exponent_);
    //   auto sll_inst_i = GET_BACKEND_TARGET()->CreateInstruction<RVSLLIWInst>(target_result, target_non_literal,
    //                                                                          target_shift_amount_i);
    //   GET_BACKEND_TARGET()->GetCurrentBlock()->AddInst(sll_inst_i);
    //   auto target_temp = GET_BACKEND_TARGET()->CreateVirtualRegister(TargetRegSize::DWord, TargetRegClass::General);
    //   auto sll_inst_j =
    //       GET_BACKEND_TARGET()->CreateInstruction<RVSLLIWInst>(target_temp, target_non_literal,
    //       target_shift_amount_j);
    //   GET_BACKEND_TARGET()->GetCurrentBlock()->AddInst(sll_inst_j);
    //   if (decomposition.op_ == PowerOfTwoBinaryOp::Add) {
    //     auto add_inst = GET_BACKEND_TARGET()->CreateInstruction<RVADDWInst>(target_result, target_result,
    //     target_temp); GET_BACKEND_TARGET()->GetCurrentBlock()->AddInst(add_inst);
    //   } else {
    //     auto sub_inst =
    //         GET_BACKEND_TARGET()->CreateInstruction<RVSUBWInst>(target_result, target_result, target_temp);
    //     GET_BACKEND_TARGET()->GetCurrentBlock()->AddInst(sub_inst);
    //   }
    //   GET_BACKEND_TARGET()->MapOperand(dst, target_result);
    //   return;
    // }
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

  auto target_lhs = lhs->IsLiteral()
                        ? MaterializeLiteral(compiler_context_, static_cast<utils::IntLiteral *>(lhs)->GetValue())
                        : GET_BACKEND_TARGET()->GetMappedOperand(lhs);
  auto target_rhs = rhs->IsLiteral()
                        ? MaterializeLiteral(compiler_context_, static_cast<utils::IntLiteral *>(rhs)->GetValue())
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

  auto target_lhs = lhs->IsLiteral()
                        ? MaterializeLiteral(compiler_context_, static_cast<utils::IntLiteral *>(lhs)->GetValue())
                        : GET_BACKEND_TARGET()->GetMappedOperand(lhs);
  auto target_rhs = rhs->IsLiteral()
                        ? MaterializeLiteral(compiler_context_, static_cast<utils::IntLiteral *>(rhs)->GetValue())
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

auto LowerHelper::LowerAddF(backend::inst::AddFInst *inst) -> void {
  auto lhs = inst->GetLHS();
  auto rhs = inst->GetRHS();
  auto dst = inst->GetResult();

  if (lhs->IsLiteral() && rhs->IsLiteral()) {
    compiler_context_->ReportError("Both float operands are literals, should have been constant folded");
    return;
  }

  auto target_lhs = lhs->IsLiteral()
                        ? MaterializeLiteral(compiler_context_, static_cast<utils::FloatLiteral *>(lhs)->GetValue())
                        : GET_BACKEND_TARGET()->GetMappedOperand(lhs);
  auto target_rhs = rhs->IsLiteral()
                        ? MaterializeLiteral(compiler_context_, static_cast<utils::FloatLiteral *>(rhs)->GetValue())
                        : GET_BACKEND_TARGET()->GetMappedOperand(rhs);
  if (target_lhs == nullptr || target_rhs == nullptr) {
    compiler_context_->ReportError("One of the float operands is not mapped to a target operand");
    return;
  }

  auto target_result = GET_BACKEND_TARGET()->CreateVirtualRegister(TargetRegSize::DWord, TargetRegClass::Float);
  GET_BACKEND_TARGET()->MapOperand(dst, target_result);
  auto addf_inst = GET_BACKEND_TARGET()->CreateInstruction<RVFADDSInst>(target_result, target_lhs, target_rhs);
  GET_BACKEND_TARGET()->GetCurrentBlock()->AddInst(addf_inst);
}

auto LowerHelper::LowerSubF(backend::inst::SubFInst *inst) -> void {
  auto lhs = inst->GetLHS();
  auto rhs = inst->GetRHS();
  auto dst = inst->GetResult();

  if (lhs->IsLiteral() && rhs->IsLiteral()) {
    compiler_context_->ReportError("Both float operands are literals, should have been constant folded");
    return;
  }

  auto target_lhs = lhs->IsLiteral()
                        ? MaterializeLiteral(compiler_context_, static_cast<utils::FloatLiteral *>(lhs)->GetValue())
                        : GET_BACKEND_TARGET()->GetMappedOperand(lhs);
  auto target_rhs = rhs->IsLiteral()
                        ? MaterializeLiteral(compiler_context_, static_cast<utils::FloatLiteral *>(rhs)->GetValue())
                        : GET_BACKEND_TARGET()->GetMappedOperand(rhs);
  if (target_lhs == nullptr || target_rhs == nullptr) {
    compiler_context_->ReportError("One of the float operands is not mapped to a target operand");
    return;
  }

  auto target_result = GET_BACKEND_TARGET()->CreateVirtualRegister(TargetRegSize::DWord, TargetRegClass::Float);
  GET_BACKEND_TARGET()->MapOperand(dst, target_result);
  auto subf_inst = GET_BACKEND_TARGET()->CreateInstruction<RVFSUBSInst>(target_result, target_lhs, target_rhs);
  GET_BACKEND_TARGET()->GetCurrentBlock()->AddInst(subf_inst);
}

auto LowerHelper::LowerMulF(backend::inst::MulFInst *inst) -> void {
  auto lhs = inst->GetLHS();
  auto rhs = inst->GetRHS();
  auto dst = inst->GetResult();

  if (lhs->IsLiteral() && rhs->IsLiteral()) {
    compiler_context_->ReportError("Both float operands are literals, should have been constant folded");
    return;
  }

  auto target_lhs = lhs->IsLiteral()
                        ? MaterializeLiteral(compiler_context_, static_cast<utils::FloatLiteral *>(lhs)->GetValue())
                        : GET_BACKEND_TARGET()->GetMappedOperand(lhs);
  auto target_rhs = rhs->IsLiteral()
                        ? MaterializeLiteral(compiler_context_, static_cast<utils::FloatLiteral *>(rhs)->GetValue())
                        : GET_BACKEND_TARGET()->GetMappedOperand(rhs);
  if (target_lhs == nullptr || target_rhs == nullptr) {
    compiler_context_->ReportError("One of the float operands is not mapped to a target operand");
    return;
  }

  auto target_result = GET_BACKEND_TARGET()->CreateVirtualRegister(TargetRegSize::DWord, TargetRegClass::Float);
  GET_BACKEND_TARGET()->MapOperand(dst, target_result);
  auto mulf_inst = GET_BACKEND_TARGET()->CreateInstruction<RVFMULSInst>(target_result, target_lhs, target_rhs);
  GET_BACKEND_TARGET()->GetCurrentBlock()->AddInst(mulf_inst);
}

auto LowerHelper::LowerDivF(backend::inst::DivFInst *inst) -> void {
  auto lhs = inst->GetLHS();
  auto rhs = inst->GetRHS();
  auto dst = inst->GetResult();

  if (lhs->IsLiteral() && rhs->IsLiteral()) {
    compiler_context_->ReportError("Both float operands are literals, should have been constant folded");
    return;
  }

  auto target_lhs = lhs->IsLiteral()
                        ? MaterializeLiteral(compiler_context_, static_cast<utils::FloatLiteral *>(lhs)->GetValue())
                        : GET_BACKEND_TARGET()->GetMappedOperand(lhs);
  auto target_rhs = rhs->IsLiteral()
                        ? MaterializeLiteral(compiler_context_, static_cast<utils::FloatLiteral *>(rhs)->GetValue())
                        : GET_BACKEND_TARGET()->GetMappedOperand(rhs);
  if (target_lhs == nullptr || target_rhs == nullptr) {
    compiler_context_->ReportError("One of the float operands is not mapped to a target operand");
    return;
  }

  auto target_result = GET_BACKEND_TARGET()->CreateVirtualRegister(TargetRegSize::DWord, TargetRegClass::Float);
  GET_BACKEND_TARGET()->MapOperand(dst, target_result);
  auto divf_inst = GET_BACKEND_TARGET()->CreateInstruction<RVFDIVSInst>(target_result, target_lhs, target_rhs);
  GET_BACKEND_TARGET()->GetCurrentBlock()->AddInst(divf_inst);
}

auto LowerHelper::LowerICmp(backend::inst::ICmpInst *inst) -> void {
  /**
   * @brief For Icmp instruction, since for rv64, we only have:
   * slt, sle, seq, so we should first transform the predicate into one of these three.
   */
  auto lhs = inst->GetLHS();
  auto rhs = inst->GetRHS();
  auto predicate = inst->GetPredicate();

  // If both operands are literals, we should have done constant folding,
  // so just report an error
  if (lhs->IsLiteral() && rhs->IsLiteral()) {
    compiler_context_->ReportError("Both operands are literals, should have been constant folded");
    return;
  }
  // If predicate is GT or GE, we can transform it into L* predicate by swapping the operands
  if (predicate == backend::inst::CmpPredicate::GT || predicate == backend::inst::CmpPredicate::GE) {
    std::swap(lhs, rhs);
    // if predicate is GT, after swapping, it becomes LT.
    predicate = (predicate == backend::inst::CmpPredicate::GT) ? backend::inst::CmpPredicate::LT
                                                               : backend::inst::CmpPredicate::LE;
  }
  // 1. Handle LT and LE predicates, if rhs is a literal, we might be able to use an immediate instruction(slti).
  if (predicate == backend::inst::CmpPredicate::LT) {
    // If predicate is LT
    if (rhs->IsLiteral()) {
      auto literal_value = static_cast<utils::IntLiteral *>(rhs)->GetValue();
      // If the literal is in 12-bit signed range, we can use an immediate instruction(slti)
      if (Is12BitSignedInteger(literal_value)) {
        // Create a target result virtual register
        auto target_result = GET_BACKEND_TARGET()->CreateVirtualRegister(TargetRegSize::DWord, TargetRegClass::General);
        GET_BACKEND_TARGET()->MapOperand(inst->GetResult(), target_result);
        auto target_lhs = GET_BACKEND_TARGET()->GetMappedOperand(lhs);
        if (target_lhs == nullptr) {
          compiler_context_->ReportError("LHS operand is not mapped to a target operand");
          return;
        }
        // Create a slti instruction to compare the two operands
        auto target_literal = GET_BACKEND_TARGET()->CreateIntLiteral(literal_value);
        auto slti_inst = GET_BACKEND_TARGET()->CreateInstruction<RVSLTIInst>(target_result, target_lhs, target_literal);
        GET_BACKEND_TARGET()->GetCurrentBlock()->AddInst(slti_inst);
        return;
      }
      // Otherwise, we need to materialize the literal into a register and use the normal slt instruction
      auto target_lhs = GET_BACKEND_TARGET()->GetMappedOperand(lhs);
      auto target_rhs = MaterializeLiteral(compiler_context_, literal_value);
      if (target_lhs == nullptr || target_rhs == nullptr) {
        compiler_context_->ReportError("One of the operands is not mapped to a target operand");
        return;
      }
      // Create a target result virtual register
      auto target_result = GET_BACKEND_TARGET()->CreateVirtualRegister(TargetRegSize::DWord, TargetRegClass::General);
      GET_BACKEND_TARGET()->MapOperand(inst->GetResult(), target_result);
      // Create a slt instruction to compare the two operands
      auto slt_inst = GET_BACKEND_TARGET()->CreateInstruction<RVSLTInst>(target_result, target_lhs, target_rhs);
      GET_BACKEND_TARGET()->GetCurrentBlock()->AddInst(slt_inst);
      return;
    }
    // Create a SLT instruction to compare the two operands
    auto target_lhs = lhs->IsLiteral()
                          ? MaterializeLiteral(compiler_context_, static_cast<utils::IntLiteral *>(lhs)->GetValue())
                          : GET_BACKEND_TARGET()->GetMappedOperand(lhs);
    auto target_rhs = GET_BACKEND_TARGET()->GetMappedOperand(rhs);
    if (target_lhs == nullptr || target_rhs == nullptr) {
      compiler_context_->ReportError("One of the operands is not mapped to a target operand");
      return;
    }
    // Create a target result virtual register
    auto target_result = GET_BACKEND_TARGET()->CreateVirtualRegister(TargetRegSize::DWord, TargetRegClass::General);
    GET_BACKEND_TARGET()->MapOperand(inst->GetResult(), target_result);
    // Create a slt instruction to compare the two operands
    auto slt_inst = GET_BACKEND_TARGET()->CreateInstruction<RVSLTInst>(target_result, target_lhs, target_rhs);
    GET_BACKEND_TARGET()->GetCurrentBlock()->AddInst(slt_inst);
    return;
  }
  // 2. Handle SLE predicate
  if (predicate == backend::inst::CmpPredicate::LE) {
    // Perform the same logic as LT, but the SLE will be like:
    // lhs <= rhs => !(lhs > rhs) => !(rhs < lhs) --after rename--> !(lhs < rhs)
    std::swap(lhs, rhs);
    // Create the result register for (lhs < rhs)
    auto tmp_result = GET_BACKEND_TARGET()->CreateVirtualRegister(TargetRegSize::DWord, TargetRegClass::General);
    if (rhs->IsLiteral()) {
      // Check if the literal is in 12-bit signed range, we can use an immediate instruction(slti)
      auto literal_value = static_cast<utils::IntLiteral *>(rhs)->GetValue();
      if (Is12BitSignedInteger(literal_value)) {
        auto target_lhs = GET_BACKEND_TARGET()->GetMappedOperand(lhs);
        if (target_lhs == nullptr) {
          compiler_context_->ReportError("LHS operand is not mapped to a target operand");
          return;
        }
        // Create a slti instruction to compare the two operands
        auto target_literal = GET_BACKEND_TARGET()->CreateIntLiteral(literal_value);
        auto slti_inst = GET_BACKEND_TARGET()->CreateInstruction<RVSLTIInst>(tmp_result, target_lhs, target_literal);
        GET_BACKEND_TARGET()->GetCurrentBlock()->AddInst(slti_inst);
      } else {
        // Otherwise, we need to materialize the literal into a register and use the normal slt instruction
        auto target_lhs = lhs->IsLiteral()
                              ? MaterializeLiteral(compiler_context_, static_cast<utils::IntLiteral *>(lhs)->GetValue())
                              : GET_BACKEND_TARGET()->GetMappedOperand(lhs);
        auto target_rhs = MaterializeLiteral(compiler_context_, literal_value);
        if (target_lhs == nullptr || target_rhs == nullptr) {
          compiler_context_->ReportError("One of the operands is not mapped to a target operand");
          return;
        }
        // Create a slt instruction to compare the two operands
        auto slt_inst = GET_BACKEND_TARGET()->CreateInstruction<RVSLTInst>(tmp_result, target_lhs, target_rhs);
        GET_BACKEND_TARGET()->GetCurrentBlock()->AddInst(slt_inst);
      }
    } else {
      // Otherwise, we need to use the normal slt instruction
      auto target_lhs = lhs->IsLiteral()
                            ? MaterializeLiteral(compiler_context_, static_cast<utils::IntLiteral *>(lhs)->GetValue())
                            : GET_BACKEND_TARGET()->GetMappedOperand(lhs);
      auto target_rhs = GET_BACKEND_TARGET()->GetMappedOperand(rhs);
      if (target_lhs == nullptr || target_rhs == nullptr) {
        compiler_context_->ReportError("One of the operands is not mapped to a target operand");
        return;
      }
      // Create a slt instruction to compare the two operands
      auto slt_inst = GET_BACKEND_TARGET()->CreateInstruction<RVSLTInst>(tmp_result, target_lhs, target_rhs);
      GET_BACKEND_TARGET()->GetCurrentBlock()->AddInst(slt_inst);
    }
    // Then we need to negate the result of the comparison to get the final result
    auto target_result = GET_BACKEND_TARGET()->CreateVirtualRegister(TargetRegSize::DWord, TargetRegClass::General);
    GET_BACKEND_TARGET()->MapOperand(inst->GetResult(), target_result);

    // Create a not instruction(xori rd, rs, -1)
    auto not_inst = GET_BACKEND_TARGET()->CreateInstruction<RVXORIInst>(target_result, tmp_result,
                                                                        GET_BACKEND_TARGET()->CreateIntLiteral(-1));
    GET_BACKEND_TARGET()->GetCurrentBlock()->AddInst(not_inst);
    return;
  }
  // 3. Handle EQ and NE predicates
  if (predicate == backend::inst::CmpPredicate::EQ || predicate == backend::inst::CmpPredicate::NE) {
    // For EQ and NE, we can use xor instruction to compare them.
    // The order does not matter, since xor is commutative.
    // First check whether one of the operands is a literal,
    // if so, we can use an immediate instruction to optimize.
    if (lhs->IsLiteral() && rhs->IsLiteral()) {
      compiler_context_->ReportError("Both operands are literals, should have been constant folded");
      return;
    }
    if (lhs->IsLiteral() || rhs->IsLiteral()) {
      // If so, get the literal
      auto literal_operand = lhs->IsLiteral() ? lhs : rhs;
      auto non_literal_operand = lhs->IsLiteral() ? rhs : lhs;
      auto literal_value = static_cast<utils::IntLiteral *>(literal_operand)->GetValue();
      // If the negative of literal is in 12-bit signed range, we can use an immediate instruction(xori)
      // -literal_value is used because we will use: addi rd, rs, -literal_value to compare the two operands.
      if (Is12BitSignedInteger(-literal_value)) {
        // Use addi instruction to compare the two operands
        auto target_non_literal = GET_BACKEND_TARGET()->GetMappedOperand(non_literal_operand);
        if (target_non_literal == nullptr) {
          compiler_context_->ReportError("Non-literal operand is not mapped to a target operand");
          return;
        }
        TargetVirtualReg *target_result = nullptr;
        if (literal_value == 0) {
          // If the literal is zero, we can just map the result to the non-literal operand
          if (!target_non_literal->IsVirtualReg()) {
            compiler_context_->ReportError("Non-literal operand is not a virtual register");
            return;
          }
          target_result = static_cast<TargetVirtualReg *>(target_non_literal);
          GET_BACKEND_TARGET()->MapOperand(inst->GetResult(), target_non_literal);
        } else {
          target_result = GET_BACKEND_TARGET()->CreateVirtualRegister(TargetRegSize::DWord, TargetRegClass::General);
          GET_BACKEND_TARGET()->MapOperand(inst->GetResult(), target_result);
          auto target_literal = GET_BACKEND_TARGET()->CreateIntLiteral(-literal_value);
          auto addi_inst =
              GET_BACKEND_TARGET()->CreateInstruction<RVADDIInst>(target_result, target_non_literal, target_literal);
          GET_BACKEND_TARGET()->GetCurrentBlock()->AddInst(addi_inst);
        }
        // Then check seqz or snez
        if (predicate == inst::CmpPredicate::EQ) {
          auto seqz_inst = GET_BACKEND_TARGET()->CreateInstruction<RVSEQZInst>(target_result, target_result);
          GET_BACKEND_TARGET()->GetCurrentBlock()->AddInst(seqz_inst);
        } else {
          auto snez_inst = GET_BACKEND_TARGET()->CreateInstruction<RVSNEZInst>(target_result, target_result);
          GET_BACKEND_TARGET()->GetCurrentBlock()->AddInst(snez_inst);
        }
        // Done
        return;
      }
      // Otherwise, we need to materialize the literal into a register and use the normal xor instruction
      auto target_non_literal = GET_BACKEND_TARGET()->GetMappedOperand(non_literal_operand);
      auto target_literal = MaterializeLiteral(compiler_context_, -literal_value);
      if (target_non_literal == nullptr || target_literal == nullptr) {
        compiler_context_->ReportError("One of the operands is not mapped to a target operand");
        return;
      }
      // Then use xor instruction to compare the two operands
      auto target_result = GET_BACKEND_TARGET()->CreateVirtualRegister(TargetRegSize::DWord, TargetRegClass::General);
      GET_BACKEND_TARGET()->MapOperand(inst->GetResult(), target_result);
      auto xor_inst =
          GET_BACKEND_TARGET()->CreateInstruction<RVXORInst>(target_result, target_non_literal, target_literal);
      GET_BACKEND_TARGET()->GetCurrentBlock()->AddInst(xor_inst);
      // Then check seqz or snez
      if (predicate == inst::CmpPredicate::EQ) {
        auto seqz_inst = GET_BACKEND_TARGET()->CreateInstruction<RVSEQZInst>(target_result, target_result);
        GET_BACKEND_TARGET()->GetCurrentBlock()->AddInst(seqz_inst);
      } else {
        auto snez_inst = GET_BACKEND_TARGET()->CreateInstruction<RVSNEZInst>(target_result, target_result);
        GET_BACKEND_TARGET()->GetCurrentBlock()->AddInst(snez_inst);
      }
      return;
    }
    // Otherwise, use xor instruction
    auto target_lhs = GET_BACKEND_TARGET()->GetMappedOperand(lhs);
    auto target_rhs = GET_BACKEND_TARGET()->GetMappedOperand(rhs);
    if (target_lhs == nullptr || target_rhs == nullptr) {
      compiler_context_->ReportError("One of the operands is not mapped to a target operand");
      return;
    }
    auto target_result = GET_BACKEND_TARGET()->CreateVirtualRegister(TargetRegSize::DWord, TargetRegClass::General);
    GET_BACKEND_TARGET()->MapOperand(inst->GetResult(), target_result);
    auto xor_inst = GET_BACKEND_TARGET()->CreateInstruction<RVXORInst>(target_result, target_lhs, target_rhs);
    GET_BACKEND_TARGET()->GetCurrentBlock()->AddInst(xor_inst);
    // Then check seqz or snez
    if (predicate == inst::CmpPredicate::EQ) {
      auto seqz_inst = GET_BACKEND_TARGET()->CreateInstruction<RVSEQZInst>(target_result, target_result);
      GET_BACKEND_TARGET()->GetCurrentBlock()->AddInst(seqz_inst);
    } else {
      auto snez_inst = GET_BACKEND_TARGET()->CreateInstruction<RVSNEZInst>(target_result, target_result);
      GET_BACKEND_TARGET()->GetCurrentBlock()->AddInst(snez_inst);
    }
    return;
  }

  // Should not reach here, since we have handled all the predicates
  compiler_context_->ReportError("Unhandled ICmp predicate");
}

auto LowerHelper::LowerFCmp(backend::inst::FCmpInst *inst) -> void {
  // For fcmp instruction, we should handle it like icmp, but no literal allowed.
  // For GT and GE, we can swap the operands and change the predicate.
  auto lhs = inst->GetLHS();
  auto rhs = inst->GetRHS();
  auto predicate = inst->GetPredicate();
  if (lhs->IsLiteral() && rhs->IsLiteral()) {
    compiler_context_->ReportError("FCmp operands cannot be literals");
    return;
  }
  if (predicate == backend::inst::CmpPredicate::GT || predicate == backend::inst::CmpPredicate::GE) {
    std::swap(lhs, rhs);
    predicate = (predicate == backend::inst::CmpPredicate::GT) ? backend::inst::CmpPredicate::LT
                                                               : backend::inst::CmpPredicate::LE;
  }
  // We can just load all the literals into registers
  auto target_lhs = lhs->IsLiteral()
                        ? MaterializeLiteral(compiler_context_, static_cast<utils::FloatLiteral *>(lhs)->GetValue())
                        : GET_BACKEND_TARGET()->GetMappedOperand(lhs);
  auto target_rhs = rhs->IsLiteral()
                        ? MaterializeLiteral(compiler_context_, static_cast<utils::FloatLiteral *>(rhs)->GetValue())
                        : GET_BACKEND_TARGET()->GetMappedOperand(rhs);
  if (target_lhs == nullptr || target_rhs == nullptr) {
    compiler_context_->ReportError("One of the operands is not mapped to a target operand");
    return;
  }

  // For LT and LE, we can use the fcmp instruction directly.
  if (predicate == backend::inst::CmpPredicate::LT) {
    auto target_result = GET_BACKEND_TARGET()->CreateVirtualRegister(TargetRegSize::DWord, TargetRegClass::General);
    GET_BACKEND_TARGET()->MapOperand(inst->GetResult(), target_result);
    auto fcmp_inst = GET_BACKEND_TARGET()->CreateInstruction<RVFLTSInst>(target_result, target_lhs, target_rhs);
    GET_BACKEND_TARGET()->GetCurrentBlock()->AddInst(fcmp_inst);
    return;
  }
  if (predicate == backend::inst::CmpPredicate::LE) {
    auto target_result = GET_BACKEND_TARGET()->CreateVirtualRegister(TargetRegSize::DWord, TargetRegClass::General);
    GET_BACKEND_TARGET()->MapOperand(inst->GetResult(), target_result);
    auto fcmp_inst = GET_BACKEND_TARGET()->CreateInstruction<RVFLESInst>(target_result, target_lhs, target_rhs);
    GET_BACKEND_TARGET()->GetCurrentBlock()->AddInst(fcmp_inst);
    return;
  }
  // if predicate is EQ or NE, we can use the fcmp instruction and then check the result.
  if (predicate == backend::inst::CmpPredicate::EQ || predicate == backend::inst::CmpPredicate::NE) {
    auto target_result = GET_BACKEND_TARGET()->CreateVirtualRegister(TargetRegSize::DWord, TargetRegClass::General);
    GET_BACKEND_TARGET()->MapOperand(inst->GetResult(), target_result);
    auto fcmp_inst = GET_BACKEND_TARGET()->CreateInstruction<RVFEQSInst>(target_result, target_lhs, target_rhs);
    GET_BACKEND_TARGET()->GetCurrentBlock()->AddInst(fcmp_inst);
    if (predicate == backend::inst::CmpPredicate::NE) {
      // If predicate is NE, we need to negate the result.
      auto neg_result = GET_BACKEND_TARGET()->CreateVirtualRegister(TargetRegSize::DWord, TargetRegClass::General);
      auto not_inst = GET_BACKEND_TARGET()->CreateInstruction<RVXORIInst>(neg_result, target_result,
                                                                          GET_BACKEND_TARGET()->CreateIntLiteral(-1));
      GET_BACKEND_TARGET()->GetCurrentBlock()->AddInst(not_inst);
      GET_BACKEND_TARGET()->MapOperand(inst->GetResult(), neg_result);
    }
    return;
  }

  // Should not reach here, since we have handled all the predicates
  compiler_context_->ReportError("Unhandled FCmp predicate");
}

auto LowerHelper::LowerXor(backend::inst::XorInst *inst) -> void {
  auto lhs = inst->GetOperands()[0];
  auto rhs = inst->GetOperands()[1];
  auto dst = inst->GetResult();

  // If both operands are literals, report error since we have done constfold
  if (lhs->IsLiteral() && rhs->IsLiteral()) {
    compiler_context_->ReportError("Both operands are literals, should have been constant folded");
    return;
  }

  // If one of the operands is a literal, we can use the immediate instruction to optimize
  if (lhs->IsLiteral() || rhs->IsLiteral()) {
    auto literal_operand = lhs->IsLiteral() ? lhs : rhs;
    auto non_literal_operand = lhs->IsLiteral() ? rhs : lhs;

    auto literal_value = static_cast<utils::IntLiteral *>(literal_operand)->GetValue();
    auto target_non_literal = GET_BACKEND_TARGET()->GetMappedOperand(non_literal_operand);
    if (target_non_literal == nullptr) {
      compiler_context_->ReportError("Non-literal operand is not mapped to a target operand");
      return;
    }

    // If the literal is zero, we can just map the result to the non-literal operand
    if (literal_value == 0) {
      GET_BACKEND_TARGET()->MapOperand(dst, target_non_literal);
      return;
    }

    // If the literal is in 12-bit signed range, we can use the immediate instruction to optimize
    if (Is12BitSignedInteger(literal_value)) {
      auto target_result = GET_BACKEND_TARGET()->CreateVirtualRegister(TargetRegSize::DWord, TargetRegClass::General);
      GET_BACKEND_TARGET()->MapOperand(dst, target_result);
      auto target_literal = GET_BACKEND_TARGET()->CreateIntLiteral(literal_value);
      auto xor_inst =
          GET_BACKEND_TARGET()->CreateInstruction<RVXORIInst>(target_result, target_non_literal, target_literal);
      GET_BACKEND_TARGET()->GetCurrentBlock()->AddInst(xor_inst);
      return;
    }

    // Otherwise, we need to materialize the literal into a register and use the normal xor instruction
    auto target_literal = MaterializeLiteral(compiler_context_, literal_value);
    auto target_result = GET_BACKEND_TARGET()->CreateVirtualRegister(TargetRegSize::DWord, TargetRegClass::General);
    GET_BACKEND_TARGET()->MapOperand(dst, target_result);
    auto xor_inst =
        GET_BACKEND_TARGET()->CreateInstruction<RVXORInst>(target_result, target_non_literal, target_literal);
    GET_BACKEND_TARGET()->GetCurrentBlock()->AddInst(xor_inst);
    return;
  }

  // Otherwise, both operands are not literals, normally they should both be virtual registers
  auto target_lhs = GET_BACKEND_TARGET()->GetMappedOperand(lhs);
  auto target_rhs = GET_BACKEND_TARGET()->GetMappedOperand(rhs);
  if (target_lhs == nullptr || target_rhs == nullptr) {
    compiler_context_->ReportError("One of the operands is not mapped to a target operand");
    return;
  }
  auto target_result = GET_BACKEND_TARGET()->CreateVirtualRegister(TargetRegSize::DWord, TargetRegClass::General);
  GET_BACKEND_TARGET()->MapOperand(dst, target_result);
  auto xor_inst = GET_BACKEND_TARGET()->CreateInstruction<RVXORInst>(target_result, target_lhs, target_rhs);
  GET_BACKEND_TARGET()->GetCurrentBlock()->AddInst(xor_inst);
}

auto LowerHelper::LowerLoad(backend::inst::LoadInst *inst) -> void {
  //
}
auto LowerHelper::LowerStore([[maybe_unused]] backend::inst::StoreInst *inst) -> void {}

auto LowerHelper::LowerBr(backend::inst::BrInst *inst) -> void {
  // For unconditional branch, we can just create the label operand
  auto target_label = inst->GetTargetLabel();
  // Need to get the target block from the label operand
  if (target_label == nullptr) {
    compiler_context_->ReportError("Target block is not found");
    return;
  }

  auto target_block = GET_BACKEND_TARGET()->GetMappedBlock(target_label->GetId());

  auto target_label_operand = GET_BACKEND_TARGET()->CreateLabelOperand(target_block->GetName());

  // Then create the branch instruction
  auto br_inst = GET_BACKEND_TARGET()->CreateInstruction<RVJMPInst>(target_label_operand);
  GET_BACKEND_TARGET()->GetCurrentBlock()->AddInst(br_inst);
}

auto LowerHelper::LowerCondBr(backend::inst::BrCondInst *inst) -> void {
  // conditional branch instruction, we need to get the condition operand and the target label operand
  auto cond = inst->GetCondition();
  auto true_label = inst->GetTrueLabel();
  auto false_label = inst->GetFalseLabel();

  // Get the target operands for the condition and the labels
  if (cond->IsLiteral()) {
    auto literal_value = static_cast<utils::BoolLiteral *>(cond)->GetValue();
    if (literal_value) {
      // If the condition is true, we can just create an unconditional branch to the true label
      auto target_true_block = GET_BACKEND_TARGET()->GetMappedBlock(true_label->GetId());
      auto target_true_label_operand = GET_BACKEND_TARGET()->CreateLabelOperand(target_true_block->GetName());
      auto br_inst = GET_BACKEND_TARGET()->CreateInstruction<RVJMPInst>(target_true_label_operand);
      GET_BACKEND_TARGET()->GetCurrentBlock()->AddInst(br_inst);
      return;
    }
    // If the condition is false, we can just create an unconditional branch to the false label
    auto target_false_block = GET_BACKEND_TARGET()->GetMappedBlock(false_label->GetId());
    auto target_false_label_operand = GET_BACKEND_TARGET()->CreateLabelOperand(target_false_block->GetName());
    auto br_inst = GET_BACKEND_TARGET()->CreateInstruction<RVJMPInst>(target_false_label_operand);
    GET_BACKEND_TARGET()->GetCurrentBlock()->AddInst(br_inst);
    return;
  }

  // If the condition is not a literal, we need to get the target operand for the condition
  auto target_cond = GET_BACKEND_TARGET()->GetMappedOperand(cond);
  if (target_cond == nullptr) {
    compiler_context_->ReportError("Condition operand is not mapped to a target operand");
    return;
  }

  // Then we need to get the target operands for the true and false labels
  auto target_true_block = GET_BACKEND_TARGET()->GetMappedBlock(true_label->GetId());
  auto target_false_block = GET_BACKEND_TARGET()->GetMappedBlock(false_label->GetId());
  if (target_true_block == nullptr || target_false_block == nullptr) {
    compiler_context_->ReportError("One of the target blocks is not found");
    return;
  }

  auto target_true_label_operand = GET_BACKEND_TARGET()->CreateLabelOperand(target_true_block->GetName());
  auto target_false_label_operand = GET_BACKEND_TARGET()->CreateLabelOperand(target_false_block->GetName());

  // Create branch instructions for both true and false labels
  auto br_true_inst = GET_BACKEND_TARGET()->CreateInstruction<RVBEQInst>(target_cond, target_true_label_operand);
  GET_BACKEND_TARGET()->GetCurrentBlock()->AddInst(br_true_inst);
  auto br_false_inst = GET_BACKEND_TARGET()->CreateInstruction<RVJMPInst>(target_false_label_operand);
  GET_BACKEND_TARGET()->GetCurrentBlock()->AddInst(br_false_inst);
}

auto LowerHelper::LowerRet(backend::inst::RetInst *inst) -> void {
  if (auto return_value = inst->GetReturnValue()) {
    // If has a return value, we need to move it to the return register
    auto target_return_value = GET_BACKEND_TARGET()->GetMappedOperand(return_value);
    auto is_float_return = return_value->IsFloat();
    // If the return value is a literal, we need to materialize it into a register first
    if (return_value->IsLiteral()) {
      if (auto int_literal = dynamic_cast<utils::IntLiteral *>(return_value)) {
        target_return_value = MaterializeLiteral(compiler_context_, int_literal->GetValue());
      } else if (auto bool_literal = dynamic_cast<utils::BoolLiteral *>(return_value)) {
        target_return_value = MaterializeLiteral(compiler_context_, bool_literal->GetValue() ? 1 : 0);
      } else if (auto float_literal = dynamic_cast<utils::FloatLiteral *>(return_value)) {
        target_return_value = MaterializeLiteral(compiler_context_, float_literal->GetValue());
        is_float_return = true;
      } else {
        compiler_context_->ReportError("Return value is an unsupported literal");
        return;
      }
    }
    // Then the return value must be mapped to a target operand
    if (target_return_value == nullptr) {
      compiler_context_->ReportError("Return value is not mapped to a target operand");
      return;
    }
    if (is_float_return) {
      auto target_return_register = rv64_getter_.GetFloatReturnValueReg(GET_BACKEND_TARGET(), TargetRegSize::DWord);
      auto mv_inst = GET_BACKEND_TARGET()->CreateInstruction<RVFMVInst>(target_return_register, target_return_value);
      GET_BACKEND_TARGET()->GetCurrentBlock()->AddInst(mv_inst);
      auto ret_inst = GET_BACKEND_TARGET()->CreateInstruction<RVRETInst>(target_return_register);
      GET_BACKEND_TARGET()->GetCurrentBlock()->AddInst(ret_inst);
      return;
    }
    auto target_return_register = rv64_getter_.GetReturnValueReg(GET_BACKEND_TARGET(), TargetRegSize::DWord);
    auto mv_inst = GET_BACKEND_TARGET()->CreateInstruction<RVMVInst>(target_return_register, target_return_value);
    GET_BACKEND_TARGET()->GetCurrentBlock()->AddInst(mv_inst);
    auto ret_inst = GET_BACKEND_TARGET()->CreateInstruction<RVRETInst>(target_return_register);
    GET_BACKEND_TARGET()->GetCurrentBlock()->AddInst(ret_inst);
    return;
  }

  // void return : just insert the 'ret' instruction
  auto ret_inst = GET_BACKEND_TARGET()->CreateInstruction<RVRETInst>();
  GET_BACKEND_TARGET()->GetCurrentBlock()->AddInst(ret_inst);
}

auto LowerHelper::LowerCall(backend::inst::CallInst *inst) -> void {
  // For call instruction, we need to load all arguments into the ABI-defined argument registers.
  // auto args = inst->GetArguments();
  (void)inst;
}

auto LowerHelper::LowerZext(backend::inst::ZextInst *inst) -> void {
  auto src = inst->GetOperands()[0];
  auto dst = inst->GetResult();

  if (src->IsLiteral()) {
    auto literal_value = static_cast<utils::BoolLiteral *>(src)->GetValue();
    auto target_literal = GET_BACKEND_TARGET()->CreateIntLiteral(literal_value ? 1 : 0);
    GET_BACKEND_TARGET()->MapOperand(dst, target_literal);
    return;
  }

  auto target_src = GET_BACKEND_TARGET()->GetMappedOperand(src);
  if (target_src == nullptr) {
    compiler_context_->ReportError("Zext source operand is not mapped to a target operand");
    return;
  }

  auto target_result = GET_BACKEND_TARGET()->CreateVirtualRegister(TargetRegSize::DWord, TargetRegClass::General);
  GET_BACKEND_TARGET()->MapOperand(dst, target_result);
  auto zext_inst = GET_BACKEND_TARGET()->CreateInstruction<RVZextWInst>(target_result, target_src);
  GET_BACKEND_TARGET()->GetCurrentBlock()->AddInst(zext_inst);
}

auto LowerHelper::LowerInt2Float(backend::inst::Int2FloatInst *inst) -> void {
  auto src = inst->GetOperands()[0];
  auto dst = inst->GetResult();

  auto target_src = src->IsLiteral()
                        ? MaterializeLiteral(compiler_context_, static_cast<utils::IntLiteral *>(src)->GetValue())
                        : GET_BACKEND_TARGET()->GetMappedOperand(src);
  if (target_src == nullptr) {
    compiler_context_->ReportError("Int2Float source operand is not mapped to a target operand");
    return;
  }

  auto target_result = GET_BACKEND_TARGET()->CreateVirtualRegister(TargetRegSize::DWord, TargetRegClass::Float);
  GET_BACKEND_TARGET()->MapOperand(dst, target_result);
  auto cast_inst = GET_BACKEND_TARGET()->CreateInstruction<RVFCVTSWInst>(target_result, target_src);
  GET_BACKEND_TARGET()->GetCurrentBlock()->AddInst(cast_inst);
}

auto LowerHelper::LowerFloat2Int(backend::inst::Float2IntInst *inst) -> void {
  auto src = inst->GetOperands()[0];
  auto dst = inst->GetResult();

  auto target_src = src->IsLiteral()
                        ? MaterializeLiteral(compiler_context_, static_cast<utils::FloatLiteral *>(src)->GetValue())
                        : GET_BACKEND_TARGET()->GetMappedOperand(src);
  if (target_src == nullptr) {
    compiler_context_->ReportError("Float2Int source operand is not mapped to a target operand");
    return;
  }

  auto target_result = GET_BACKEND_TARGET()->CreateVirtualRegister(TargetRegSize::DWord, TargetRegClass::General);
  GET_BACKEND_TARGET()->MapOperand(dst, target_result);
  auto cast_inst = GET_BACKEND_TARGET()->CreateInstruction<RVFCVTWSInst>(target_result, target_src);
  GET_BACKEND_TARGET()->GetCurrentBlock()->AddInst(cast_inst);
}

auto LowerHelper::LowerBool2Float(backend::inst::Bool2FloatInst *inst) -> void {
  auto src = inst->GetOperands()[0];
  auto dst = inst->GetResult();

  backend::targetir::TargetOperand *target_src = nullptr;
  if (src->IsLiteral()) {
    target_src = MaterializeLiteral(compiler_context_, static_cast<utils::BoolLiteral *>(src)->GetValue() ? 1 : 0);
  } else {
    target_src = GET_BACKEND_TARGET()->GetMappedOperand(src);
  }
  if (target_src == nullptr) {
    compiler_context_->ReportError("Bool2Float source operand is not mapped to a target operand");
    return;
  }

  auto target_result = GET_BACKEND_TARGET()->CreateVirtualRegister(TargetRegSize::DWord, TargetRegClass::Float);
  GET_BACKEND_TARGET()->MapOperand(dst, target_result);
  auto cast_inst = GET_BACKEND_TARGET()->CreateInstruction<RVFCVTSWUInst>(target_result, target_src);
  GET_BACKEND_TARGET()->GetCurrentBlock()->AddInst(cast_inst);
}

auto LowerHelper::LowerCopy(backend::inst::CopyInst *inst) -> void {
  // Use mv instruction to copy the value from src to dst
  auto src = inst->GetOperands()[0];
  auto dst = inst->GetResult();

  // If the src is a literal, we need to materialize it into a register first
  backend::targetir::TargetOperand *target_src = nullptr;
  if (src->IsLiteral()) {
    if (auto int_literal = dynamic_cast<utils::IntLiteral *>(src)) {
      target_src = MaterializeLiteral(compiler_context_, int_literal->GetValue());
    } else if (auto bool_literal = dynamic_cast<utils::BoolLiteral *>(src)) {
      target_src = MaterializeLiteral(compiler_context_, bool_literal->GetValue() ? 1 : 0);
    } else if (auto float_literal = dynamic_cast<utils::FloatLiteral *>(src)) {
      target_src = MaterializeLiteral(compiler_context_, float_literal->GetValue());
    } else {
      compiler_context_->ReportError("Copy source is an unsupported literal");
      return;
    }
  } else {
    target_src = GET_BACKEND_TARGET()->GetMappedOperand(src);
  }

  if (target_src == nullptr) {
    compiler_context_->ReportError("Copy source operand is not mapped to a target operand");
    return;
  }

  // If the copy instruction's result has not been created,
  // create a virtual register for the result
  auto target_result = GET_BACKEND_TARGET()->GetMappedOperand(dst);
  if (target_result == nullptr) {
    const auto reg_class = dst->IsFloat() ? TargetRegClass::Float : TargetRegClass::General;
    target_result = GET_BACKEND_TARGET()->CreateVirtualRegister(TargetRegSize::DWord, reg_class);
    GET_BACKEND_TARGET()->MapOperand(dst, target_result);
  }

  auto mv_inst =
      dst->IsFloat()
          ? static_cast<TargetInst *>(GET_BACKEND_TARGET()->CreateInstruction<RVFMVInst>(target_result, target_src))
          : static_cast<TargetInst *>(GET_BACKEND_TARGET()->CreateInstruction<RVMVInst>(target_result, target_src));
  GET_BACKEND_TARGET()->GetCurrentBlock()->AddInst(mv_inst);
}

}  // namespace backend::targetir::rv64
