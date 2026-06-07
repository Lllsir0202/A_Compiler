#ifndef BACKEND_TARGETS_RV64_INST_FUNC_RET_H
#define BACKEND_TARGETS_RV64_INST_FUNC_RET_H
#include "backend/targets/utils/inst/targetinst.h"
namespace backend::targetir::rv64 {
class RVRETInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

 private:
  static auto Create() -> RVRETInst *;
  friend class InstFactory;
};

}  // namespace backend::targetir::rv64

#endif