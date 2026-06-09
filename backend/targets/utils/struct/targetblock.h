#ifndef BACKEND_TARGETS_UTILS_STRUCT_TARGETBLOCK_H
#define BACKEND_TARGETS_UTILS_STRUCT_TARGETBLOCK_H
#include <fstream>
#include <utility>
#include <vector>
namespace backend::targetir {
class TargetInst;
class TargetOperand;

/**
 * @brief TargetBlock represents a block in target instruction level,
 * which contains a list of TargetInst and a list of successor blocks,
 * as well as a list of predecessor blocks.
 */
class TargetBlock {
 public:
  // Destructor: just use default
  ~TargetBlock() = default;
  // Output
  auto Emit(std::ofstream *out) -> void;

  // Interfaces to get, add, as well as remove instructions and successor/predecessor blocks.
  auto GetInsts() const -> const std::vector<TargetInst *> & { return insts_; }
  auto GetSuccessors() const -> const std::vector<TargetBlock *> & { return successors_; }
  auto GetPredecessors() const -> const std::vector<TargetBlock *> & { return predecessors_; }

  // Add-part
  auto AddInst(TargetInst *inst) -> void { insts_.emplace_back(inst); }
  auto AddSuccessor(TargetBlock *succ) -> void { successors_.emplace_back(succ); }
  auto AddPredecessor(TargetBlock *pred) -> void { predecessors_.emplace_back(pred); }

  // Get id
  auto GetID() const -> size_t { return id_; }

  // Set function name
  auto SetFuncName(std::string func_name) -> void { func_name_ = std::move(func_name); }

  // Interface to get and set live-in and live-out operands.
  auto GetLiveIn() const -> const std::vector<TargetOperand *> & { return live_in_; }
  auto GetLiveOut() const -> const std::vector<TargetOperand *> & { return live_out_; }

  auto AddLiveIn(TargetOperand *operand) -> void { live_in_.emplace_back(operand); }
  auto AddLiveOut(TargetOperand *operand) -> void { live_out_.emplace_back(operand); }
  auto SetLiveIn(const std::vector<TargetOperand *> &live_in) -> void { live_in_ = live_in; }
  auto SetLiveOut(const std::vector<TargetOperand *> &live_out) -> void { live_out_ = live_out; }

 private:
  // Constructor with id
  explicit TargetBlock(size_t id) : id_(id) {}
  friend class BlockFactory;
  std::vector<TargetInst *> insts_;
  std::vector<TargetBlock *> successors_;
  std::vector<TargetBlock *> predecessors_;

  // Unique identifier, and it will be unique in the same function
  size_t id_;

  // Function name
  std::string func_name_;

  // Record the live-in and live-out operands of this block,
  // which will be used for register allocation
  std::vector<TargetOperand *> live_in_;
  std::vector<TargetOperand *> live_out_;
};

}  // namespace backend::targetir

#endif