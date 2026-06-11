#include "context/backend_context.h"
#include "backend/targets/utils/factory/opfactory.h"
#include "backend/targets/utils/operand/targetoperand.h"
#include "targets/utils/operand/targetprinter.h"
#include "targets/utils/operand/targetreg.h"

namespace context {
BackendContext::~BackendContext() {
  for (auto operand : operands_) {
    delete operand;
  }
  operands_.clear();
  delete module_;
  module_ = nullptr;
}

BackendContext::BackendContext(const std::string &target_arch) {
  if (target_arch == "riscv64") {
    // Set the target printer for physical register printing
    backend::targetir::TargetPhysicalReg::SetTargetPrinter(&backend::targetir::RV64RegPrinter::GetInstance());
  } else if (target_arch == "aarch64") {
    backend::targetir::TargetPhysicalReg::SetTargetPrinter(&backend::targetir::AArch64RegPrinter::GetInstance());
  } else {
    throw std::runtime_error("Unsupported target architecture: " + target_arch);
  }
}

// Create the operands
auto BackendContext::CreateVirtualRegister(backend::targetir::TargetRegSize reg_size)
    -> backend::targetir::TargetVirtualReg * {
  auto virtual_reg =
      backend::targetir::OperandFactory::CreateVirtualReg(reg_size, current_function_->GetVirtualRegisters().size());
  operands_.emplace_back(virtual_reg);
  current_function_->AddVirtualRegister(virtual_reg);
  return virtual_reg;
}

auto BackendContext::CreatePhysicalRegister(backend::targetir::TargetRegSize size,
                                            backend::targetir::TargetRegClass reg_class,
                                            uint8_t id) -> backend::targetir::TargetPhysicalReg * {
  auto physical_reg = backend::targetir::OperandFactory::CreatePhysicalReg(size, reg_class, id);
  operands_.emplace_back(physical_reg);
  return physical_reg;
}

auto BackendContext::CreateIntLiteral(int value) -> backend::targetir::TargetIntLiteral * {
  auto int_literal = backend::targetir::OperandFactory::CreateIntLiteral(value);
  operands_.emplace_back(int_literal);
  return int_literal;
}

auto BackendContext::CreateFloatLiteral(float value) -> backend::targetir::TargetFloatLiteral * {
  auto float_literal = backend::targetir::OperandFactory::CreateFloatLiteral(value);
  operands_.emplace_back(float_literal);
  return float_literal;
}

auto BackendContext::CreateFrameOperand(size_t slot_id) -> backend::targetir::TargetFrameOperand * {
  auto frame_operand = backend::targetir::OperandFactory::CreateFrameOperand(slot_id);
  operands_.emplace_back(frame_operand);
  return frame_operand;
}

}  // namespace context