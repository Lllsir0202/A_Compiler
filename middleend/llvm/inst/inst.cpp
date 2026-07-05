#include "middleend/llvm/inst/inst.h"
#include <algorithm>
#include "middleend/llvm/utils/operand/operand.h"

namespace middleend::llvm::inst {
auto Instruction::ReplaceAllUsersWith(utils::Operand *new_operand) -> void {
  for (auto *user : users_) {
    for (auto &operand : user->operands_) {
      if (operand == result_) {
        operand = new_operand;
        // Add the user to the new operand's users
        if (new_operand->GetInstruction() != nullptr) {
          if (operand->GetInstruction() != nullptr) {
            new_operand->GetInstruction()->AddUser(user);
          }
        }
      }
    }
  }
  // Clear the users after replacement
  users_.clear();
}

auto Instruction::AreAllOperandsConstants() const -> bool {
  return std::all_of(operands_.begin(), operands_.end(),
                     [](const utils::Operand *operand) { return operand->Isconstant(); });
}

}  // namespace middleend::llvm::inst