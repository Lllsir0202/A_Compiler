#ifndef CONTEXT_BACKEND_CONTEXT_H
#define CONTEXT_BACKEND_CONTEXT_H

#include <unordered_map>
#include <vector>
#include "backend/mir/utils/operand/operand.h"
#include "backend/targets/utils/factory/blockfactory.h"
#include "backend/targets/utils/factory/functionfactory.h"
#include "backend/targets/utils/factory/instfactory.h"
#include "backend/targets/utils/factory/modulefactory.h"
#include "backend/targets/utils/factory/opfactory.h"
#include "backend/targets/utils/manager/registerpool.h"
#include "backend/targets/utils/manager/targetinitializer.h"
#include "backend/targets/utils/operand/targetframe.h"
#include "backend/targets/utils/operand/targetlabel.h"
#include "backend/targets/utils/operand/targetliteral.h"
#include "backend/targets/utils/operand/targetoperand.h"
#include "backend/targets/utils/operand/targetreg.h"
#include "backend/targets/utils/ra/liverange/liverange.h"
#include "backend/targets/utils/ra/manager/ramanager.h"
#include "backend/targets/utils/ra/method/hint/allocatorhint.h"
#include "backend/targets/utils/struct/targetmodule.h"

namespace context {
// Provide the context for the backend,
// Include the target architecture.
class BackendContext {
 public:
  explicit BackendContext(const std::string &target_arch, int optimization_level);

  ~BackendContext();
  // Create the operands
  auto CreateVirtualRegister(backend::targetir::TargetRegSize reg_size,
                             backend::targetir::TargetRegClass reg_class) -> backend::targetir::TargetVirtualReg *;

  auto CreatePhysicalRegView(backend::targetir::TargetRegSize reg_size, backend::targetir::TargetRegClass reg_class,
                             backend::targetir::PhysicalReg *physical_reg,
                             backend::targetir::TargetRegRole role = backend::targetir::TargetRegRole::NONE,
                             uint8_t offset = 0) -> backend::targetir::TargetPhysicalReg *;
  auto CreateIntLiteral(int value) -> backend::targetir::TargetIntLiteral *;
  auto CreateFloatLiteral(float value) -> backend::targetir::TargetFloatLiteral *;
  auto CreateLabelOperand(const std::string &name) -> backend::targetir::TargetLabel *;

  auto CreateFrameOperand(size_t slot_id) -> backend::targetir::TargetFrameOperand *;

  // Create a symbol operand, using template to support different architectures.
  template <typename SymbolType, typename... Args>
  auto CreateSymbolOperand(Args &&...args) -> SymbolType * {
    auto *symbol = backend::targetir::OperandFactory::CreateSymbolOperand<SymbolType>(std::forward<Args>(args)...);
    operands_.emplace_back(symbol);
    return symbol;
  }

  auto CreateModule() -> backend::targetir::TargetModule * {
    return backend::targetir::ModuleFactory::CreateTargetModule();
  }

  auto CreateFunction(const std::string &name) -> backend::targetir::TargetFunction * {
    auto function = backend::targetir::FunctionFactory::CreateTargetFunction(name);
    module_->AddFunction(function);
    return function;
  }

  auto CreateBlock(size_t id) -> backend::targetir::TargetBlock * {
    return backend::targetir::BlockFactory::CreateTargetBlock(id);
  }

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
  // And reset the instruction number when entering a new function
  auto AddFunction(backend::targetir::TargetFunction *function) -> void {
    module_->AddFunction(function);
    ResetInstNum();
    current_function_ = function;
  }
  auto GetFunctions() -> const std::vector<backend::targetir::TargetFunction *> & { return module_->GetFunctions(); }

  auto AddFrameObject(size_t size, size_t alignment) -> backend::targetir::TargetFrameObject * {
    auto frame_object = backend::targetir::FunctionFactory::CreateTargetFrameObject(
        current_function_->GetFrameObjectsSize(), size, alignment);
    current_function_->AddFrameObject(frame_object);
    return frame_object;
  }

  auto GetFrameObjectByID(size_t id) -> backend::targetir::TargetFrameObject * {
    return current_function_->GetFrameObjectByID(id);
  }

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

  auto MapBlock(size_t mir_block_id, backend::targetir::TargetBlock *target_block) -> void {
    block_map_[mir_block_id] = target_block;
  }

  auto ClearBlockMap() -> void { block_map_.clear(); }

  auto GetMappedBlock(size_t mir_block_id) -> backend::targetir::TargetBlock * {
    auto it = block_map_.find(mir_block_id);
    if (it != block_map_.end()) {
      return it->second;
    }
    return nullptr;
  }

  // Create instructions
  template <typename T, typename... Args>
  auto CreateInstruction(Args &&...args) -> T * {
    auto *inst = backend::targetir::InstFactory::CreateInst<T>(std::forward<Args>(args)...);
    inst->SetInstNum(++inst_num_);
    return inst;
  }

