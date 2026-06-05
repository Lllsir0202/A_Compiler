#include "backend/targets/utils/struct/targetblock.h"
#include "backend/targets/utils/inst/targetinst.h"

namespace backend::targetir {
auto TargetBlock::Emit(std::ofstream *out) -> void {
  // Block be like: .Lmain_0:
  *out << ".L" << func_name_ << "_" << id_ << ":\n";
  for (auto inst : insts_) {
    inst->Emit(out);
  }
}

}  // namespace backend::targetir