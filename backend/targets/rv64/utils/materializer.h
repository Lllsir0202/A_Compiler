#ifndef BACKEND_TARGETS_RV64_UTILS_MATERIALIZER_H
#define BACKEND_TARGETS_RV64_UTILS_MATERIALIZER_H

#include "backend/targets/rv64/inst/pseudo/pseudo.h"
#include "context/compiler_context.h"
#include "targets/rv64/inst/memory/load.h"
#include "targets/rv64/symbol/rv64symbol.h"
namespace backend::targetir::rv64 {

// Materialize a literal value into a target operand.
inline auto MaterializeLiteral(context::CompilerContext *compiler_context,
                               int literal_value) -> backend::targetir::TargetOperand * {
  auto target_reg =
      compiler_context->GetBackendContext()->CreateVirtualRegister(TargetRegSize::DWord, TargetRegClass::General);
  auto target_literal = compiler_context->GetBackendContext()->CreateIntLiteral(literal_value);
  auto li_inst = compiler_context->GetBackendContext()->CreateInstruction<RVLIInst>(target_reg, target_literal);
  compiler_context->GetBackendContext()->GetCurrentBlock()->AddInst(li_inst);
  return target_reg;
}

// Materialize a float literal value into a target operand.
/**
 * @note Note that this function is used to load a float literal into a register,
 * currently, we use following strategy to materialize a float literal:
 * 1. For those literals that using frequently,
 * we can use li to load them directly to a float register
 * and record them in a local literal pool, and reuse them later.
 * 2. For those literals that are not used frequently,
 * we can load them from a function-local literal pool.
 */
inline auto MaterializeLiteral(context::CompilerContext *compiler_context,
                               float literal_value) -> backend::targetir::TargetOperand * {
  // If the literal value is frequently used
  if (compiler_context->GetBackendContext()->GetCurrentFunction()->IsFloatLiteralFrequentlyUsed(literal_value)) {
    // Get the mapped target operand for this literal value, if it exists
    auto target_operand =
        compiler_context->GetBackendContext()->GetCurrentFunction()->GetOperandForFloatLiteral(literal_value);
    if (target_operand == nullptr) {
      // If the target operand does not exist, create a new virtual register for it
      auto target_reg =
          compiler_context->GetBackendContext()->CreateVirtualRegister(TargetRegSize::DWord, TargetRegClass::Float);
      // Create a li instruction to load the literal value into a int register,
      // and then use flw to move the int register to the float register
      auto target_int_reg =
          compiler_context->GetBackendContext()->CreateVirtualRegister(TargetRegSize::DWord, TargetRegClass::General);
      auto target_literal = compiler_context->GetBackendContext()->CreateFloatLiteral(literal_value);
      auto li_inst = compiler_context->GetBackendContext()->CreateInstruction<RVLIInst>(target_int_reg, target_literal);
      compiler_context->GetBackendContext()->GetCurrentBlock()->AddInst(li_inst);
      auto zero_literal = compiler_context->GetBackendContext()->CreateIntLiteral(0);
      auto flw_inst =
          compiler_context->GetBackendContext()->CreateInstruction<RVFLWInst>(target_reg, zero_literal, target_int_reg);
      compiler_context->GetBackendContext()->GetCurrentBlock()->AddInst(flw_inst);
      // Map the literal value to the target operand
      compiler_context->GetBackendContext()->GetCurrentFunction()->SetOperandForFloatLiteral(literal_value, target_reg);
      return target_reg;
    }
    // Otherwise, return the mapped target operand
    return target_operand;
  }
  // If the literal value is not frequently used, we can load it from the function literal pool.
  auto target_reg =
      compiler_context->GetBackendContext()->CreateVirtualRegister(TargetRegSize::DWord, TargetRegClass::Float);
  // Get the literal pool entry for this literal value, if it exists.
  auto literal_pool_entry =
      compiler_context->GetBackendContext()->GetCurrentFunction()->GetFloatLiteralPoolEntry(literal_value);
  if (literal_pool_entry == nullptr) {
    auto offset = compiler_context->GetBackendContext()->GetCurrentFunction()->GetFloatLiteralPoolSize() * 4;
    compiler_context->GetBackendContext()->GetCurrentFunction()->UpdateFloatLiteralPoolEntry(
        literal_value, offset, compiler_context->GetBackendContext()->GetCurrentFunction()->GetFloatLiteralPoolSize());
  }
  // Get the index of the literal pool entry for loading the literal value from the literal pool.
  literal_pool_entry =
      compiler_context->GetBackendContext()->GetCurrentFunction()->GetFloatLiteralPoolEntry(literal_value);
  auto tmp_reg =
      compiler_context->GetBackendContext()->CreateVirtualRegister(TargetRegSize::DWord, TargetRegClass::General);
  auto symbol_operand =
      compiler_context->GetBackendContext()->CreateSymbolOperand<RV64Symbol>(literal_pool_entry->GetName());
  // Create a 'la' instruction to load the address of the literal pool
  auto la_inst = compiler_context->GetBackendContext()->CreateInstruction<RVLAInst>(tmp_reg, symbol_operand);
  compiler_context->GetBackendContext()->GetCurrentBlock()->AddInst(la_inst);
  // Create a 'flw' instruction to load the float literal value from the literal pool
  auto zero_literal = compiler_context->GetBackendContext()->CreateIntLiteral(0);
  auto flw_inst =
      compiler_context->GetBackendContext()->CreateInstruction<RVFLWInst>(target_reg, zero_literal, tmp_reg);
  compiler_context->GetBackendContext()->GetCurrentBlock()->AddInst(flw_inst);

  return target_reg;
}
}  // namespace backend::targetir::rv64

#endif