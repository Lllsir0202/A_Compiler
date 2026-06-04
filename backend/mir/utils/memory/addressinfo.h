#ifndef BACKEND_MIR_UTILS_MEMORY_ADDRESSINFO_H
#define BACKEND_MIR_UTILS_MEMORY_ADDRESSINFO_H
#include <vector>
#include "backend/mir/utils/operand/operand.h"
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
};

}  // namespace backend::utils

#endif