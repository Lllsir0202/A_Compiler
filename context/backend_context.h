#ifndef CONTEXT_BACKEND_CONTEXT_H
#define CONTEXT_BACKEND_CONTEXT_H

#include <cstdint>
#include <unordered_map>
#include <vector>
#include "backend/mir/utils/operand/operand.h"
#include "backend/targets/utils/operand/targetframe.h"
#include "backend/targets/utils/operand/targetliteral.h"
#include "backend/targets/utils/operand/targetoperand.h"
#include "backend/targets/utils/operand/targetreg.h"
#include "backend/targets/utils/struct/targetmodule.h"
#include "mir/utils/factory/instfactory.h"
namespace context {
// Provide the context for the backend,
// Include the target architecture.
class BackendContext {
 public:
  ~BackendContext();
  // Create the operands
  auto CreateVirtualRegister() -> backend::targetir::TargetVirtualReg *;
  // Create the physical register based on the given information
  auto CreatePhysicalRegister(backend::targetir::TargetRegSize size, backend::targetir::TargetRegClass reg_class,
                              uint8_t id) -> backend::targetir::TargetPhysicalReg *;
  auto CreateIntLiteral(int value) -> backend::targetir::TargetIntLiteral *;
  auto CreateFloatLiteral(float value) -> backend::targetir::TargetFloatLiteral *;

  auto CreateFrameOperand(size_t slot_id) -> backend::targetir::TargetFrameOperand *;

  // Set module
  auto SetModule(backend::targetir::TargetModule *module) -> void {
    if (module_ == module) {
      return;
    }
    if (module_ != nullptr) {
      module_->Reset();
    }
    module_ = module;
  }
  auto GetModule() -> backend::targetir::TargetModule * { return module_; }

  // Add a function to the module and Get functions
  auto AddFunction(backend::targetir::TargetFunction *function) -> void { module_->AddFunction(function); }
  auto GetFunctions() -> const std::vector<backend::targetir::TargetFunction *> & { return module_->GetFunctions(); }

  // Map from mir operand to target operand
  auto MapOperand(backend::utils::Operand *mir_operand, backend::targetir::TargetOperand *target_operand) -> void {
    operand_map_[mir_operand] = target_operand;
  }
  auto GetMappedOperand(backend::utils::Operand *mir_operand) -> backend::targetir::TargetOperand * {
    auto it = operand_map_.find(mir_operand);
    if (it != operand_map_.end()) {
      return it->second;
    }
    return nullptr;
  }

  // Create instructions
  template <typename T, typename... Args>
  auto CreateInstruction(Args &&...args) -> T * {
    return backend::utils::factory::InstFactory::CreateInst<T>(std::forward<Args>(args)...);
  }

 private:
  /**
   * @brief Just provide the target-free backend context here,
   * the arichtecture-specific context will be provided in the corresponding lower pass.
   */
  std::vector<backend::targetir::TargetOperand *> operands_;

  // Map from mir register operand to target operand
  std::unordered_map<backend::utils::Operand *, backend::targetir::TargetOperand *> operand_map_;

  backend::targetir::TargetModule *module_;
};

}  // namespace context

#endif