#include "middleend/pass/transform/mlirfunctioninline/mlirfunctioninline.h"
#include <deque>
#include <string>
#include <utility>
#include "context/compiler_context.h"
#include "middleend/mlir/dialect/arithm/binaryop.h"
#include "middleend/mlir/dialect/arithm/copy.h"
#include "middleend/mlir/dialect/arithm/unaryop.h"
#include "middleend/mlir/dialect/func/funccall.h"
#include "middleend/mlir/dialect/func/funcdef.h"
#include "middleend/mlir/dialect/func/return.h"
#include "middleend/mlir/dialect/scf/break.h"
#include "middleend/mlir/dialect/scf/condition.h"
#include "middleend/mlir/dialect/scf/continue.h"
#include "middleend/mlir/dialect/scf/if.h"
#include "middleend/mlir/dialect/scf/while.h"
#include "middleend/mlir/dialect/scf/yield.h"
#include "middleend/mlir/dialect/var/def.h"
#include "middleend/mlir/dialect/var/getptr.h"
#include "middleend/mlir/dialect/var/getvalue.h"
#include "middleend/mlir/dialect/var/set.h"
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
    RebuildUses(func_op);
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
        if (IsInlineCandidate(caller, callee) && call_op->GetOperands().size() == callee->GetArguments().size()) {
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
  return func_op->GetEntryBlock()->operations_.size();
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
    return false;
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
      if (call_op->GetOperands().size() != callee->GetArguments().size()) {
        continue;
      }
      sites.push_back({caller, block, index, call_op, callee});
    }
  }
  return sites;
}

auto MLIRFunctionInlinePass::CloneResult(mlir::Value *original) -> mlir::Value * {
  if (original == nullptr) {
    return nullptr;
  }
  auto *result = GET_MLIR_CONTEXT()->CreateValue();
  result->SetType(original->GetType());
  result->name_ = original->name_;
  value_map_[original] = result;
  return result;
}

auto MLIRFunctionInlinePass::RemapValue(mlir::Value *value) -> mlir::Value * {
  if (value == nullptr) {
    return nullptr;
  }
  auto iter = value_map_.find(value);
  if (iter != value_map_.end()) {
    return iter->second;
  }
  return value;
}

auto MLIRFunctionInlinePass::CloneBlock(mlir::Block *block) -> mlir::Block * {
  auto *new_block = new mlir::Block();
  new_block->block_name_ = block->block_name_;
  new_block->block_id_ = block->block_id_;
  for (auto *operation : block->operations_) {
    new_block->operations_.emplace_back(CloneOperation(operation));
  }
  return new_block;
}

auto MLIRFunctionInlinePass::CloneRegion(mlir::Region *region) -> mlir::Region * {
  auto *new_region = new mlir::Region();
  for (auto *block : region->blocks_) {
    new_region->blocks_.emplace_back(CloneBlock(block));
  }
  return new_region;
}

