#ifndef BACKEND_TARGETS_UTILS_OPERAND_TARGETFRAME_H
#define BACKEND_TARGETS_UTILS_OPERAND_TARGETFRAME_H

#include "backend/targets/utils/operand/targetoperand.h"
namespace backend::targetir {
class TargetFrameOperand : public TargetOperand {
 public:
  auto Emit(std::ofstream *out) -> void override {
    // Output the frame operand in the format "f{slot_id}",
    // but should not exist in the final assembly.
    *out << "f" << slot_id_;
  }

  auto GetSlotID() -> size_t { return slot_id_; }

 private:
  explicit TargetFrameOperand(size_t slot_id) : TargetOperand(TargetOperandType::Frame), slot_id_(slot_id) {}
  friend class OperandFactory;
  // Frame slot id, used to identify the slot in the stack frame
  size_t slot_id_;
};

}  // namespace backend::targetir

#endif