#ifndef BACKEND_MIR_UTILS_FACTORY_OPFACTORY_H
#define BACKEND_MIR_UTILS_FACTORY_OPFACTORY_H
#include "backend/mir/utils/operand/globaloperand.h"
#include "backend/mir/utils/operand/labeloperand.h"
#include "backend/mir/utils/operand/literaloperand.h"
#include "backend/mir/utils/operand/regoperand.h"
namespace backend::utils::factory {
class Operandfactory {
  // Interface to create operand,
  // set backend context as a friend class,
  // and only the context can access the interface to create operands
  static auto CreateRegOperand(size_t reg_num) -> Operand * { return new RegOperand(reg_num); }
  static auto CreateIntLiteral(int value) -> Operand * { return new IntLiteral(value); }
  static auto CreateFloatLiteral(float value) -> Operand * { return new FloatLiteral(value); }
  static auto CreateLabelOperand(size_t id) -> Operand * { return new LabelOperand(id); }
  static auto CreateGlobalOperand(const std::string &name) -> Operand * { return new GlobalOperand(name); }
  static auto CreateBoolLiteral(bool value) -> Operand * { return new BoolLiteral(value); }
  static auto CreateLongLiteral(std::int64_t value) -> Operand * { return new LongLiteral(value); }
  static auto CreateCharLiteral(char value) -> Operand * { return new CharLiteral(value); }
};

}  // namespace backend::utils::factory

#endif