auto MLIRFunctionInlinePass::CloneOperation(mlir::Operation *operation) -> mlir::Operation * {
  if (auto *call_op = mlir::DynamicOrNull<mlir::func::FuncCallOp>(operation)) {
    std::vector<mlir::Value *> operands;
    for (auto *operand : call_op->GetOperands()) {
      operands.emplace_back(RemapValue(operand));
    }
    auto *new_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::func::FuncCallOp>(GET_MLIR_CONTEXT(), call_op->GetName(),
                                                                               operands, call_op->GetRes()->GetType());
    value_map_[call_op->GetRes()] = new_op->GetRes();
    return new_op;
  }
  if (auto *return_op = mlir::DynamicOrNull<mlir::func::ReturnOp>(operation)) {
    if (return_op->GetReturnValue() != nullptr) {
      return GET_MLIR_CONTEXT()->CreateOperation<mlir::func::ReturnOp>(
          GET_MLIR_CONTEXT(), return_op->GetRes()->GetType(), RemapValue(return_op->GetReturnValue()));
    }
    return GET_MLIR_CONTEXT()->CreateOperation<mlir::func::ReturnOp>(GET_MLIR_CONTEXT(),
                                                                     return_op->GetRes()->GetType());
  }
  if (auto *copy_op = mlir::DynamicOrNull<mlir::arithm::CopyOp>(operation)) {
    auto *new_op =
        GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::CopyOp>(GET_MLIR_CONTEXT(), RemapValue(copy_op->GetValue()));
    new_op->GetRes()->SetType(copy_op->GetRes()->GetType());
    value_map_[copy_op->GetRes()] = new_op->GetRes();
    return new_op;
  }
  if (auto *const_op = mlir::DynamicOrNull<mlir::arithm::ConstOp>(operation)) {
    mlir::arithm::ConstOp *new_op = nullptr;
    auto value = const_op->GetValue();
    if (std::holds_alternative<int>(value)) {
      new_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::ConstOp>(GET_MLIR_CONTEXT(), std::get<int>(value));
    } else {
      new_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::ConstOp>(GET_MLIR_CONTEXT(), std::get<float>(value));
    }
    new_op->GetRes()->SetType(const_op->GetRes()->GetType());
    value_map_[const_op->GetRes()] = new_op->GetRes();
    return new_op;
  }
  if (auto *cast_op = mlir::DynamicOrNull<mlir::arithm::CastOp>(operation)) {
    auto *new_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::CastOp>(
        GET_MLIR_CONTEXT(), RemapValue(cast_op->GetOperand()), cast_op->GetFromType(), cast_op->GetToType());
    value_map_[cast_op->GetRes()] = new_op->GetRes();
    return new_op;
  }
  if (auto *var_def_op = mlir::DynamicOrNull<mlir::var::VarDefOp>(operation)) {
    std::vector<mlir::Value *> inits;
    for (auto *init : var_def_op->GetInits()) {
      inits.emplace_back(RemapValue(init));
    }
    auto *new_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::var::VarDefOp>(
        GET_MLIR_CONTEXT(), std::vector<int>(var_def_op->GetDimensions().begin(), var_def_op->GetDimensions().end()),
        inits);
    new_op->GetRes()->SetType(var_def_op->GetRes()->GetType());
    value_map_[var_def_op->GetRes()] = new_op->GetRes();
    return new_op;
  }
  if (auto *get_ptr_op = mlir::DynamicOrNull<mlir::var::VarGetPtrOp>(operation)) {
    std::vector<mlir::Value *> indices;
    for (auto *index : get_ptr_op->GetIndices()) {
      indices.emplace_back(RemapValue(index));
    }
    auto *new_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::var::VarGetPtrOp>(
        GET_MLIR_CONTEXT(), RemapValue(get_ptr_op->GetPtr()), indices);
    new_op->GetRes()->SetType(get_ptr_op->GetRes()->GetType());
    value_map_[get_ptr_op->GetRes()] = new_op->GetRes();
    return new_op;
  }
  if (auto *get_val_op = mlir::DynamicOrNull<mlir::var::VarGetValOp>(operation)) {
    auto *new_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::var::VarGetValOp>(GET_MLIR_CONTEXT(),
                                                                               RemapValue(get_val_op->GetPtr()));
    new_op->GetRes()->SetType(get_val_op->GetRes()->GetType());
    value_map_[get_val_op->GetRes()] = new_op->GetRes();
    return new_op;
  }
  if (auto *set_op = mlir::DynamicOrNull<mlir::var::VarSetOp>(operation)) {
    auto *new_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::var::VarSetOp>(
        GET_MLIR_CONTEXT(), RemapValue(set_op->GetPtr()), RemapValue(set_op->GetNewValue()));
    new_op->GetRes()->SetType(set_op->GetRes()->GetType());
    value_map_[set_op->GetRes()] = new_op->GetRes();
    return new_op;
  }
  if (auto *yield_op = mlir::DynamicOrNull<mlir::scf::YieldOp>(operation)) {
    std::vector<mlir::Value *> operands;
    for (auto *operand : yield_op->GetOperands()) {
      operands.emplace_back(RemapValue(operand));
    }
    return GET_MLIR_CONTEXT()->CreateOperation<mlir::scf::YieldOp>(GET_MLIR_CONTEXT(), operands);
  }
  if (auto *if_op = mlir::DynamicOrNull<mlir::scf::IfOp>(operation)) {
    auto *new_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::scf::IfOp>(
        GET_MLIR_CONTEXT(), if_op->results_.size(), RemapValue(if_op->GetCondition()),
        CloneRegion(if_op->GetThenRegion()), CloneRegion(if_op->GetElseRegion()));
    for (size_t i = 0; i < if_op->results_.size(); ++i) {
      new_op->GetResultByIndex(i)->SetType(if_op->GetResultByIndex(i)->GetType());
      value_map_[if_op->GetResultByIndex(i)] = new_op->GetResultByIndex(i);
    }
    return new_op;
  }
  if (auto *while_op = mlir::DynamicOrNull<mlir::scf::WhileOp>(operation)) {
    // The scf.while cond and body blocks share the SAME loop-carried block-argument
    // Value objects. Clone each shared argument exactly once and register it in the
    // value map BEFORE cloning the regions, so references inside both regions remap to
    // the cloned argument; then attach the same cloned arguments to both cloned blocks
    // to preserve the shared-argument invariant.
    auto *cond_block = while_op->GetCondRegion()->blocks_[0];
    std::vector<mlir::Value *> cloned_args;
    cloned_args.reserve(cond_block->arguments_.size());
    for (auto *arg : cond_block->arguments_) {
      cloned_args.emplace_back(CloneResult(arg));
    }
    auto *new_cond_region = CloneRegion(while_op->GetCondRegion());
    auto *new_body_region = CloneRegion(while_op->GetBodyRegion());
    for (auto *cloned_arg : cloned_args) {
      new_cond_region->blocks_[0]->AddArgument(cloned_arg);
      new_body_region->blocks_[0]->AddArgument(cloned_arg);
    }
    auto *new_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::scf::WhileOp>(
        GET_MLIR_CONTEXT(), while_op->GetResultsSize(), new_cond_region, new_body_region);
    for (size_t i = 0; i < while_op->GetResultsSize(); ++i) {
      new_op->GetResultByIndex(i)->SetType(while_op->GetResultByIndex(i)->GetType());
      value_map_[while_op->GetResultByIndex(i)] = new_op->GetResultByIndex(i);
    }
    return new_op;
  }

#define CLONE_BINARY_OP(OP_TYPE)                                                               \
  if (auto *binary_op = mlir::DynamicOrNull<mlir::arithm::OP_TYPE##Op>(operation)) {           \
    auto *new_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::OP_TYPE##Op>(             \
        GET_MLIR_CONTEXT(), RemapValue(binary_op->GetLHS()), RemapValue(binary_op->GetRHS())); \
    new_op->GetRes()->SetType(binary_op->GetRes()->GetType());                                 \
    value_map_[binary_op->GetRes()] = new_op->GetRes();                                        \
    return new_op;                                                                             \
  }

  CLONE_BINARY_OP(Add)
  CLONE_BINARY_OP(Sub)
  CLONE_BINARY_OP(Mul)
  CLONE_BINARY_OP(Div)
  CLONE_BINARY_OP(Mod)
  CLONE_BINARY_OP(Equal)
  CLONE_BINARY_OP(NotEqual)
  CLONE_BINARY_OP(Less)
  CLONE_BINARY_OP(LessEqual)
  CLONE_BINARY_OP(Greater)
  CLONE_BINARY_OP(GreaterEqual)

