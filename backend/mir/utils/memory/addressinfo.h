#ifndef BACKEND_MIR_UTILS_MEMORY_ADDRESSINFO_H
#define BACKEND_MIR_UTILS_MEMORY_ADDRESSINFO_H
#include <vector>
#include "backend/mir/utils/operand/operand.h"
#include "mir/utils/operand/frameoperand.h"
#include "mir/utils/operand/globaloperand.h"
#include "mir/utils/operand/literaloperand.h"
#include "mir/utils/operand/regoperand.h"
namespace backend::utils {
class AddressInfo {
 public:
  /**
   * @brief For memory operand, we need to record the base pointer,
   * no matter it is a register operand or a frame operand,
   * and the offset, or index
   * since the offset might not be able to be decided since the index might be
   * a register operand, and the offset might be decided after register allocation,
   * @note Check if the member is nullptr
   */
  Operand *base_ptr_;

  // offset(might be nullptr if the offset is not decided yet)
  Operand *offset_;

  // dimensions
  std::vector<int> dimensions_;

  // a list of index
  std::vector<Operand *> indices_;

  // record the access type,

  // For debug, output the address info
  auto ToString() const -> std::string {
    std::string result = "base_ptr: ";
    if (base_ptr_ != nullptr) {
      if (base_ptr_->IsRegister()) {
        result += "Register(" + std::to_string(static_cast<RegOperand *>(base_ptr_)->GetRegNum()) + ")";
      } else if (base_ptr_->IsFrame()) {
        result += "Frame(" + std::to_string(static_cast<FrameOperand *>(base_ptr_)->GetSlotId()) + ")";
      } else if (base_ptr_->IsGlobal()) {
        result += "Global(" + static_cast<GlobalOperand *>(base_ptr_)->GetName() + ")";
      } else {
        result += "Unknown";
      }
    } else {
      result += "nullptr";
    }
    result += ", offset: ";
    if (offset_ != nullptr) {
      if (offset_->IsRegister()) {
        result += "Register(" + std::to_string(static_cast<RegOperand *>(offset_)->GetRegNum()) + ")";
      } else if (offset_->IsFrame()) {
        result += "Frame(" + std::to_string(static_cast<FrameOperand *>(offset_)->GetSlotId()) + ")";
      } else if (offset_->IsLiteral()) {
        result += "Literal(" + std::to_string(static_cast<IntLiteral *>(offset_)->GetValue()) + ")";
      } else {
        result += "Unknown";
      }
    } else {
      result += "nullptr";
    }
    result += ", dimensions: [";
    for (size_t i = 0; i < dimensions_.size(); ++i) {
      result += std::to_string(dimensions_[i]);
      if (i != dimensions_.size() - 1) {
        result += ", ";
      }
    }
    result += "], indices: [";
    for (size_t i = 0; i < indices_.size(); ++i) {
      if (indices_[i] != nullptr) {
        if (indices_[i]->IsRegister()) {
          result += "Register(" + std::to_string(static_cast<RegOperand *>(indices_[i])->GetRegNum()) + ")";
        } else if (indices_[i]->IsFrame()) {
          result += "Frame(" + std::to_string(static_cast<FrameOperand *>(indices_[i])->GetSlotId()) + ")";
        } else if (indices_[i]->IsLiteral()) {
          result += "Literal(" + std::to_string(static_cast<IntLiteral *>(indices_[i])->GetValue()) + ")";
        } else {
          result += "Unknown";
        }
      } else {
        result += "nullptr";
      }
      if (i != indices_.size() - 1) {
        result += ", ";
      }
    }
    result += "]";
    return result;
  }
};

}  // namespace backend::utils

#endif