#ifndef BACKEND_MIR_LOWER_PHIELIM_H
#define BACKEND_MIR_LOWER_PHIELIM_H

#include "backend/mir/utils/struct/function.h"
#include "context/mir_context.h"
namespace backend::lower {
class MIRLower;  // Forward declaration
class PhiElimination {
 public:
  auto EliminatePhi(utils::Function *function) -> void;

 private:
  explicit PhiElimination(context::MIRContext *mir_context) : mir_context_(mir_context) {}
  friend class MIRLower;

  // Recognize the critical edge and split it by inserting a new basic block,
  // and update the phi instructions accordingly
  auto SplitCriticalEdge(utils::Function *function) -> void;

  // Lower phi instructions by replacing them with copy instructions in the predecessor blocks
  auto LowerPhi(utils::Function *function) -> void;

  context::MIRContext *mir_context_;
};

}  // namespace backend::lower
#endif