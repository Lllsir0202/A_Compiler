#ifndef BACKEND_TARGETS_UTILS_INST_TARGETINST_H
#define BACKEND_TARGETS_UTILS_INST_TARGETINST_H
#include <fstream>
namespace backend::targetir {
class TargetInst {
 public:
  virtual auto Emit(std::ofstream *out) -> void = 0;

 protected:
};

}  // namespace backend::targetir

#endif