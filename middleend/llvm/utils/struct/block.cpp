#include "middleend/llvm/utils/struct/block.h"
#include "middleend/llvm/inst/inst.h"

namespace middleend::llvm::utils {
BasicBlock::~BasicBlock() { instructions_.clear(); }

auto BasicBlock::Emit(std::ofstream *out) const -> void {
  *out << "label" << id_ << ":  ; " << comment_ << "\n";
  for (const auto &inst : instructions_) {
    inst->Emit(out);
  }
}

}  // namespace middleend::llvm::utils