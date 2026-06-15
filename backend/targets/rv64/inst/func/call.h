#ifndef BACKEND_TARGETS_RV64_INST_FUNC_CALL_H
#define BACKEND_TARGETS_RV64_INST_FUNC_CALL_H

#include "backend/targets/utils/inst/targetinst.h"
namespace backend::targetir::rv64 {
class RVCALLInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

 private:
  RVCALLInst() = default;
  // In RV64, we will store all the args before the call instruction
  static auto Create(TargetOperand *rd, std::string callee, const std::vector<TargetOperand *> &args) -> RVCALLInst *;
  friend class InstFactory;
  // Record the callee function name
  std::string callee_;
};

}  // namespace backend::targetir::rv64

#endif