#undef CLONE_BINARY_OP

#define CLONE_UNARY_OP(OP_TYPE)                                                                                        \
  if (auto *unary_op = mlir::DynamicOrNull<mlir::arithm::OP_TYPE##Op>(operation)) {                                    \
    auto *new_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::OP_TYPE##Op>(GET_MLIR_CONTEXT(),                  \
                                                                                  RemapValue(unary_op->GetOperand())); \
    new_op->GetRes()->SetType(unary_op->GetRes()->GetType());                                                          \
    value_map_[unary_op->GetRes()] = new_op->GetRes();                                                                 \
    return new_op;                                                                                                     \
  }

  CLONE_UNARY_OP(Neg)
  CLONE_UNARY_OP(Not)

#undef CLONE_UNARY_OP

  throw std::logic_error("Unsupported operation in MLIRFunctionInlinePass clone: " + operation->Name());
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
  RebuildUses(site.caller);

  value_map_.clear();
  for (size_t i = 0; i < site.callee->GetArguments().size(); ++i) {
    value_map_[site.callee->GetArguments()[i]] = site.call_op->GetOperands()[i];
  }

  std::vector<mlir::Operation *> cloned_operations;
  mlir::Value *return_value = nullptr;
  for (auto *operation : site.callee->GetEntryBlock()->operations_) {
    auto *return_op = mlir::DynamicOrNull<mlir::func::ReturnOp>(operation);
    if (return_op != nullptr) {
      return_value = RemapValue(return_op->GetReturnValue());
      continue;
    }
    cloned_operations.emplace_back(CloneOperation(operation));
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

auto MLIRFunctionInlinePass::RebuildOperationUses(mlir::Operation *operation) -> void {
  for (size_t i = 0; i < operation->operands_.size(); ++i) {
    auto *operand = operation->operands_[i];
    if (operand != nullptr) {
      operand->AddUse(operation, i);
    }
  }
  for (auto *region : operation->regions_) {
    RebuildUsesInRegion(region);
  }
}

// Clearing must be separated from rebuilding across the whole function: a
// while-loop's cond and body blocks share the same loop-carried block
// argument value, so clearing per-region after a sibling region was already
// rebuilt would wipe the uses that sibling registered.
auto MLIRFunctionInlinePass::ClearUsesInRegion(mlir::Region *region) -> void {
  if (region == nullptr) {
    return;
  }
  for (auto *block : region->blocks_) {
    for (auto *argument : block->arguments_) {
      if (argument != nullptr) {
        argument->ClearUses();
      }
    }
    for (auto *operation : block->operations_) {
      for (auto *result : operation->results_) {
        if (result != nullptr) {
          result->ClearUses();
        }
      }
      for (auto *nested_region : operation->regions_) {
        ClearUsesInRegion(nested_region);
      }
    }
  }
}

auto MLIRFunctionInlinePass::RebuildUsesInRegion(mlir::Region *region) -> void {
  if (region == nullptr) {
    return;
  }
  for (auto *block : region->blocks_) {
    for (auto *operation : block->operations_) {
      RebuildOperationUses(operation);
    }
  }
}

auto MLIRFunctionInlinePass::RebuildUses(mlir::func::FuncDefOp *func_op) -> void {
  // Phase 1: clear every use in the function before rebuilding any.
  for (auto *argument : func_op->GetArguments()) {
    if (argument != nullptr) {
      argument->ClearUses();
    }
  }
  for (auto *result : func_op->results_) {
    if (result != nullptr) {
      result->ClearUses();
    }
  }
  for (auto *region : func_op->regions_) {
    ClearUsesInRegion(region);
  }
  // Phase 2: rebuild operand uses now that all use-lists start empty.
  for (auto *region : func_op->regions_) {
    RebuildUsesInRegion(region);
  }
}

}  // namespace middleend::pass::transform
