#ifndef BACKEND_TARGETS_AARCH64_SYMBOL_AARCH64SYMBOL_H
#define BACKEND_TARGETS_AARCH64_SYMBOL_AARCH64SYMBOL_H

#include "backend/targets/utils/operand/targetsymbol.h"
namespace backend::targetir {
enum class AArch64SymbolModifier : uint8_t {
  None,
  Hi,          // High 20 bits of the symbol's address, used for LUI instruction
  Lo,          // Low 12 bits of the symbol's address, used for ADDI instruction
  PCRelHi,     // High 20 bits of the symbol's address relative to the program counter, used for PC-relative addressing
  PCRelLo,     // Low 12 bits of the symbol's address relative to the program
  GOTPCRelHi,  // High 20 bits of the symbol's address relative to the program counter,
               // used for PC-relative addressing through GOT
};

class AArch64Symbol : public TargetSymbol {
 public:
  explicit AArch64Symbol(std::string name, AArch64SymbolModifier modifier)
      : TargetSymbol(std::move(name)), modifier_(modifier) {}

  auto Emit(std::ofstream *out) -> void override;

 private:
  AArch64SymbolModifier modifier_;
};

}  // namespace backend::targetir

#endif