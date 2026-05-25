#ifndef BACKEND_MIR_UTILS_OPERAND_MEMORYOPERAND_H
#define BACKEND_MIR_UTILS_OPERAND_MEMORYOPERAND_H

#include "backend/mir/utils/memory/addressinfo.h"
#include "backend/mir/utils/memory/memoryinfo.h"
#include "backend/mir/utils/operand/operand.h"
namespace backend::utils {
enum class MemSize : uint8_t { UnSet, Bit32, Bit64 };
/**
 * @brief This class represents the memory operand in the virtual mir,
 * which will be used as the result of the memory-related instructions,
 * and we will set the operand's actual memory size while lowering.
 * @note The memory operand will only be used in the memory-related instructions.
 * it will only be used as :
 * 1. the result of gep
 * 2. the operand of load and store
 * 3. the operand of call (for the arguments that need to be passed by memory)
 */
class MemoryOperand : public Operand {
 public:
  /**
   * @brief Since the memory address will vary among different architectures,
   * we can set the memory size of the memory operand and deal with them separately in the lowering phase.
   */
  auto GetMemSize() const -> MemSize { return mem_size_; }
  auto SetMemSize(MemSize mem_size) -> void { mem_size_ = mem_size; }

  // Set the addressinfo after creating
  auto GetAddressInfo() const -> const AddressInfo & { return address_info_; }
  auto SetAddressInfo(const AddressInfo &address_info) -> void { address_info_ = address_info; }

  // Set the memory info after creating
  auto GetMemoryInfo() const -> const MemoryInfo & { return memory_info_; }
  auto SetMemoryInfo(const MemoryInfo &memory_info) -> void { memory_info_ = memory_info; }

 private:
  explicit MemoryOperand() : Operand(OpType::Memory) {}
  // Memory operand is used to represent the memory address
  AddressInfo address_info_;
  MemoryInfo memory_info_;

  friend class factory::OperandFactory;
  MemSize mem_size_ = MemSize::UnSet;
};

}  // namespace backend::utils

#endif