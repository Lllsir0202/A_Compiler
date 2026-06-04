#ifndef BACKEND_MIR_UTILS_OPERAND_FRAMEOPERAND_H
#define BACKEND_MIR_UTILS_OPERAND_FRAMEOPERAND_H
#include "backend/mir/utils/operand/operand.h"

/**
 * @brief This file defines the FrameOperand class,
 * since in backend, the alloca instruction in llvmir will be lowered to
 * create a stack slot, and the operand of the stack slot is represented by FrameOperand.
 */
namespace backend::utils {
class FrameOperand : public Operand {
 public:
  auto Emit(std::ofstream *out) const -> void override {
    // The frame operand will be emitted as the stack slot id
    *out << "frame_slot_" << slot_id_;
  }

  auto GetSlotId() const -> size_t { return slot_id_; }

 private:
  explicit FrameOperand(size_t slot_id) : Operand(OpType::Frame), slot_id_(slot_id) {}
  size_t slot_id_;
  friend class factory::OperandFactory;
};

}  // namespace backend::utils

#endif