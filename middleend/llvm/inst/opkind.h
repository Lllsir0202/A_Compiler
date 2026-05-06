#ifndef MIDDLEEND_LLVM_INST_OPKIND_H
#define MIDDLEEND_LLVM_INST_OPKIND_H

namespace middleend::llvm::inst {
/**
 * @note Not all opkind might be used in our llvm ir
 */
enum class OpKind {
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
  ICmpEQ,
  ICmpNE,
  ICmpLT,
  ICmpLE,
  ICmpGT,
  ICmpGE,
  // Float
  FCmpOEQ,
  FCmpONE,
  FCmpOLT,
  FCmpOLE,
  FCmpOGT,
  FCmpOGE,
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
  Call,
  Alloca,
  GetElementPtr,
  Phi,
  // Bitcast and other type conversion operations
  Bitcast,
  // Boolean 2 Integer
  Bool2Int,
  // Integer 2 Boolean
  Int2Bool,
  // Int 2 Float
  Int2Float,
  // Float 2 Int
  Float2Int,
  // Float 2 Bool
  Float2Bool,
  // Bool 2 Float
  Bool2Float,
};

}  // namespace middleend::llvm::inst

#endif