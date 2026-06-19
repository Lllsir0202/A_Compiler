#ifndef BACKEND_MIR_LOWER_LOWER_H
#define BACKEND_MIR_LOWER_LOWER_H
#include "backend/mir/lower/lowerhelper.h"
#include "context/compiler_context.h"
#include "mir/lower/phielim.h"

namespace backend::lower {
class MIRLower {
 public:
  explicit MIRLower(context::CompilerContext *compiler_context) : compiler_context_(compiler_context) {
    lower_helper_ = new LowerHelper(compiler_context);
    phi_elimination_ = new PhiElimination(compiler_context_->GetMIRContext());
  }
  ~MIRLower() {
    delete lower_helper_;
    delete phi_elimination_;
  }
  auto Lower() -> int;

 private:
  context::CompilerContext *compiler_context_;
  LowerHelper *lower_helper_;
  PhiElimination *phi_elimination_;
};

}  // namespace backend::lower
#endif