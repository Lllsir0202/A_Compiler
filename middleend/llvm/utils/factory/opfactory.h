#ifndef MIDDLEEND_LLVM_UTILS_FACTORY_OPFACTORY_H
#define MIDDLEEND_LLVM_UTILS_FACTORY_OPFACTORY_H

#include "middleend/llvm/utils/operand/labeloperand.h"
#include "middleend/llvm/utils/operand/literaloperand.h"
#include "middleend/llvm/utils/operand/regoperand.h"

namespace context {
class LLVMIRResourceContext;
}
namespace middleend::llvm::utils::factory {
class OperandFactory {
 private:
  static auto CreateRegOperand(size_t id) -> RegOperand * { return new RegOperand(id); }
  static auto CreateIntLiteral(int value) -> IntLiteral * { return new IntLiteral(value); }
  static auto CreateFloatLiteral(float value) -> FloatLiteral * { return new FloatLiteral(value); }
  static auto CreateLabelOperand(size_t id) -> LabelOperand * { return new LabelOperand(id); }
  friend class context::LLVMIRResourceContext;
};

}  // namespace middleend::llvm::utils::factory

#endif