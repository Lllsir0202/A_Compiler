#ifndef BACKEND_TARGETS_RV64_UTILS_MATERIALIZER_H
#define BACKEND_TARGETS_RV64_UTILS_MATERIALIZER_H

#include "backend/targets/rv64/inst/pseudo/pseudo.h"
#include "context/compiler_context.h"
namespace backend::targetir::rv64 {

// Materialize a literal value into a target operand,
inline auto MaterializeLiteral(context::CompilerContext *compiler_context,
                               int literal_value) -> backend::targetir::TargetOperand * {
  auto target_reg =
      compiler_context->GetBackendContext()->CreateVirtualRegister(TargetRegSize::DWord, TargetRegClass::General);
  auto target_literal = compiler_context->GetBackendContext()->CreateIntLiteral(literal_value);
  auto li_inst = compiler_context->GetBackendContext()->CreateInstruction<RVLIInst>(target_reg, target_literal);
  compiler_context->GetBackendContext()->GetCurrentBlock()->AddInst(li_inst);
  return target_reg;
}

}  // namespace backend::targetir::rv64

#endif