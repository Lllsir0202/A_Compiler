#ifndef BACKEND_TARGETS_UTILS_STRUCT_TARGETFUNCTION_H
#define BACKEND_TARGETS_UTILS_STRUCT_TARGETFUNCTION_H
#include <string>
#include <vector>
namespace backend::targetir {
class TargetBlock;
class TargetOperand;
class TargetInst;

class TargetFunction {
 public:
  // Constructor with function name
  explicit TargetFunction(std::string name) : name_(std::move(name)) {}
  // Destructor: delete all blocks
  ~TargetFunction();
  auto Emit(std::ofstream *out) -> void;

 private:
  // Record a list of blocks
  std::vector<TargetBlock *> blocks_;
  // Function name
  std::string name_;
};

}  // namespace backend::targetir

#endif