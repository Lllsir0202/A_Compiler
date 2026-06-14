#include "backend/targets/utils/ra/manager/ramanager.h"
#include "context/backend_context.h"

namespace backend::targetir {
auto RAManager::InitializeFreeRegs(context::BackendContext *context) -> void {
  // Initialize free registers based on the reserved registers and the physical register pool
  auto &physical_reg_pool = context->GetPhysicalRegPool();
  for (const auto &reg : physical_reg_pool.GetPhysicalRegs(TargetRegClass::General)) {
    if (!IsReservedReg(reg)) {
      free_gpr_physical_regs_.insert(reg);
    }
  }
  for (const auto &reg : physical_reg_pool.GetPhysicalRegs(TargetRegClass::Float)) {
    if (!IsReservedReg(reg)) {
      free_fpr_physical_regs_.insert(reg);
    }
  }
  for (const auto &reg : physical_reg_pool.GetPhysicalRegs(TargetRegClass::Vector)) {
    if (!IsReservedReg(reg)) {
      free_vector_physical_regs_.insert(reg);
    }
  }
}

auto RAManager::GetFreeReg(backend::targetir::TargetRegClass reg_class) -> PhysicalReg * {
  // Get a free register from the corresponding free register set based on the register class
  if (reg_class == TargetRegClass::General) {
    if (free_gpr_physical_regs_.empty()) {
      throw std::runtime_error("No free general-purpose registers available");
    }
    auto reg = *free_gpr_physical_regs_.begin();
    MarkRegUsed(reg);
    return reg;
  }
  if (reg_class == TargetRegClass::Float) {
    if (free_fpr_physical_regs_.empty()) {
      throw std::runtime_error("No free floating-point registers available");
    }
    auto reg = *free_fpr_physical_regs_.begin();
    MarkRegUsed(reg);
    return reg;
  }
  if (reg_class == TargetRegClass::Vector) {
    if (free_vector_physical_regs_.empty()) {
      throw std::runtime_error("No free vector registers available");
    }
    auto reg = *free_vector_physical_regs_.begin();
    MarkRegUsed(reg);
    return reg;
  }
  throw std::runtime_error("Unknown register class");
  return nullptr;  // Unreachable, but silence compiler warning
}

auto RAManager::MarkRegUsed(PhysicalReg *reg) -> void {
  // Mark a register as used, i.e., move it from the free set to the used set based on the register class
  if (free_gpr_physical_regs_.count(reg) > 0) {
    free_gpr_physical_regs_.erase(reg);
    used_gpr_physical_regs_.insert(reg);
  } else if (free_fpr_physical_regs_.count(reg) > 0) {
    free_fpr_physical_regs_.erase(reg);
    used_fpr_physical_regs_.insert(reg);
  } else if (free_vector_physical_regs_.count(reg) > 0) {
    free_vector_physical_regs_.erase(reg);
    used_vector_physical_regs_.insert(reg);
  } else {
    throw std::runtime_error("Trying to use a register that is not in the free register pool");
  }
}

auto RAManager::MarkRegFree(PhysicalReg *reg) -> void {
  // Mark a register as free, i.e., move it from the used set to the free set based on the register class
  if (used_gpr_physical_regs_.count(reg) > 0) {
    used_gpr_physical_regs_.erase(reg);
    free_gpr_physical_regs_.insert(reg);
  } else if (used_fpr_physical_regs_.count(reg) > 0) {
    used_fpr_physical_regs_.erase(reg);
    free_fpr_physical_regs_.insert(reg);
  } else if (used_vector_physical_regs_.count(reg) > 0) {
    used_vector_physical_regs_.erase(reg);
    free_vector_physical_regs_.insert(reg);
  } else {
    throw std::runtime_error("Trying to free a register that is not in the used register pool");
  }
}

}  // namespace backend::targetir