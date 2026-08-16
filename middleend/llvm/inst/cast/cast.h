#ifndef MIDDLEEND_LLVM_INST_CAST_CAST_H
#define MIDDLEEND_LLVM_INST_CAST_CAST_H
#include "middleend/llvm/inst/inst.h"
#include "middleend/llvm/utils/operand/operand.h"
namespace middleend::llvm::inst {
class ZextInst : public Instruction {
 public:
  // Virtual override in instruction
  auto Emit(std::ofstream *out) const -> void override;
  auto Verify(context::CompilerContext *context) const -> bool override;

  auto GetSrc() const -> utils::Operand * { return operands_[0]; }

 private:
  ZextInst() : Instruction(OpKind::Zext) {}
  static auto Create(context::LLVMIRContext *context, utils::Operand *src) -> ZextInst *;
  friend class utils::factory::InstFactory;
};

class SextInst : public Instruction {
 public:
  auto Emit(std::ofstream *out) const -> void override;
  auto Verify(context::CompilerContext *context) const -> bool override;
  auto GetSrc() const -> utils::Operand * { return operands_[0]; }

 private:
  SextInst() : Instruction(OpKind::Sext) {}
  static auto Create(context::LLVMIRContext *context, utils::Operand *src) -> SextInst *;
  friend class utils::factory::InstFactory;
};

class TruncInst : public Instruction {
 public:
  auto Emit(std::ofstream *out) const -> void override;
  auto Verify(context::CompilerContext *context) const -> bool override;
  auto GetSrc() const -> utils::Operand * { return operands_[0]; }

 private:
  TruncInst() : Instruction(OpKind::Trunc) {}
  static auto Create(context::LLVMIRContext *context, utils::Operand *src) -> TruncInst *;
  friend class utils::factory::InstFactory;
};

class SitoFpInst : public Instruction {
 public:
  // Virtual override in instruction
  auto Emit(std::ofstream *out) const -> void override;
  auto Verify(context::CompilerContext *context) const -> bool override;

  auto GetSrc() const -> utils::Operand * { return operands_[0]; }

 private:
  SitoFpInst() : Instruction(OpKind::Sitofp) {}
  static auto Create(context::LLVMIRContext *context, utils::Operand *src) -> SitoFpInst *;
  friend class utils::factory::InstFactory;
};

class FptoSiInst : public Instruction {
 public:
  // Virtual override in instruction
  auto Emit(std::ofstream *out) const -> void override;
  auto Verify(context::CompilerContext *context) const -> bool override;

  auto GetSrc() const -> utils::Operand * { return operands_[0]; }

 private:
  FptoSiInst() : Instruction(OpKind::Fptosi) {}
  static auto Create(context::LLVMIRContext *context, utils::Operand *src) -> FptoSiInst *;
  friend class utils::factory::InstFactory;
};

class UitoFpInst : public Instruction {
 public:
  // Virtual override in instruction
  auto Emit(std::ofstream *out) const -> void override;
  auto Verify(context::CompilerContext *context) const -> bool override;

  auto GetSrc() const -> utils::Operand * { return operands_[0]; }

 private:
  UitoFpInst() : Instruction(OpKind::Uitofp) {}
  static auto Create(context::LLVMIRContext *context, utils::Operand *src) -> UitoFpInst *;
  friend class utils::factory::InstFactory;
};

}  // namespace middleend::llvm::inst

#endif
