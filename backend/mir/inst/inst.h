#ifndef BACKEND_MIR_INST_INST_H
#define BACKEND_MIR_INST_INST_H
#include <vector>
#include "backend/mir/inst/opkind.h"
namespace backend::utils {
class Operand;
class InstFactory;
}  // namespace backend::utils

namespace backend::inst {
class Instruction {
 public:
  virtual ~Instruction() = default;

  // Interface to access the instruction information
  auto GetResult() const -> utils::Operand * { return result_; }
  auto SetResult(utils::Operand *result) -> void { result_ = result; }

  auto AddUser(Instruction *user) -> void { users_.emplace_back(user); }
  auto GetUsers() const -> const std::vector<Instruction *> & { return users_; }
  auto ReplaceAllUsesWith(utils::Operand *new_operand) -> void {
    for (auto *user : users_) {
      for (auto &operand : user->operands_) {
        if (operand == result_) {
          operand = new_operand;
        }
      }
    }
    users_.clear();
  }

 protected:
  explicit Instruction(OpKind op_kind) : op_kind_(op_kind) {}

 private:
  /**
   * @brief An instruction includes :
   * 1. a result
   * 2. an operation kind
   * 3. a list of operands
   */
  std::vector<utils::Operand *> operands_;
  utils::Operand *result_ = nullptr;
  friend class utils::InstFactory;
  OpKind op_kind_;

  // Record all users
  std::vector<Instruction *> users_;
};
}  // namespace backend::inst

#endif