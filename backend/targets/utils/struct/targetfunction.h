#ifndef BACKEND_TARGETS_UTILS_STRUCT_TARGETFUNCTION_H
#define BACKEND_TARGETS_UTILS_STRUCT_TARGETFUNCTION_H
#include <string>
#include <vector>
namespace backend::targetir {
class TargetBlock;
class TargetOperand;
class TargetInst;
class TargetFrameObject;

class TargetFunction {
 public:
  // Destructor: delete all blocks
  ~TargetFunction();
  auto Emit(std::ofstream *out) -> void;
  auto AddBlock(TargetBlock *block) -> void { blocks_.emplace_back(block); }
  auto GetBlocks() const -> const std::vector<TargetBlock *> & { return blocks_; }
  auto GetName() const -> const std::string & { return name_; }
  [[maybe_unused]] auto SetBlocks(const std::vector<TargetBlock *> &blocks) -> void { blocks_ = blocks; }
  [[maybe_unused]] auto ReplaceBlock(TargetBlock *old_block, TargetBlock *new_block) -> void {
    for (auto &block : blocks_) {
      if (block == old_block) {
        block = new_block;
      }
    }
  }

  // Interface to get, add frame objects
  auto GetFrameObjects() const -> const std::vector<TargetFrameObject *> & { return frame_objects_; }
  auto AddFrameObject(TargetFrameObject *frame_object) -> void { frame_objects_.emplace_back(frame_object); }

  auto GetVirtualRegisters() -> std::vector<TargetOperand *> & { return virtual_registers_; }
  auto AddVirtualRegister(TargetOperand *virtual_register) -> void {
    virtual_registers_.emplace_back(virtual_register);
  }

 private:
  // Constructor with function name
  explicit TargetFunction(std::string name) : name_(std::move(name)) {}
  friend class FunctionFactory;
  // Record a list of blocks
  std::vector<TargetBlock *> blocks_;

  // A list of frame objects
  std::vector<TargetFrameObject *> frame_objects_;
  // Function name
  std::string name_;

  // Record all virtual registers in this function, which will be used for register allocation.
  // Note that the index of the virtual register in this vector is its id.
  std::vector<TargetOperand *> virtual_registers_;
};

}  // namespace backend::targetir

#endif