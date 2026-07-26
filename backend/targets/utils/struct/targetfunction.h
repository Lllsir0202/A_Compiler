#ifndef BACKEND_TARGETS_UTILS_STRUCT_TARGETFUNCTION_H
#define BACKEND_TARGETS_UTILS_STRUCT_TARGETFUNCTION_H
#include <fstream>
#include <map>
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

  // Pure instructions needed to materialize a machine-phi incoming value on a
  // specific CFG edge. Phi elimination places this code in the split block for
  // a critical edge, or immediately before the predecessor terminator when the
  // predecessor has only one successor.
  auto AddEdgeCode(const std::string &predecessor, const std::string &successor,
                   const std::vector<TargetInst *> &code) -> void {
    auto &edge = edge_code_[{predecessor, successor}];
    edge.insert(edge.end(), code.begin(), code.end());
  }
  auto GetEdgeCode(const std::string &predecessor,
                   const std::string &successor) const -> const std::vector<TargetInst *> & {
    static const std::vector<TargetInst *> EMPTY;
    auto iter = edge_code_.find({predecessor, successor});
    return iter == edge_code_.end() ? EMPTY : iter->second;
  }
  auto GetAllEdgeCode() const -> const std::map<std::pair<std::string, std::string>, std::vector<TargetInst *>> & {
    return edge_code_;
  }
  auto ClearEdgeCode() -> void { edge_code_.clear(); }

 private:
  // Constructor with function name
  explicit TargetFunction(std::string name) : name_(std::move(name)) {}
  // Record a list of blocks
  std::vector<TargetBlock *> blocks_;
  // Function name
  std::string name_;
  std::map<std::pair<std::string, std::string>, std::vector<TargetInst *>> edge_code_;
  friend class TargetContext;
};

}  // namespace backend::targetir

#endif
