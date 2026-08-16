#ifndef BACKEND_MIR_INST_CAST_CAST_H
#define BACKEND_MIR_INST_CAST_CAST_H

#include "backend/mir/inst/inst.h"
#include "context/mir_context.h"
namespace backend::inst {
class ZextInst : public Instruction {
 public:
  auto Verify(context::CompilerContext *context) const -> bool override;
  auto Emit(std::ofstream *out) const -> void override;

 private:
  explicit ZextInst() : Instruction(OpKind::Zext) {}
  static auto Create(context::MIRContext *context, utils::Operand *src) -> ZextInst *;
  friend class utils::factory::InstFactory;
};

class SextInst : public Instruction {
 public:
  auto Verify(context::CompilerContext *context) const -> bool override;
  auto Emit(std::ofstream *out) const -> void override;

 private:
  explicit SextInst() : Instruction(OpKind::Sext) {}
  static auto Create(context::MIRContext *context, utils::Operand *src) -> SextInst *;
  friend class utils::factory::InstFactory;
};

class TruncInst : public Instruction {
 public:
  auto Verify(context::CompilerContext *context) const -> bool override;
  auto Emit(std::ofstream *out) const -> void override;

 private:
  explicit TruncInst() : Instruction(OpKind::Trunc) {}
  static auto Create(context::MIRContext *context, utils::Operand *src) -> TruncInst *;
  friend class utils::factory::InstFactory;
};

class Int2FloatInst : public Instruction {
 public:
  auto Verify(context::CompilerContext *context) const -> bool override;
  auto Emit(std::ofstream *out) const -> void override;

 private:
  explicit Int2FloatInst() : Instruction(OpKind::Int2Float) {}
  static auto Create(context::MIRContext *context, utils::Operand *src) -> Int2FloatInst *;
  friend class utils::factory::InstFactory;
};

class Float2IntInst : public Instruction {
 public:
  auto Verify(context::CompilerContext *context) const -> bool override;
  auto Emit(std::ofstream *out) const -> void override;

 private:
  explicit Float2IntInst() : Instruction(OpKind::Float2Int) {}
  static auto Create(context::MIRContext *context, utils::Operand *src) -> Float2IntInst *;
  friend class utils::factory::InstFactory;
};

class Bool2FloatInst : public Instruction {
 public:
  auto Verify(context::CompilerContext *context) const -> bool override;
  auto Emit(std::ofstream *out) const -> void override;

 private:
  explicit Bool2FloatInst() : Instruction(OpKind::Bool2Float) {}
  static auto Create(context::MIRContext *context, utils::Operand *src) -> Bool2FloatInst *;
  friend class utils::factory::InstFactory;
};

}  // namespace backend::inst

#endif
