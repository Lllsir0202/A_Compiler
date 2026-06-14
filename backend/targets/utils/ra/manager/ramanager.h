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
  auto GetUsedRegs() -> const std::unordered_set<PhysicalReg *> & { return used_physical_regs_; }
  auto GetFreeRegs() -> const std::unordered_set<PhysicalReg *> & { return free_physical_regs_; }
  auto UseReg(PhysicalReg *reg) -> void {
    if (free_physical_regs_.count(reg) > 0) {
      free_physical_regs_.erase(reg);
      used_physical_regs_.insert(reg);
    } else {
      throw std::runtime_error("Trying to use a register that is not in the free register pool");
    }
  }

  // TODO(loki): Add cost model to estimate the cost of using a register

 private:
  std::unordered_set<PhysicalReg *> used_physical_regs_;
  std::unordered_set<PhysicalReg *> free_physical_regs_;

  // Reserved registers for special purposes, e.g., stack pointer, return address, etc.
  std::unordered_set<PhysicalReg *> reserved_physical_regs_;
  std::unordered_set<PhysicalReg *> caller_saved_physical_regs_;
  std::unordered_set<PhysicalReg *> callee_saved_physical_regs_;
};
}  // namespace backend::targetir
#endif