#ifndef BACKEND_TARGETS_UTILS_OPERAND_TARGETSYMBOL_H
#define BACKEND_TARGETS_UTILS_OPERAND_TARGETSYMBOL_H

#include "backend/targets/utils/operand/targetoperand.h"
#include "backend/targets/utils/operand/targetprinter.h"
namespace backend::targetir {
class TargetSymbol : public TargetOperand {
 public:
  explicit TargetSymbol(std::string name) : TargetOperand(TargetOperandType::Symbol), name_(std::move(name)) {}
  auto Emit(std::ofstream *out) -> void override {
    if (symbol_printer == nullptr) {
      // Should report error
      throw std::runtime_error("Target symbol printer is not set for symbol printing");
    }
    *out << symbol_printer->GetSymbolName(name_, access_mode_);
  }

  // Set the target symbol printer for symbol printing
  static auto SetTargetSymbolPrinter(const TargetSymbolPrinter *printer) -> void { symbol_printer = printer; }

 private:
  std::string name_;
  // Record the access mode of the symbol,
  // e.g. PC_relative, absolute, etc.
  // This is used for symbol printing in different architectures.
  TargetSymbolAccessMode access_mode_ = TargetSymbolAccessMode::None;
  static const TargetSymbolPrinter *symbol_printer;
};

}  // namespace backend::targetir

#endif