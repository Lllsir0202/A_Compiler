#ifndef BACKEND_MIR_INST_INST_H
#define BACKEND_MIR_INST_INST_H
#include <fstream>
#include <vector>
#include "backend/mir/inst/opkind.h"

namespace context {
class CompilerContext;
class MIRContext;
}  // namespace context
namespace backend::utils {
class Operand;
class Block;
}  // namespace backend::utils
namespace backend::utils::factory {
class InstFactory;
}

namespace backend::inst {
class Instruction {
 public:
  virtual ~Instruction() = default;

  virtual auto Emit(std::ofstream *out) const -> void = 0;

  // Interface to access the instruction information
  auto GetResult() const -> utils::Operand * { return result_; }
  auto SetResult(utils::Operand *result) -> void { result_ = result; }

  auto GetOpKind() const -> OpKind { return op_kind_; }
  auto GetOperands() const -> const std::vector<utils::Operand *> & { return operands_; }

  auto AddOperand(utils::Operand *operand) -> void { operands_.emplace_back(operand); }

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

  auto GetParentBlock() const -> utils::Block * { return parent_block_; }
  auto SetParentBlock(utils::Block *block) -> void { parent_block_ = block; }
  virtual auto IsTerminal() const -> bool { return false; }
  virtual auto IsBranch() const -> bool { return false; }
  virtual auto IsCall() const -> bool { return false; }
  virtual auto IsPhi() const -> bool { return false; }

  virtual auto Verify(context::CompilerContext *context) const -> bool = 0;

 protected:
  explicit Instruction(OpKind op_kind) : op_kind_(op_kind) {}
  /**
   * @brief An instruction includes :
   * 1. a result
   * 2. an operation kind
   * 3. a list of operands
   */
  std::vector<utils::Operand *> operands_;
  utils::Operand *result_ = nullptr;
  friend class utils::factory::InstFactory;
  OpKind op_kind_;

  // Record the parent block
  utils::Block *parent_block_;

  // Record all users
  std::vector<Instruction *> users_;
};
}  // namespace backend::inst

#endif