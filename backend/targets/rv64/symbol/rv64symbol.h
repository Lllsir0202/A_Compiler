#ifndef BACKEND_TARGETS_RV64_SYMBOL_RV64SYMBOL_H
#define BACKEND_TARGETS_RV64_SYMBOL_RV64SYMBOL_H

#include <fstream>
#include "backend/targets/utils/operand/targetsymbol.h"
namespace backend::targetir {
// RiscV64 symbol access mode
enum class RV64SymbolModifier : uint8_t {
  None,
  Hi,          // High 20 bits of the symbol's address, used for LUI instruction
  Lo,          // Low 12 bits of the symbol's address, used for ADDI instruction
  PCRelHi,     // High 20 bits of the symbol's address relative to the program counter, used for PC-relative addressing
  PCRelLo,     // Low 12 bits of the symbol's address relative to the program
  GOTPCRelHi,  // High 20 bits of the symbol's address relative to the program counter,
               // used for PC-relative addressing through GOT
};

class RV64Symbol : public TargetSymbol {
 public:
  explicit RV64Symbol(std::string name, RV64SymbolModifier modifier)
      : TargetSymbol(std::move(name)), modifier_(modifier) {}

  auto Emit(std::ofstream *out) -> void override;

 private:
  RV64SymbolModifier modifier_;
};

}  // namespace backend::targetir
#endif