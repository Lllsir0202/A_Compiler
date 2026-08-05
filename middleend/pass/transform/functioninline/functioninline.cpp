#include "middleend/pass/transform/functioninline/functioninline.h"
#include <deque>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include "context/compiler_context.h"
#include "middleend/mlir/dialect/base/binaryop.h"
#include "middleend/mlir/dialect/base/branch.h"
#include "middleend/mlir/dialect/base/cast.h"
#include "middleend/mlir/dialect/base/copy.h"
#include "middleend/mlir/dialect/base/funccall.h"
#include "middleend/mlir/dialect/base/funcdef.h"
#include "middleend/mlir/dialect/base/phi.h"
#include "middleend/mlir/dialect/base/unaryop.h"
#include "middleend/mlir/dialect/base/widemulmod.h"
#include "middleend/mlir/dialect/func/return.h"
#include "middleend/mlir/dialect/var/def.h"
#include "middleend/mlir/dialect/var/getptr.h"
#include "middleend/mlir/dialect/var/getvalue.h"
#include "middleend/mlir/dialect/var/set.h"
#include "middleend/mlir/utils/operation/operation.h"
#include "middleend/mlir/utils/struct/block.h"
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

auto FunctionInlinePass::Run() -> void {
  try {
    PreRun();
  } catch (const std::exception &e) {
    compiler_context_->Print("Error in FunctionInlinePass PreRun: " + std::string(e.what()));
    throw std::logic_error("PreRun failed in FunctionInlinePass");
  }

  bool changed = false;
  for (size_t round = 0; round < kMaxInlineSites; ++round) {
    CollectFunctions();
    CollectCallGraph();
    auto inline_sites = FindInlineSites();
    if (inline_sites.empty()) {
      break;
    }
    if (!InlineCallSite(inline_sites.front())) {
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
  compiler_context_->GetPassContext()->InvalidateAnalysis();
}

auto FunctionInlinePass::CollectFunctions() -> void {
  function_map_.clear();
  auto *module_op = GET_MLIR_CONTEXT()->GetModuleOp();
  if (module_op == nullptr) {
    throw std::logic_error("Module operation is not set in MLIRContext");
  }

  for (auto *operation : module_op->GetBlock()->operations_) {
    auto *func_op = mlir::DynamicOrNull<mlir::base::FuncDefOp>(operation);
    if (func_op == nullptr) {
      continue;
    }
    function_map_[func_op->GetName()] = func_op;
  }
}

auto FunctionInlinePass::CollectCallGraph() -> void {
  call_graph_.clear();
  for (const auto &[_, func_op] : function_map_) {
    auto &callees = call_graph_[func_op];
    for (auto *block : func_op->GetBlocks()) {
      for (auto *operation : block->operations_) {
        auto *call_op = mlir::DynamicOrNull<mlir::base::FuncCallOp>(operation);
        if (call_op == nullptr) {
          continue;
        }
        auto iter = function_map_.find(call_op->GetName());
        if (iter != function_map_.end()) {
          callees.insert(iter->second);
        }
      }
    }
  }
}

auto FunctionInlinePass::HasRecursivePath(mlir::base::FuncDefOp *from, mlir::base::FuncDefOp *target) -> bool {
  std::deque<mlir::base::FuncDefOp *> worklist;
  std::unordered_set<mlir::base::FuncDefOp *> visited;
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

auto FunctionInlinePass::CountOperations(mlir::base::FuncDefOp *func_op) -> size_t {
  size_t count = 0;
  for (auto *block : func_op->GetBlocks()) {
    count += block->operations_.size();
  }
  return count;
}

auto FunctionInlinePass::IsInlineCandidate(mlir::base::FuncDefOp *caller, mlir::base::FuncDefOp *callee) -> bool {
  if (caller == nullptr || callee == nullptr) {
    return false;
  }
  if (caller == callee) {
    return false;
  }
  if (IsLibraryFunctionName(callee->GetName())) {
    return false;
  }
  if (!HasUnifiedReturn(callee)) {
    return false;
  }
  for (auto *block : callee->GetBlocks()) {
    if (!block->arguments_.empty()) {
      return false;
    }
  }
  for (auto *argument : callee->GetArguments()) {
    // Pointer arguments need aggregate/pointee remapping that the BASEIR cloner
    // does not model yet.  Keep this inliner deliberately scalar-only.
    if (argument->GetType() == nullptr || argument->GetType()->GetKind() == type::TypeKind::PTR) {
      return false;
    }
  }
  if (CountOperations(callee) > kMaxInlineOperationCount) {
    return false;
  }
  auto callee_edges = call_graph_.find(callee);
  if (callee_edges != call_graph_.end()) {
    for (auto *next : callee_edges->second) {
      // Do not repeatedly expand a recursive callee into an otherwise
      // non-recursive caller.
      if (HasRecursivePath(next, callee)) {
        return false;
      }
    }
  }
  if (HasRecursivePath(callee, caller)) {
    return false;
  }
  return true;
}

auto FunctionInlinePass::HasUnifiedReturn(mlir::base::FuncDefOp *callee) -> bool {
  size_t returns = 0;
  for (auto *block : callee->GetBlocks()) {
    for (size_t index = 0; index < block->operations_.size(); ++index) {
      if (mlir::DynamicOrNull<mlir::func::ReturnOp>(block->operations_[index]) == nullptr) {
        continue;
      }
      if (index + 1 != block->operations_.size()) {
        return false;
      }
      ++returns;
    }
  }
  return returns == 1;
}

auto FunctionInlinePass::FindInlineSites() -> std::vector<InlineSite> {
  std::vector<InlineSite> sites;
  for (const auto &[_, caller] : function_map_) {
    for (auto *block : caller->GetBlocks()) {
      for (size_t index = 0; index < block->operations_.size(); ++index) {
        auto *call_op = mlir::DynamicOrNull<mlir::base::FuncCallOp>(block->operations_[index]);
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
  }
  return sites;
}

auto FunctionInlinePass::CreateInlineBlock(const std::string &prefix) -> mlir::Block * {
  auto *block = new mlir::Block();
  block->block_name_ = prefix;
  block->block_id_ = static_cast<int>(inline_block_id_counter_++);
  return block;
}

auto FunctionInlinePass::CloneValue(mlir::Value *original) -> mlir::Value * {
  if (original == nullptr) {
    return nullptr;
  }
  auto existing = value_map_.find(original);
  if (existing != value_map_.end()) {
    return existing->second;
  }
  auto *cloned = GET_MLIR_CONTEXT()->CreateValue();
  cloned->SetType(original->GetType());
  cloned->name_ = original->name_;
  value_map_[original] = cloned;
  return cloned;
}

auto FunctionInlinePass::RemapValue(mlir::Value *value) -> mlir::Value * {
  if (value == nullptr) {
    return nullptr;
  }
  auto iter = value_map_.find(value);
  if (iter != value_map_.end()) {
    return iter->second;
  }
  return value;
}

auto FunctionInlinePass::RemapBlock(mlir::Block *block) -> mlir::Block * {
  auto iter = block_map_.find(block);
  if (iter == block_map_.end()) {
    throw std::logic_error("Failed to remap inline block");
  }
  return iter->second;
}

auto FunctionInlinePass::CloneOperation(mlir::Operation *operation) -> mlir::Operation * {
  if (auto *phi_op = mlir::DynamicOrNull<mlir::base::PhiOp>(operation)) {
    auto *result = CloneValue(phi_op->GetRes());
    std::vector<mlir::Value *> values;
    std::vector<mlir::Block *> blocks;
    for (size_t i = 0; i < phi_op->GetIncomingSize(); ++i) {
      values.emplace_back(RemapValue(phi_op->GetIncomingValue()[i]));
      blocks.emplace_back(RemapBlock(phi_op->GetIncomingBlock()[i]));
    }
    return GET_MLIR_CONTEXT()->CreateOperation<mlir::base::PhiOp>(result, values, blocks);
  }
  if (auto *br_op = mlir::DynamicOrNull<mlir::base::BrOp>(operation)) {
    return GET_MLIR_CONTEXT()->CreateOperation<mlir::base::BrOp>(RemapBlock(br_op->GetTarget()));
  }
  if (auto *cond_br_op = mlir::DynamicOrNull<mlir::base::CondBrOp>(operation)) {
    return GET_MLIR_CONTEXT()->CreateOperation<mlir::base::CondBrOp>(RemapValue(cond_br_op->GetCondition()),
                                                                     RemapBlock(cond_br_op->GetTrueTarget()),
                                                                     RemapBlock(cond_br_op->GetFalseTarget()));
  }
  if (auto *copy_op = mlir::DynamicOrNull<mlir::base::CopyOp>(operation)) {
    return GET_MLIR_CONTEXT()->CreateOperation<mlir::base::CopyOp>(CloneValue(copy_op->GetRes()),
                                                                   RemapValue(copy_op->GetSrc()));
  }
  if (auto *call_op = mlir::DynamicOrNull<mlir::base::FuncCallOp>(operation)) {
    std::vector<mlir::Value *> operands;
    for (auto *operand : call_op->GetOperands()) {
      operands.emplace_back(RemapValue(operand));
    }
    return GET_MLIR_CONTEXT()->CreateOperation<mlir::base::FuncCallOp>(CloneValue(call_op->GetRes()),
                                                                       call_op->GetName(), operands);
  }
  if (auto *return_op = mlir::DynamicOrNull<mlir::func::ReturnOp>(operation)) {
    if (return_op->GetReturnValue() != nullptr) {
      return GET_MLIR_CONTEXT()->CreateOperation<mlir::func::ReturnOp>(
          GET_MLIR_CONTEXT(), return_op->GetRes()->GetType(), RemapValue(return_op->GetReturnValue()));
    }
    return GET_MLIR_CONTEXT()->CreateOperation<mlir::func::ReturnOp>(GET_MLIR_CONTEXT(),
                                                                     return_op->GetRes()->GetType());
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

#define CLONE_BASE_BINARY_OP(OP_TYPE)                                                                       \
  if (auto *binary_op = mlir::DynamicOrNull<mlir::base::OP_TYPE##Op>(operation)) {                          \
    return GET_MLIR_CONTEXT()->CreateOperation<mlir::base::OP_TYPE##Op>(                                    \
        CloneValue(binary_op->GetRes()), RemapValue(binary_op->GetLHS()), RemapValue(binary_op->GetRHS())); \
  }

  CLONE_BASE_BINARY_OP(Add)
  CLONE_BASE_BINARY_OP(Sub)
  CLONE_BASE_BINARY_OP(Mul)
  CLONE_BASE_BINARY_OP(Div)
  CLONE_BASE_BINARY_OP(Mod)
  CLONE_BASE_BINARY_OP(Shl)
  CLONE_BASE_BINARY_OP(LShr)
  CLONE_BASE_BINARY_OP(AShr)
  CLONE_BASE_BINARY_OP(Equal)
  CLONE_BASE_BINARY_OP(NotEqual)
  CLONE_BASE_BINARY_OP(Less)
  CLONE_BASE_BINARY_OP(LessEqual)
  CLONE_BASE_BINARY_OP(Greater)
  CLONE_BASE_BINARY_OP(GreaterEqual)
  CLONE_BASE_BINARY_OP(AddF)
  CLONE_BASE_BINARY_OP(SubF)
  CLONE_BASE_BINARY_OP(MulF)
  CLONE_BASE_BINARY_OP(DivF)
  CLONE_BASE_BINARY_OP(EqualF)
  CLONE_BASE_BINARY_OP(NotEqualF)
  CLONE_BASE_BINARY_OP(LessF)
  CLONE_BASE_BINARY_OP(LessEqualF)
  CLONE_BASE_BINARY_OP(GreaterF)
  CLONE_BASE_BINARY_OP(GreaterEqualF)

#undef CLONE_BASE_BINARY_OP

  if (auto *wide = mlir::DynamicOrNull<mlir::base::WideMulModOp>(operation)) {
    return GET_MLIR_CONTEXT()->CreateOperation<mlir::base::WideMulModOp>(
        CloneValue(wide->GetRes()), RemapValue(wide->GetLHS()), RemapValue(wide->GetRHS()),
        RemapValue(wide->GetModulus()));
  }

#define CLONE_BASE_UNARY_OP(OP_TYPE)                                                                         \
  if (auto *unary_op = mlir::DynamicOrNull<mlir::base::OP_TYPE##Op>(operation)) {                            \
    return GET_MLIR_CONTEXT()->CreateOperation<mlir::base::OP_TYPE##Op>(CloneValue(unary_op->GetRes()),      \
                                                                        RemapValue(unary_op->GetOperand())); \
  }

  CLONE_BASE_UNARY_OP(Neg)
  CLONE_BASE_UNARY_OP(NegF)
  CLONE_BASE_UNARY_OP(Not)

#undef CLONE_BASE_UNARY_OP

  if (auto *const_int_op = mlir::DynamicOrNull<mlir::base::ConstIntOp>(operation)) {
    return GET_MLIR_CONTEXT()->CreateOperation<mlir::base::ConstIntOp>(CloneValue(const_int_op->GetRes()),
                                                                       const_int_op->GetValue());
  }
  if (auto *const_float_op = mlir::DynamicOrNull<mlir::base::ConstFloatOp>(operation)) {
    return GET_MLIR_CONTEXT()->CreateOperation<mlir::base::ConstFloatOp>(CloneValue(const_float_op->GetRes()),
                                                                         const_float_op->GetValue());
  }

#define CLONE_CAST_OP(OP_TYPE)                                                                              \
  if (auto *cast_op = mlir::DynamicOrNull<mlir::base::OP_TYPE##Op>(operation)) {                            \
    return GET_MLIR_CONTEXT()->CreateOperation<mlir::base::OP_TYPE##Op>(CloneValue(cast_op->GetRes()),      \
                                                                        RemapValue(cast_op->GetOperand())); \
  }

  CLONE_CAST_OP(Int2Float)
  CLONE_CAST_OP(Float2Int)
  CLONE_CAST_OP(Bool2Int)
  CLONE_CAST_OP(Int2Bool)
  CLONE_CAST_OP(Bool2Float)
  CLONE_CAST_OP(Float2Bool)

#undef CLONE_CAST_OP

  throw std::logic_error("Unsupported operation in FunctionInlinePass clone: " + operation->Name());
}

auto FunctionInlinePass::InlineCallSite(const InlineSite &site) -> bool {
  if (site.call_index >= site.call_block->operations_.size() ||
      site.call_block->operations_[site.call_index] != site.call_op) {
    return false;
  }

  value_map_.clear();
  block_map_.clear();

  auto *call_block = site.call_block;
  auto *call_op = site.call_op;
  auto *callee = site.callee;

  auto *call_result = call_op->GetRes();
  for (size_t i = 0; i < callee->GetArguments().size(); ++i) {
    value_map_[callee->GetArguments()[i]] = call_op->GetOperands()[i];
  }

  auto callee_blocks = callee->GetBlocks();
  for (auto *callee_block : callee_blocks) {
    block_map_[callee_block] = CreateInlineBlock(callee->GetName() + "_inline");
    for (auto *operation : callee_block->operations_) {
      for (auto *result : operation->results_) {
        CloneValue(result);
      }
    }
  }

  auto *continuation = CreateInlineBlock(site.caller->GetName() + "_inline_cont");
  for (size_t index = site.call_index + 1; index < call_block->operations_.size(); ++index) {
    continuation->operations_.emplace_back(call_block->operations_[index]);
  }

  auto caller_blocks = site.caller->GetBlocks();
  for (auto *block : caller_blocks) {
    for (auto *operation : block->operations_) {
      auto *phi = mlir::DynamicOrNull<mlir::base::PhiOp>(operation);
      if (phi == nullptr) {
        continue;
      }
      for (size_t index = 0; index < phi->GetIncomingSize(); ++index) {
        if (phi->GetIncomingBlock()[index] == call_block) {
          phi->UpdateIncoming(index, phi->GetIncomingValue()[index], continuation);
        }
      }
    }
  }

  call_block->operations_.resize(site.call_index);
  call_block->operations_.emplace_back(
      GET_MLIR_CONTEXT()->CreateOperation<mlir::base::BrOp>(block_map_.at(callee->GetEntryBlock())));

  mlir::Value *inlined_return_value = nullptr;
  for (auto *callee_block : callee_blocks) {
    auto *cloned_block = block_map_.at(callee_block);
    for (auto *operation : callee_block->operations_) {
      if (auto *return_op = mlir::DynamicOrNull<mlir::func::ReturnOp>(operation)) {
        inlined_return_value = RemapValue(return_op->GetReturnValue());
        cloned_block->operations_.emplace_back(GET_MLIR_CONTEXT()->CreateOperation<mlir::base::BrOp>(continuation));
      } else {
        cloned_block->operations_.emplace_back(CloneOperation(operation));
      }
    }
  }

  if (call_result != nullptr && inlined_return_value != nullptr) {
    call_result->ReplaceAllUsesWith(inlined_return_value);
  }

  std::vector<mlir::Block *> new_blocks;
  new_blocks.reserve(caller_blocks.size() + callee_blocks.size());
  for (auto *block : caller_blocks) {
    new_blocks.emplace_back(block);
    if (block != call_block) {
      continue;
    }
    for (auto *callee_block : callee_blocks) {
      new_blocks.emplace_back(block_map_.at(callee_block));
    }
    new_blocks.emplace_back(continuation);
  }
  site.caller->SetBlocks(std::move(new_blocks));
  return true;
}

auto FunctionInlinePass::RebuildPhiUses(mlir::Operation *operation) -> void {
  auto *phi_op = mlir::DynamicOrNull<mlir::base::PhiOp>(operation);
  if (phi_op == nullptr) {
    return;
  }
  for (size_t i = 0; i < phi_op->GetIncomingSize(); ++i) {
    auto *value = phi_op->GetIncomingValue()[i];
    if (value != nullptr) {
      value->AddUse(operation, i);
    }
  }
}

auto FunctionInlinePass::RebuildOperationUses(mlir::Operation *operation) -> void {
  for (size_t i = 0; i < operation->operands_.size(); ++i) {
    auto *operand = operation->operands_[i];
    if (operand != nullptr) {
      operand->AddUse(operation, i);
    }
  }
  RebuildPhiUses(operation);
}

auto FunctionInlinePass::RebuildUses(mlir::base::FuncDefOp *func_op) -> void {
  for (auto *argument : func_op->GetArguments()) {
    if (argument != nullptr) {
      argument->ClearUses();
    }
  }
  for (auto *block : func_op->GetBlocks()) {
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
    }
  }

  for (auto *block : func_op->GetBlocks()) {
    for (auto *operation : block->operations_) {
      RebuildOperationUses(operation);
    }
  }
}

}  // namespace middleend::pass::transform
