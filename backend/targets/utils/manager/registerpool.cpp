#include "backend/targets/utils/manager/registerpool.h"
#include "targets/utils/operand/targetoperand.h"

namespace backend::targetir {
PhysicalRegPool::~PhysicalRegPool() {
  for (auto reg : gpr_regs_) {
    delete reg;
  }
  gpr_regs_.clear();
  for (auto reg : fpr_regs_) {
    delete reg;
  }
  fpr_regs_.clear();
  for (auto reg : vector_regs_) {
    delete reg;
  }
  vector_regs_.clear();
}

auto PhysicalRegPool::GetPhysicalReg(TargetRegClass reg_class, uint8_t id) -> PhysicalReg * {
  // Return the physical register with the given class and id
  switch (reg_class) {
    case TargetRegClass::General: {
      if (id >= gpr_regs_.size()) {
        throw std::runtime_error("Invalid GPR register ID");
      }
      return gpr_regs_[id];
    }
    case TargetRegClass::Float: {
      if (id >= fpr_regs_.size()) {
        throw std::runtime_error("Invalid FPR register ID");
      }
      return fpr_regs_[id];
    }
    case TargetRegClass::Vector: {
      if (id >= vector_regs_.size()) {
        throw std::runtime_error("Invalid vector register ID");
      }
      return vector_regs_[id];
    }
    default:
      throw std::runtime_error("Invalid register class");
  }
}

// Initialize the physical register pool, called in the constructor of backend context.
auto PhysicalRegPool::InitPhysicalRegPool(TargetRegClass reg_class, size_t reg_num) -> void {
  switch (reg_class) {
    case TargetRegClass::General: {
      gpr_regs_.reserve(reg_num);
      for (size_t i = 0; i < reg_num; ++i) {
        gpr_regs_.emplace_back(new PhysicalReg(i));
      }
    } break;
    case TargetRegClass::Float: {
      fpr_regs_.reserve(reg_num);
      for (size_t i = 0; i < reg_num; ++i) {
        fpr_regs_.emplace_back(new PhysicalReg(i));
      }
    } break;
    case TargetRegClass::Vector: {
      vector_regs_.reserve(reg_num);
      for (size_t i = 0; i < reg_num; ++i) {
        vector_regs_.emplace_back(new PhysicalReg(i));
      }
    } break;
    default:
      throw std::runtime_error("Invalid register class");
  }
}

// Get specific physical registers
auto PhysicalRegPool::GetPhysicalRegs(TargetRegClass reg_class) -> const std::vector<PhysicalReg *> & {
  switch (reg_class) {
    case TargetRegClass::General:
      return gpr_regs_;
    case TargetRegClass::Float:
      return fpr_regs_;
    case TargetRegClass::Vector:
      return vector_regs_;
    default:
      throw std::runtime_error("Invalid register class");
  }
}

}  // namespace backend::targetir