#ifndef BACKEND_TARGETS_RV64_INST_FUNC_CALL_H
#define BACKEND_TARGETS_RV64_INST_FUNC_CALL_H

#include "backend/targets/utils/factory/instfactory.h"
#include "backend/targets/utils/inst/targetinst.h"
namespace backend::targetir::rv64 {
class RVCALLInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

  // Set result operand
  auto SetResult(TargetOperand *rd) -> void { result_ = rd; }

 private:
  RVCALLInst() = default;
  // In RV64, we will store all the args before the call instruction
  static auto Create(std::string callee, const std::vector<TargetOperand *> &args) -> RVCALLInst *;
  friend class targetir::InstFactory;
  // Record the callee function name
  std::string callee_;
};

}  // namespace backend::targetir::rv64

#endif