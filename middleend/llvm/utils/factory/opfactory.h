#ifndef MIDDLEEND_LLVM_UTILS_FACTORY_OPFACTORY_H
#define MIDDLEEND_LLVM_UTILS_FACTORY_OPFACTORY_H

#include "middleend/llvm/utils/operand/labeloperand.h"
#include "middleend/llvm/utils/operand/literaloperand.h"
#include "middleend/llvm/utils/operand/regoperand.h"
namespace middleend::llvm::utils::factory {
class OperandFactory {
 public:
  static auto CreateRegOperand(size_t id) -> operand::RegOperand * { return new operand::RegOperand(id); }
  static auto CreateIntLiteral(int value) -> operand::IntLiteral * { return new operand::IntLiteral(value); }
  static auto CreateFloatLiteral(float value) -> operand::FloatLiteral * { return new operand::FloatLiteral(value); }
  static auto CreateLabelOperand(size_t id) -> operand::LabelOperand * { return new operand::LabelOperand(id); }
};

}  // namespace middleend::llvm::utils::factory

#endif