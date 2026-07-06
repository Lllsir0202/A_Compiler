#include "backend/targets/utils/struct/targetblock.h"
#include "backend/targets/utils/inst/targetinst.h"

namespace backend::targetir {
TargetBlock::~TargetBlock() {
  for (auto inst : insts_) {
    delete inst;
  }
  insts_.clear();
}

auto TargetBlock::Emit(std::ofstream *out) -> void {
  // Block be like: .Lmain_0:
  *out << ".L" << func_name_ << "_" << id_ << ":\n";
  for (auto inst : insts_) {
    inst->Emit(out);
  }
}

}  // namespace backend::targetir