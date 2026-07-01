#include "context/backend_context.h"
#include "backend/targets/utils/factory/opfactory.h"
#include "backend/targets/utils/operand/targetoperand.h"
#include "targets/utils/manager/targetinitializer.h"
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

BackendContext::BackendContext(const std::string &target_arch, int optimization_level)
    : module_(nullptr), current_function_(nullptr), current_block_(nullptr) {
  auto *target_initializer =
      backend::targetir::TargetInitializerRegistry::GetInstance().GetTargetInitializer(target_arch);
  if (target_initializer != nullptr) {
    try {
      target_initializer->Initialize(this, optimization_level);
    } catch (const std::exception &e) {
      throw std::runtime_error("Error initializing target for architecture " + target_arch + ": " + e.what());
    }
  } else {
    throw std::runtime_error("No target initializer found for target architecture: " + target_arch);
  }
}

// Create the operands
auto BackendContext::CreateVirtualRegister(backend::targetir::TargetRegSize reg_size,
                                           backend::targetir::TargetRegClass reg_class)
    -> backend::targetir::TargetVirtualReg * {
  auto virtual_reg = backend::targetir::OperandFactory::CreateVirtualReg(
      reg_size, reg_class, current_function_->GetVirtualRegisters().size());
  operands_.emplace_back(virtual_reg);
  current_function_->AddVirtualRegister(virtual_reg);
  return virtual_reg;
}

auto BackendContext::CreatePhysicalRegView(backend::targetir::TargetRegSize reg_size,
                                           backend::targetir::TargetRegClass reg_class,
                                           backend::targetir::PhysicalReg *physical_reg,
                                           backend::targetir::TargetRegRole role,
                                           uint8_t offset) -> backend::targetir::TargetPhysicalReg * {
  auto physical_reg_view =
      backend::targetir::OperandFactory::CreatePhysicalReg(reg_size, reg_class, physical_reg, role, offset);
  operands_.emplace_back(physical_reg_view);
  return physical_reg_view;
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

auto BackendContext::CreateLabelOperand(const std::string &name) -> backend::targetir::TargetLabel * {
  auto label = backend::targetir::OperandFactory::CreateLabelOperand(name);
  operands_.emplace_back(label);
  return label;
}

auto BackendContext::CreateFrameOperand(size_t slot_id) -> backend::targetir::TargetFrameOperand * {
  auto frame_operand = backend::targetir::OperandFactory::CreateFrameOperand(slot_id);
  operands_.emplace_back(frame_operand);
  return frame_operand;
}

}  // namespace context
