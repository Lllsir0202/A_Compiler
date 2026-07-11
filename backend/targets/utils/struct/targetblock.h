#ifndef BACKEND_TARGETS_UTILS_STRUCT_TARGETBLOCK_H
#define BACKEND_TARGETS_UTILS_STRUCT_TARGETBLOCK_H
#include <fstream>
#include <string>
#include <utility>
#include <vector>
namespace backend::targetir {
class TargetInst;
class TargetOperand;
class TargetContext;

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

  // Get and Set id
  auto GetID() const -> size_t { return id_; }

  // Assembly label for this block (e.g. ".L_main_3"), set during instruction
  // selection and used both as a branch target and as the emitted block header.
  auto SetLabel(std::string label) -> void { label_ = std::move(label); }
  auto GetLabel() const -> const std::string & { return label_; }

 private:
  // Constructor with id
  explicit TargetBlock(size_t id) : id_(id) {}
  friend class TargetContext;
  std::vector<TargetInst *> insts_;
  std::vector<TargetBlock *> successors_;
  std::vector<TargetBlock *> predecessors_;

  // Unique identifier, and it will be unique in the same function
  size_t id_;

  // Function name
  std::string func_name_;

  // Assembly label for this block, e.g. ".L_main_3".
  std::string label_;
};

}  // namespace backend::targetir

#endif