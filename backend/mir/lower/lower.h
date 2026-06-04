#ifndef BACKEND_MIR_LOWER_LOWER_H
#define BACKEND_MIR_LOWER_LOWER_H
#include "backend/mir/lower/lowerhelper.h"
#include "context/compiler_context.h"

namespace backend::lower {
class MIRLower {
 public:
  explicit MIRLower(context::CompilerContext *compiler_context) : compiler_context_(compiler_context) {
    lower_helper_ = new LowerHelper(compiler_context);
  }
  ~MIRLower() { delete lower_helper_; }
  auto Lower() -> int;

 private:
  context::CompilerContext *compiler_context_;
  LowerHelper *lower_helper_;
};

}  // namespace backend::lower
#endif