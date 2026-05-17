#ifndef MIDDLEEND_LLVM_UTILS_FACTORY_OPFACTORY_H
#define MIDDLEEND_LLVM_UTILS_FACTORY_OPFACTORY_H

#include "middleend/llvm/utils/operand/globaloperand.h"
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
  static auto CreateGlobalOperand(std::string name) -> GlobalOperand * { return new GlobalOperand(std::move(name)); }
  static auto CreateBoolLiteral(bool value) -> BoolLiteral * { return new BoolLiteral(value); }
  static auto CreateLongLiteral(std::int64_t value) -> LongLiteral * { return new LongLiteral(value); }
  static auto CreateCharLiteral(char value) -> CharLiteral * { return new CharLiteral(value); }
  friend class context::LLVMIRResourceContext;
};

}  // namespace middleend::llvm::utils::factory

#endif