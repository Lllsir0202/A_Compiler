#ifndef BACKEND_MIR_UTILS_STRUCT_BLOCK_H
#define BACKEND_MIR_UTILS_STRUCT_BLOCK_H

#include <deque>
#include <fstream>
namespace backend::inst {
class Instruction;
}
namespace backend::utils::factory {
class BlockFactory;
}
namespace backend::utils {
// This class represents the block in the mir
class Block {
 public:
  auto Emit(std::ofstream *out) const -> void;
  ~Block();
  // Interface to access the block information
  auto GetInstructions() const -> const std::deque<inst::Instruction *> & { return instructions_; }
  auto SetInstructions(const std::deque<inst::Instruction *> &instructions) -> void { instructions_ = instructions; }
  auto AddInstFront(inst::Instruction *inst) -> void { instructions_.push_front(inst); }
  auto AddInstBack(inst::Instruction *inst) -> void { instructions_.push_back(inst); }

  auto GetId() const -> size_t { return id_; }

 private:
  explicit Block(size_t id) : id_(id) {}
  // Every block record a list of instructions
  std::deque<inst::Instruction *> instructions_;

  // Record a unique block id in a function
  const size_t id_;
  friend class factory::BlockFactory;
};

}  // namespace backend::utils

#endif