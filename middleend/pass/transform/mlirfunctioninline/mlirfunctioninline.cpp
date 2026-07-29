#include "middleend/pass/transform/mlirfunctioninline/mlirfunctioninline.h"
#include <deque>
#include <string>
#include <utility>
#include "context/compiler_context.h"
#include "middleend/mlir/dialect/func/funccall.h"
#include "middleend/mlir/dialect/func/funcdef.h"
#include "middleend/mlir/dialect/func/return.h"
#include "middleend/mlir/dialect/scf/break.h"
#include "middleend/mlir/dialect/scf/condition.h"
#include "middleend/mlir/dialect/scf/continue.h"
#include "middleend/mlir/dialect/scf/if.h"
#include "middleend/mlir/dialect/scf/while.h"
#include "middleend/mlir/utils/clone/uselistrebuilder.h"
#include "middleend/mlir/utils/operation/operation.h"
#include "middleend/mlir/utils/struct/block.h"
#include "middleend/mlir/utils/struct/region.h"
#include "middleend/mlir/utils/value/value.h"

#define GET_MLIR_CONTEXT() compiler_context_->GetIRContext()->GetMLIRContext()

namespace middleend::pass::transform {
namespace {
auto IsLibraryFunctionName(const std::string &name) -> bool {
  return name == "getint" || name == "getch" || name == "getfloat" || name == "getarray" || name == "getfarray" ||
         name == "putint" || name == "putch" || name == "putfloat" || name == "putarray" || name == "putfarray" ||
         name == "_sysy_starttime" || name == "_sysy_stoptime";
}
}  // namespace

auto MLIRFunctionInlinePass::Run() -> void {
  bool changed = false;
  for (size_t round = 0; round < kMaxInlineRounds; ++round) {
    CollectFunctions();
    CollectCallGraph();
    if (!RunInlineRound()) {
      break;
    }
    changed = true;
  }

  if (!changed) {
    return;
  }

  for (const auto &[_, func_op] : function_map_) {
    mlir::utils::RebuildFunctionUses(func_op);
  }
}

auto MLIRFunctionInlinePass::RunInlineRound() -> bool {
  bool inlined_any = false;
  for (const auto &[_, caller] : function_map_) {
    if (IsLibraryFunctionName(caller->GetName())) {
      continue;
    }
    // Inline every eligible call site in this caller before moving on, restarting
    // the scan after each inline: InlineCallSite rebuilds the caller's operation
    // list, and a freshly inlined body can expose further call sites that should
    // also be inlined in this same pass. Termination is guaranteed because
    // IsInlineCandidate only accepts acyclic (non-recursive, non-back-edge)
    // callees, so the transitive expansion is finite.
    while (TryInlineInBlock(caller, caller->GetEntryBlock())) {
      inlined_any = true;
    }
  }
  return inlined_any;
}

auto MLIRFunctionInlinePass::TryInlineInBlock(mlir::func::FuncDefOp *caller, mlir::Block *block) -> bool {
  for (size_t index = 0; index < block->operations_.size(); ++index) {
    auto *operation = block->operations_[index];
    auto *call_op = mlir::DynamicOrNull<mlir::func::FuncCallOp>(operation);
    if (call_op != nullptr) {
      auto iter = function_map_.find(call_op->GetName());
      if (iter != function_map_.end()) {
        auto *callee = iter->second;
        if (IsInlineCandidate(caller, callee) && HaveCompatibleArgumentTypes(call_op, callee)) {
          return InlineCallSite({caller, block, index, call_op, callee});
        }
      }
    }
    for (auto *region : operation->regions_) {
      if (region == nullptr) {
        continue;
      }
      for (auto *nested_block : region->blocks_) {
        if (TryInlineInBlock(caller, nested_block)) {
          return true;
        }
      }
    }
  }
  return false;
}

auto MLIRFunctionInlinePass::CollectFunctions() -> void {
  function_map_.clear();
  auto *module_op = GET_MLIR_CONTEXT()->GetModuleOp();
  if (module_op == nullptr) {
    throw std::logic_error("Module operation is not set in MLIRContext");
  }

  for (auto *operation : module_op->GetBlock()->operations_) {
    auto *func_op = mlir::DynamicOrNull<mlir::func::FuncDefOp>(operation);
    if (func_op == nullptr) {
      continue;
    }
    function_map_[func_op->GetName()] = func_op;
  }
}

auto MLIRFunctionInlinePass::CollectCallsInBlock(mlir::Block *block,
                                                 std::unordered_set<mlir::func::FuncDefOp *> &callees) -> void {
  for (auto *operation : block->operations_) {
    if (auto *call_op = mlir::DynamicOrNull<mlir::func::FuncCallOp>(operation)) {
      auto iter = function_map_.find(call_op->GetName());
      if (iter != function_map_.end()) {
        callees.insert(iter->second);
      }
    }
    for (auto *region : operation->regions_) {
      if (region == nullptr) {
        continue;
      }
      for (auto *nested_block : region->blocks_) {
        CollectCallsInBlock(nested_block, callees);
      }
    }
  }
}

auto MLIRFunctionInlinePass::CollectCallGraph() -> void {
  call_graph_.clear();
  for (const auto &[_, func_op] : function_map_) {
    auto &callees = call_graph_[func_op];
    CollectCallsInBlock(func_op->GetEntryBlock(), callees);
  }
}

auto MLIRFunctionInlinePass::IsSelfRecursive(mlir::func::FuncDefOp *func_op) -> bool {
  auto it = call_graph_.find(func_op);
  if (it == call_graph_.end()) {
    return false;
  }
  for (auto *direct_callee : it->second) {
    if (HasRecursivePath(direct_callee, func_op)) {
      return true;
    }
  }
  return false;
}

auto MLIRFunctionInlinePass::HasRecursivePath(mlir::func::FuncDefOp *from, mlir::func::FuncDefOp *target) -> bool {
  std::deque<mlir::func::FuncDefOp *> worklist;
  std::unordered_set<mlir::func::FuncDefOp *> visited;
  worklist.emplace_back(from);
  visited.insert(from);

  while (!worklist.empty()) {
    auto *current = worklist.front();
    worklist.pop_front();
    if (current == target) {
      return true;
    }
    auto iter = call_graph_.find(current);
    if (iter == call_graph_.end()) {
      continue;
    }
    for (auto *next : iter->second) {
      if (visited.count(next) > 0) {
        continue;
      }
      visited.insert(next);
      worklist.emplace_back(next);
    }
  }
  return false;
}

auto MLIRFunctionInlinePass::CountOperations(mlir::func::FuncDefOp *func_op) -> size_t {
  size_t count = 0;
  for (auto *operation : func_op->GetEntryBlock()->operations_) {
    count += CountOperations(operation);
  }
  return count;
}

auto MLIRFunctionInlinePass::CountOperations(mlir::Operation *operation) -> size_t {
  size_t count = 1;
  for (auto *region : operation->regions_) {
    count += CountOperations(region);
  }
  return count;
}

auto MLIRFunctionInlinePass::CountOperations(mlir::Region *region) -> size_t {
  if (region == nullptr) {
    return 0;
  }
  size_t count = 0;
  for (auto *block : region->blocks_) {
    count += block->operations_.size();
    for (auto *operation : block->operations_) {
      for (auto *nested_region : operation->regions_) {
        count += CountOperations(nested_region);
      }
    }
  }
  return count;
}

auto MLIRFunctionInlinePass::CanCloneRegion(mlir::Region *region) -> bool {
  if (region == nullptr || region->blocks_.size() != 1) {
    return false;
  }
  for (auto *operation : region->blocks_[0]->operations_) {
    if (mlir::DynamicOrNull<mlir::func::ReturnOp>(operation) != nullptr) {
      return false;
    }
    if (!CanCloneOperation(operation)) {
      return false;
    }
  }
  return true;
}

auto MLIRFunctionInlinePass::CanCloneOperation(mlir::Operation *operation) -> bool {
  if (mlir::DynamicOrNull<mlir::func::FuncDefOp>(operation) != nullptr) {
    return false;
  }
  if (auto *call_op = mlir::DynamicOrNull<mlir::func::FuncCallOp>(operation)) {
    return !IsLibraryFunctionName(call_op->GetName());
  }
  if (mlir::DynamicOrNull<mlir::scf::BreakOp>(operation) != nullptr ||
      mlir::DynamicOrNull<mlir::scf::ContinueOp>(operation) != nullptr ||
      mlir::DynamicOrNull<mlir::scf::ConditionOp>(operation) != nullptr) {
    return true;
  }
  if (auto *if_op = mlir::DynamicOrNull<mlir::scf::IfOp>(operation)) {
    return CanCloneRegion(if_op->GetThenRegion()) && CanCloneRegion(if_op->GetElseRegion());
  }
  if (auto *while_op = mlir::DynamicOrNull<mlir::scf::WhileOp>(operation)) {
    return CanCloneRegion(while_op->GetCondRegion()) && CanCloneRegion(while_op->GetBodyRegion());
  }
  return true;
}

auto MLIRFunctionInlinePass::IsInlineCandidate(mlir::func::FuncDefOp *caller, mlir::func::FuncDefOp *callee) -> bool {
  if (caller == nullptr || callee == nullptr) {
    return false;
  }
  if (caller == callee) {
    return false;
  }
  if (IsLibraryFunctionName(callee->GetName())) {
    return false;
  }
  if (callee->GetEntryBlock()->operations_.empty()) {
    return false;
  }
  if (CountOperations(callee) > kMaxInlineOperationCount) {
    return false;
  }
  if (HasRecursivePath(callee, caller)) {
    return false;
  }
  if (IsSelfRecursive(callee)) {
    return false;
  }
  for (auto *operation : callee->GetEntryBlock()->operations_) {
    if (!CanCloneOperation(operation)) {
      return false;
    }
  }
  return true;
}

auto MLIRFunctionInlinePass::HaveCompatibleArgumentTypes(mlir::func::FuncCallOp *call_op,
                                                         mlir::func::FuncDefOp *callee) -> bool {
  if (call_op == nullptr || callee == nullptr ||
      call_op->GetOperands().size() != callee->GetArguments().size()) {
    return false;
  }

  for (size_t i = 0; i < call_op->GetOperands().size(); ++i) {
    auto *actual_type = call_op->GetOperands()[i]->GetType();
    auto *formal_type = callee->GetArguments()[i]->GetType();
    if (actual_type == nullptr || formal_type == nullptr) {
      return false;
    }

    // A function call passes only the pointer value, so the callee can give an
    // array slice a more precise pointee type than the caller-side expression.
    // Inlining, however, substitutes the SSA value directly.  VarGetPtr lowering
    // derives GEP strides from that value's pointee type; substituting a
    // differently-shaped pointer therefore changes every nested-array offset.
    // Keep such calls out of the inliner until the IR has an explicit pointer
    // bitcast/type-refinement operation.  TypeContext interns identical pointer
    // layouts, so pointer identity is the exact compatibility test here.
    if (actual_type->GetKind() == type::TypeKind::PTR ||
        formal_type->GetKind() == type::TypeKind::PTR) {
      if (actual_type != formal_type) {
        return false;
      }
      continue;
    }
    if (actual_type->GetKind() != formal_type->GetKind()) {
      return false;
    }
  }
  return true;
}

auto MLIRFunctionInlinePass::FindInlineSites() -> std::vector<InlineSite> {
  std::vector<InlineSite> sites;
  for (const auto &[_, caller] : function_map_) {
    auto *block = caller->GetEntryBlock();
    for (size_t index = 0; index < block->operations_.size(); ++index) {
      auto *call_op = mlir::DynamicOrNull<mlir::func::FuncCallOp>(block->operations_[index]);
      if (call_op == nullptr) {
        continue;
      }
      auto iter = function_map_.find(call_op->GetName());
      if (iter == function_map_.end()) {
        continue;
      }
      auto *callee = iter->second;
      if (!IsInlineCandidate(caller, callee)) {
        continue;
      }
      if (!HaveCompatibleArgumentTypes(call_op, callee)) {
        continue;
      }
      sites.push_back({caller, block, index, call_op, callee});
    }
  }
  return sites;
}

auto MLIRFunctionInlinePass::InlineCallSite(const InlineSite &site) -> bool {
  if (site.call_index >= site.block->operations_.size() || site.block->operations_[site.call_index] != site.call_op) {
    return false;
  }

  // ReplaceAllUsesWith below relies on the call result's use-list being current.
  // Cloning done by previous inline rounds does not register uses on the cloned
  // operands, so rebuild the caller's use-lists before substituting the return
  // value; otherwise the substitution silently misses uses and leaves dangling
  // operands referencing the removed call.
  mlir::utils::RebuildFunctionUses(site.caller);

  cloner_.value_map_.clear();
  for (size_t i = 0; i < site.callee->GetArguments().size(); ++i) {
    cloner_.value_map_[site.callee->GetArguments()[i]] = site.call_op->GetOperands()[i];
  }

  std::vector<mlir::Operation *> cloned_operations;
  mlir::Value *return_value = nullptr;
  for (auto *operation : site.callee->GetEntryBlock()->operations_) {
    auto *return_op = mlir::DynamicOrNull<mlir::func::ReturnOp>(operation);
    if (return_op != nullptr) {
      return_value = cloner_.RemapValue(return_op->GetReturnValue());
      continue;
    }
    cloned_operations.emplace_back(cloner_.Clone(operation));
  }

  if (return_value != nullptr) {
    site.call_op->GetRes()->ReplaceAllUsesWith(return_value);
  }

  std::vector<mlir::Operation *> new_operations;
  new_operations.reserve(site.block->operations_.size() - 1 + cloned_operations.size());
  for (size_t i = 0; i < site.call_index; ++i) {
    new_operations.emplace_back(site.block->operations_[i]);
  }
  for (auto *operation : cloned_operations) {
    new_operations.emplace_back(operation);
  }
  for (size_t i = site.call_index + 1; i < site.block->operations_.size(); ++i) {
    new_operations.emplace_back(site.block->operations_[i]);
  }
  site.block->operations_ = std::move(new_operations);
  return true;
}

}  // namespace middleend::pass::transform
