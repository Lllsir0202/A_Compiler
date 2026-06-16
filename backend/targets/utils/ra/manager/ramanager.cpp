#include "backend/targets/utils/ra/manager/ramanager.h"
#include "context/backend_context.h"
#include "targets/utils/operand/targetoperand.h"

namespace backend::targetir {
auto RAManager::InitializeRegs(context::BackendContext *context) -> void {
  // Initialize free registers based on the reserved registers and the physical register pool
  auto &physical_reg_pool = context->GetPhysicalRegPool();
  for (const auto &reg : physical_reg_pool.GetPhysicalRegs(TargetRegClass::General)) {
    if (!IsReservedReg(reg)) {
      free_gpr_physical_regs_.insert(reg);
      allocable_gpr_physical_regs_.insert(reg);
    }
  }
  for (const auto &reg : physical_reg_pool.GetPhysicalRegs(TargetRegClass::Float)) {
    if (!IsReservedReg(reg)) {
      free_fpr_physical_regs_.insert(reg);
      allocable_fpr_physical_regs_.insert(reg);
    }
  }
  for (const auto &reg : physical_reg_pool.GetPhysicalRegs(TargetRegClass::Vector)) {
    if (!IsReservedReg(reg)) {
      free_vector_physical_regs_.insert(reg);
      allocable_vector_physical_regs_.insert(reg);
    }
  }
}

auto RAManager::GetFreeRegs(backend::targetir::TargetRegClass reg_class) -> std::unordered_set<PhysicalReg *> {
  // Get the set of free registers from the corresponding free register set based on the register class
  if (reg_class == TargetRegClass::General) {
    return free_gpr_physical_regs_;
  }
  if (reg_class == TargetRegClass::Float) {
    return free_fpr_physical_regs_;
  }
  if (reg_class == TargetRegClass::Vector) {
    return free_vector_physical_regs_;
  }
  throw std::runtime_error("Unknown register class");
}

auto RAManager::MarkRegUsed(TargetRegClass reg_class, PhysicalReg *reg) -> void {
  // Mark a register as used, i.e., move it from the free set to the used set based on the register class
  if (reg_class == TargetRegClass::General) {
    if (free_gpr_physical_regs_.count(reg) > 0) {
      free_gpr_physical_regs_.erase(reg);
      used_gpr_physical_regs_.insert(reg);
    }
  } else if (reg_class == TargetRegClass::Float) {
    if (free_fpr_physical_regs_.count(reg) > 0) {
      free_fpr_physical_regs_.erase(reg);
      used_fpr_physical_regs_.insert(reg);
    }
  } else if (reg_class == TargetRegClass::Vector) {
    if (free_vector_physical_regs_.count(reg) > 0) {
      free_vector_physical_regs_.erase(reg);
      used_vector_physical_regs_.insert(reg);
    }
  } else {
    throw std::runtime_error("Trying to use a register that is not in the free register pool");
  }
}

auto RAManager::MarkRegFree(TargetRegClass reg_class, PhysicalReg *reg) -> void {
  // Mark a register as free, i.e., move it from the used set to the free set based on the register class
  if (reg_class == TargetRegClass::General) {
    if (used_gpr_physical_regs_.count(reg) > 0) {
      used_gpr_physical_regs_.erase(reg);
      free_gpr_physical_regs_.insert(reg);
    }
  } else if (reg_class == TargetRegClass::Float) {
    if (used_fpr_physical_regs_.count(reg) > 0) {
      used_fpr_physical_regs_.erase(reg);
      free_fpr_physical_regs_.insert(reg);
    }
  } else if (reg_class == TargetRegClass::Vector) {
    if (used_vector_physical_regs_.count(reg) > 0) {
      used_vector_physical_regs_.erase(reg);
      free_vector_physical_regs_.insert(reg);
    }
  } else {
    throw std::runtime_error("Trying to free a register that is not in the used register pool");
  }
}

}  // namespace backend::targetir