#ifndef BACKEND_TARGETS_UTILS_STRUCT_TARGETFUNCTION_H
#define BACKEND_TARGETS_UTILS_STRUCT_TARGETFUNCTION_H
#include <fstream>
#include <string>
#include <utility>
#include <vector>
namespace backend::targetir {
class TargetBlock;
class TargetOperand;
class TargetInst;
class TargetContext;

class TargetFunction {
 public:
  // Destructor: delete all blocks
  ~TargetFunction();
  auto Emit(std::ofstream *out) -> void;

  auto GetName() const -> const std::string & { return name_; }
  auto GetBlocks() const -> const std::vector<TargetBlock *> & { return blocks_; }
  auto AddBlock(TargetBlock *block) -> void { blocks_.emplace_back(block); }
  // Replace the whole block list, used by machine passes that delete blocks (e.g.
  // if-conversion collapsing a diamond). Removed blocks are not freed here.
  auto SetBlocks(std::vector<TargetBlock *> blocks) -> void { blocks_ = std::move(blocks); }

 private:
  // Constructor with function name
  explicit TargetFunction(std::string name) : name_(std::move(name)) {}
  // Record a list of blocks
  std::vector<TargetBlock *> blocks_;
  // Function name
  std::string name_;
  friend class TargetContext;
};

}  // namespace backend::targetir

#endif