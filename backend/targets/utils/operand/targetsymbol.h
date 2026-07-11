#ifndef BACKEND_TARGETS_UTILS_OPERAND_TARGETSYMBOL_H
#define BACKEND_TARGETS_UTILS_OPERAND_TARGETSYMBOL_H

#include <string>
#include <utility>
#include "backend/targets/utils/operand/targetoperand.h"

namespace backend::targetir {
class TargetContext;

/// A basic-block label used as a branch / jump target, e.g. `.L_fn_3`.
class TargetLabel : public TargetOperand {
 public:
  auto Emit(std::ofstream *out) -> void override { *out << name_; }
  auto GetName() const -> const std::string & { return name_; }

 private:
  explicit TargetLabel(std::string name) : TargetOperand(TargetOperandType::Label), name_(std::move(name)) {}
  std::string name_;
  friend class TargetContext;
};

/// A named symbol (function or global) used by `call` / `la`, e.g. `main`.
class TargetSymbol : public TargetOperand {
 public:
  auto Emit(std::ofstream *out) -> void override { *out << name_; }
  auto GetName() const -> const std::string & { return name_; }

 private:
  explicit TargetSymbol(std::string name) : TargetOperand(TargetOperandType::Symbol), name_(std::move(name)) {}
  std::string name_;
  friend class TargetContext;
};

}  // namespace backend::targetir

#endif
