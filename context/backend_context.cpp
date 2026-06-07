#include "context/backend_context.h"
#include "backend/targets/utils/operand/targetoperand.h"

namespace context {
BackendContext::~BackendContext() {
  for (auto operand : operands_) {
    delete operand;
  }
  operands_.clear();
  delete module_;
  module_ = nullptr;
}
}  // namespace context