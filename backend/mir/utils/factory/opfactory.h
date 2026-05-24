#ifndef BACKEND_MIR_UTILS_FACTORY_OPFACTORY_H
#define BACKEND_MIR_UTILS_FACTORY_OPFACTORY_H
#include "backend/mir/utils/operand/globaloperand.h"
#include "backend/mir/utils/operand/labeloperand.h"
#include "backend/mir/utils/operand/literaloperand.h"
#include "backend/mir/utils/operand/memoryoperand.h"
#include "backend/mir/utils/operand/regoperand.h"
namespace context {
class MIRResourceContext;
}
namespace backend::utils::factory {
class OperandFactory {
  // Interface to create operand,
  // set backend context as a friend class,
  // and only the context can access the interface to create operands
  static auto CreateRegOperand(size_t reg_num) -> RegOperand * { return new RegOperand(reg_num); }
  static auto CreateIntLiteral(int value) -> IntLiteral * { return new IntLiteral(value); }
  static auto CreateFloatLiteral(float value) -> FloatLiteral * { return new FloatLiteral(value); }
  static auto CreateLabelOperand(size_t id) -> LabelOperand * { return new LabelOperand(id); }
  static auto CreateGlobalOperand(const std::string &name) -> GlobalOperand * { return new GlobalOperand(name); }
  static auto CreateBoolLiteral(bool value) -> BoolLiteral * { return new BoolLiteral(value); }
  static auto CreateLongLiteral(std::int64_t value) -> LongLiteral * { return new LongLiteral(value); }
  static auto CreateCharLiteral(char value) -> CharLiteral * { return new CharLiteral(value); }
  static auto CreateMemoryOperand(size_t reg_num) -> MemoryOperand * { return new MemoryOperand(reg_num); }

  friend class context::MIRResourceContext;
};

}  // namespace backend::utils::factory

#endif