#ifndef MIDDLEEND_LLVM_INST_OPKIND_H
#define MIDDLEEND_LLVM_INST_OPKIND_H

#include <cstdint>
namespace middleend::llvm::inst {
/**
 * @note Not all opkind might be used in our llvm ir
 */
enum class OpKind : uint8_t {
  // Arithmetic operations
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
  Call,
  Alloca,
  GlobalDef,
  GetElementPtr,
  Phi,
  // Bitcast and other type conversion operations
  Bitcast,
  // Boolean 2 Integer
  // Use zext
  Zext,
  // Integer 2 Boolean
  // Use icmp ne with 0
  // Int 2 Float
  // Use sitofp
  Sitofp,
  // Float 2 Int
  // Use fptosi
  Fptosi,
  // Float 2 Bool
  // Use fcmp oeq with 0.0
  // Bool 2 Float
  // UitoFp
  Uitofp
};

}  // namespace middleend::llvm::inst

#endif