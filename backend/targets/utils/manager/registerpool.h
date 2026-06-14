#ifndef BACKEND_TARGETS_UTILS_OPERAND_TARGETPHYREGPOOL_H
#define BACKEND_TARGETS_UTILS_OPERAND_TARGETPHYREGPOOL_H

#include <cstdint>
#include <vector>
#include "targets/utils/operand/targetoperand.h"
#include "targets/utils/operand/targetreg.h"

namespace context {
class BackendContext;
}

namespace backend::targetir {
/**
 * @brief Define the physical register
 */
class PhysicalReg {
 public:
  auto GetID() -> uint8_t { return id_; }

 private:
  explicit PhysicalReg(uint8_t id) : id_(id) {}
  friend class PhysicalRegPool;
  // Just physical register ID
  uint8_t id_;
};

/**
 * @brief Physical register pool, used to manage the physical registers.
 * Provide interface to get access to the physical registers.
 * Register specific arichitectures' registers.
 */
class PhysicalRegPool {
 public:
  ~PhysicalRegPool();

  // Provide interface to get access to the physical registers.
  auto GetPhysicalReg(TargetRegClass reg_class, uint8_t id) -> PhysicalReg *;

  // Initialize the physical register pool, called in the constructor of backend context.
  auto InitPhysicalRegPool(TargetRegClass reg_class, size_t reg_num) -> void;

  // Get all physical registers of a certain class, used for register allocation.
  auto GetPhysicalRegs(TargetRegClass reg_class) -> const std::vector<PhysicalReg *> &;

 private:
  // Only allow backend context to create
  PhysicalRegPool() = default;

  // Record all the physical registers in the pool and sperate them by class.
  // Different size of registers should be regarded as the same register
  /**
   * @brief Record different classes of registers.
   */
  // GPR
  std::vector<PhysicalReg *> gpr_regs_;

  // FPR
  std::vector<PhysicalReg *> fpr_regs_;

  // Vector
  std::vector<PhysicalReg *> vector_regs_;

  friend class context::BackendContext;

  // Implement different target architectures' physical register initialization
};
}  // namespace backend::targetir
#endif