#include "backend/mir/utils/struct/block.h"
#include "backend/mir/inst/inst.h"

namespace backend::utils {
Block::~Block() { instructions_.clear(); }

auto Block::Emit(std::ofstream *out) const -> void {
  *out << "block" << id_ << ":\n";
  for (const auto &inst : instructions_) {
    inst->Emit(out);
  }
}

}  // namespace backend::utils