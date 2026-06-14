#ifndef BACKEND_TARGETS_UTILS_RA_MANAGER_RAMANAGER_H
#define BACKEND_TARGETS_UTILS_RA_MANAGER_RAMANAGER_H

#include <unordered_set>
#include "backend/targets/utils/manager/registerpool.h"
namespace backend::targetir {
class RAManager {
 public:
  ~RAManager() = default;

  auto SetReservedRegs(const std::unordered_set<PhysicalReg *> &reserved_regs) -> void {
    reserved_physical_regs_ = reserved_regs;
  }

  auto SetCallerSavedRegs(const std::unordered_set<PhysicalReg *> &caller_saved_regs) -> void {
    caller_saved_physical_regs_ = caller_saved_regs;
  }

  auto SetCalleeSavedRegs(const std::unordered_set<PhysicalReg *> &callee_saved_regs) -> void {
    callee_saved_physical_regs_ = callee_saved_regs;
  }

  auto GetReservedRegs() -> const std::unordered_set<PhysicalReg *> & { return reserved_physical_regs_; }
  auto GetCallerSavedRegs() -> const std::unordered_set<PhysicalReg *> & { return caller_saved_physical_regs_; }
  auto GetCalleeSavedRegs() -> const std::unordered_set<PhysicalReg *> & { return callee_saved_physical_regs_; }

  auto IsReservedReg(PhysicalReg *reg) -> bool { return reserved_physical_regs_.count(reg) > 0; }
  auto IsCallerSavedReg(PhysicalReg *reg) -> bool { return caller_saved_physical_regs_.count(reg) > 0; }
  auto IsCalleeSavedReg(PhysicalReg *reg) -> bool { return callee_saved_physical_regs_.count(reg) > 0; }

  // Set up free registers based on the reserved registers and the physical register pool
  auto InitializeFreeRegs(context::BackendContext *context) -> void;

  // TODO(loki): Add cost model to estimate the cost of using a register

 private:
  // Get the set of physical registers, and maintain the used and free sets for each register class

  std::unordered_set<PhysicalReg *> used_gpr_physical_regs_;
  std::unordered_set<PhysicalReg *> free_gpr_physical_regs_;
  std::unordered_set<PhysicalReg *> used_fpr_physical_regs_;
  std::unordered_set<PhysicalReg *> free_fpr_physical_regs_;

  // Reserved registers for special purposes, e.g., stack pointer, return address, etc.
  std::unordered_set<PhysicalReg *> reserved_physical_regs_;
  std::unordered_set<PhysicalReg *> caller_saved_physical_regs_;
  std::unordered_set<PhysicalReg *> callee_saved_physical_regs_;
};
}  // namespace backend::targetir
#endif