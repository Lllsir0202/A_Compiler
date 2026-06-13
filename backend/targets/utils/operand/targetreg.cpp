#include "backend/targets/utils/operand/targetreg.h"
#include "backend/targets/utils/manager/registerpool.h"
namespace backend::targetir {
const TargetRegPrinter *TargetPhysicalReg::target_printer = nullptr;

auto TargetPhysicalReg::Emit(std::ofstream *out) -> void {
  if (target_printer == nullptr) {
    // Should report error
    throw std::runtime_error("Target printer is not set for physical register");
  }
  // Output the physical register name using target printer
  *out << target_printer->GetRegName(reg_size_, physical_reg_->GetID(), reg_class_);
}

auto TargetPhysicalReg::GetID() -> uint8_t { return physical_reg_->GetID(); }

}  // namespace backend::targetir