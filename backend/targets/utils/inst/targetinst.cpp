#include "backend/targets/utils/inst/targetinst.h"

namespace backend::targetir {
auto TargetInst::ReplaceOperand(TargetOperand *old_operand, TargetOperand *new_operand) -> void {
  // Replace all occurrences of old_operand with new_operand in operands_
  for (auto &operand : operands_) {
    if (operand == old_operand) {
      operand = new_operand;
    }
  }
}

}  // namespace backend::targetir