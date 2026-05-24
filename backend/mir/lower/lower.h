#ifndef BACKEND_MIR_LOWER_LOWER_H
#define BACKEND_MIR_LOWER_LOWER_H
#include "context/compiler_context.h"

namespace backend::lower {
class MIRLower {
 public:
  explicit MIRLower(context::CompilerContext *compiler_context) : compiler_context_(compiler_context) {}
  auto Lower() -> int;

 private:
  context::CompilerContext *compiler_context_;
};

}  // namespace backend::lower
#endif