#ifndef BACKEND_MIR_INST_OPKIND_H
#define BACKEND_MIR_INST_OPKIND_H

#include <cstdint>
namespace backend::inst {
enum class OpKind : uint8_t {
  // Arithemic operations
  // Integer
  Add,
  Sub,
  Mul,
  Div,
  Mod,
  // Float
  AddF,
  SubF,
  MulF,
  DivF,
  // Comparison operations
  // Integer
  ICmp,
  // Float
  FCmp,
  // Logical operations
  And,
  Or,
  Xor,
  // Shift operations
  Shl,
  LShr,
  AShr,
  // Memory operations
  Load,
  Store,
  // Control flow operations
  Br,
  CondBr,
  Ret,
  // Other operations
  FuncDef,
  FuncDeclare,
  Call,
  GlobalDef,

  // Cast operations
  Zext,
  Int2Float,
  Float2Int,
  Bool2Float,

  // Pseudo operations
  Copy,
  Phi

};

}  // namespace backend::inst

#endif