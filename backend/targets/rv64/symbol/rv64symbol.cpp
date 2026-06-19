#include "backend/targets/rv64/symbol/rv64symbol.h"

namespace backend::targetir {
auto RV64Symbol::Emit(std::ofstream *out) -> void {
  switch (modifier_) {
    case RV64SymbolModifier::None:
      *out << GetName();
      break;
    case RV64SymbolModifier::Hi:
      *out << "%hi(" << GetName() << ")";
      break;
    case RV64SymbolModifier::Lo:
      *out << "%lo(" << GetName() << ")";
      break;
    case RV64SymbolModifier::PCRelHi:
      *out << "%pcrel_hi(" << GetName() << ")";
      break;
    case RV64SymbolModifier::PCRelLo:
      *out << "%pcrel_lo(" << GetName() << ")";
      break;
    case RV64SymbolModifier::GOTPCRelHi:
      *out << "%got_pcrel_hi(" << GetName() << ")";
      break;
  }
}

}  // namespace backend::targetir