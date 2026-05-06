#ifndef CONTEXT_LLVMIR_CONTEXT_H
#define CONTEXT_LLVMIR_CONTEXT_H
#include "llvm/utils/operand/regoperand.h"
#include "middleend/llvm/utils/factory/opfactory.h"
namespace context {

/**
 * @brief LLVMIRLowerContext is responsible for lowering the base ir to llvmir
 */
class LLVMIRLowerContext {};

class LLVMIRResourceContext {};

class LLVMIRContext {
 public:
  // Get different types of operands from the context,
  // and we will ensure that the same operand will always be the same instance of operand.
  auto GetRegOperand() -> middleend::llvm::utils::operand::RegOperand *;

  auto GetLabelOperand() -> middleend::llvm::utils::operand::LabelOperand *;

  auto GetIntLiteral(int value) -> middleend::llvm::utils::operand::IntLiteral *;

  auto GetFloatLiteral(float value) -> middleend::llvm::utils::operand::FloatLiteral *;

 private:
  // Id counters for different operand types
  size_t reg_id_counter_ = 0;
  size_t label_id_counter_ = 0;
};

}  // namespace context
#endif