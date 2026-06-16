#ifndef BACKEND_TARGETS_RV64_LOWER_LOWERHELPER_H
#define BACKEND_TARGETS_RV64_LOWER_LOWERHELPER_H

#include "backend/mir/inst/inst.h"
namespace backend::targetir::rv64 {
class LowerHelper {
 public:
  auto LowerDisptch(backend::inst::Instruction *inst) -> void;

 private:
  // Lower arithmetic instructions
};

}  // namespace backend::targetir::rv64

#endif