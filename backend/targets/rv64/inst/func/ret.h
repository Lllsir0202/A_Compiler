#ifndef BACKEND_TARGETS_RV64_INST_FUNC_RET_H
#define BACKEND_TARGETS_RV64_INST_FUNC_RET_H
#include "backend/targets/utils/factory/instfactory.h"
#include "backend/targets/utils/inst/targetinst.h"
#include "backend/targets/utils/operand/targetoperand.h"
namespace backend::targetir::rv64 {
class RVRETInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

 private:
  RVRETInst() = default;
  static auto Create(TargetOperand *operand) -> RVRETInst *;
  static auto Create() -> RVRETInst *;
  friend class targetir::InstFactory;
};

}  // namespace backend::targetir::rv64

#endif