  // Reset the instruction number, called when entering a new function
  auto ResetInstNum() -> void { inst_num_ = 0; }

  auto GetLiveRanges()
      -> const std::unordered_map<backend::targetir::TargetVirtualReg *, backend::targetir::LiveRange> & {
    return live_ranges_;
  }

  auto GetLiveRange(backend::targetir::TargetVirtualReg *vreg) -> backend::targetir::LiveRange & {
    return live_ranges_[vreg];
  }

  auto SetLiveRange(backend::targetir::TargetVirtualReg *vreg, const backend::targetir::LiveRange &live_range) -> void {
    live_ranges_[vreg] = live_range;
  }

  // Normally only used for updating the live range when adding a segment.
  auto UpdateLiveRange(backend::targetir::TargetVirtualReg *vreg, size_t start, size_t end) -> void {
    live_ranges_[vreg].AddSegment(start, end);
  }
  auto UpdateLiveRange(backend::targetir::TargetVirtualReg *vreg, const backend::targetir::Segment &segment) -> void {
    live_ranges_[vreg].AddSegment(segment);
  }

  auto UpdateLiveRangeUsage(backend::targetir::TargetVirtualReg *vreg, size_t inst_num) -> void {
    live_ranges_[vreg].AddUsedInst(inst_num);
  }

  auto GetPhysicalRegPool() -> backend::targetir::PhysicalRegPool & { return physical_reg_pool_; }
  auto GetRAManager() -> backend::targetir::RAManager & { return ra_manager_; }

  // Map from virtual register to physical register, used for register allocation and code generation
  auto MapVregToPreg(backend::targetir::TargetVirtualReg *vreg, backend::targetir::PhysicalReg *preg) -> void {
    vreg_to_preg_map_[vreg] = preg;
  }

  auto GetPregOfVreg(backend::targetir::TargetVirtualReg *vreg) -> backend::targetir::PhysicalReg * {
    auto it = vreg_to_preg_map_.find(vreg);
    if (it != vreg_to_preg_map_.end()) {
      return it->second;
    }
    return nullptr;
  }

  auto IsMapped(backend::targetir::TargetVirtualReg *vreg) -> bool { return vreg_to_preg_map_.count(vreg) > 0; }

  // Set and get hint for register allocation, which is used for optimization, e.g., coalescing.
  auto SetAllocatorHint(backend::targetir::TargetVirtualReg *vreg,
                        const backend::targetir::AllocatorHint &hint) -> void {
    allocator_hint_map_[vreg] = hint;
  }
  auto GetAllocatorHint(backend::targetir::TargetVirtualReg *vreg) -> backend::targetir::AllocatorHint & {
    return allocator_hint_map_[vreg];
  }
  auto HasAllocatorHint(backend::targetir::TargetVirtualReg *vreg) -> bool {
    return allocator_hint_map_.count(vreg) > 0;
  }

  // Set and get the lowering context
  auto GetCurrentFunction() -> backend::targetir::TargetFunction * { return current_function_; }
  auto SetCurrentFunction(backend::targetir::TargetFunction *function) -> void { current_function_ = function; }

  auto SetCurrentBlock(backend::targetir::TargetBlock *block) -> void { current_block_ = block; }
  auto GetCurrentBlock() -> backend::targetir::TargetBlock * { return current_block_; }

 private:
  /**
   * @brief Just provide the target-free backend context here,
   * the arichtecture-specific context will be provided in the corresponding lower pass.
   */
  std::vector<backend::targetir::TargetOperand *> operands_;

  // Map from mir register operand to target operand
  std::unordered_map<backend::utils::Operand *, backend::targetir::TargetOperand *> operand_map_;

  // Map from MIR block id to target block, used by branch and phi lowering.
  std::unordered_map<size_t, backend::targetir::TargetBlock *> block_map_;

  backend::targetir::TargetModule *module_;

  // Used as a counter for creating instructions, reset when entering a new function
  size_t inst_num_ = 0;

  // Live range information, mapped from VirtualReg
  std::unordered_map<backend::targetir::TargetVirtualReg *, backend::targetir::LiveRange> live_ranges_;

  // Record the current function
  backend::targetir::TargetFunction *current_function_;

  // Record the current block, which is used for lowering
  backend::targetir::TargetBlock *current_block_;

  // Information for register allocation, map from virtual register to physical register
  std::unordered_map<backend::targetir::TargetVirtualReg *, backend::targetir::PhysicalReg *> vreg_to_preg_map_;

  // Allocator hint for optimization
  std::unordered_map<backend::targetir::TargetVirtualReg *, backend::targetir::AllocatorHint> allocator_hint_map_;

  // Physical register pool
  backend::targetir::PhysicalRegPool physical_reg_pool_;

  backend::targetir::RAManager ra_manager_;
};

}  // namespace context

#endif
